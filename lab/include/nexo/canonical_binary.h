#pragma once

#include <array>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <utility>

namespace ps2native::nexo::binary
{
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
inline uint32_t checksum(std::span<const uint8_t> bytes)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (i >= 20 && i < 24) continue;
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
inline void put32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
{ for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = uint8_t(value >> (8 * i)); }
inline uint32_t get32(std::span<const uint8_t> bytes, size_t offset)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= uint32_t(bytes[offset + i]) << (8 * i);
    return value;
}
class Writer
{
    std::vector<uint8_t> data;
    size_t maximum;
    void room(size_t count)
    { if (count > maximum - data.size()) throw std::invalid_argument("canonical state exceeds its byte budget"); }
public:
    static constexpr bool reading = false;
    Writer(std::array<uint8_t, 8> magic, uint32_t variant, size_t bound) : maximum(bound)
    {
        if (bound < 24 || bound > UINT32_MAX) throw std::invalid_argument("invalid canonical byte budget");
        data.assign(magic.begin(), magic.end()); data.resize(24);
        put32(data, 8, 1); put32(data, 12, variant);
    }
    template <typename T> void operator()(const T &value)
    {
        if constexpr (std::is_same_v<T, bool>) { room(1); data.push_back(value ? 1 : 0); }
        else if constexpr (std::is_integral_v<T>)
        {
            static_assert(sizeof(T) <= 8);
            const auto bits = std::bit_cast<std::make_unsigned_t<T>>(value);
            room(sizeof(T)); for (unsigned i = 0; i < sizeof(T); ++i) data.push_back(uint8_t(bits >> (8 * i)));
        }
        else if constexpr (std::is_same_v<T, float>) (*this)(std::bit_cast<uint32_t>(value));
        else if constexpr (std::is_same_v<T, double>) (*this)(std::bit_cast<uint64_t>(value));
        else if constexpr (std::is_enum_v<T>) (*this)(static_cast<std::underlying_type_t<T>>(value));
        else canonicalFields(*this, value);
    }
    void blob(std::span<const uint8_t> bytes)
    {
        if (bytes.size() > UINT32_MAX) throw std::invalid_argument("canonical blob is too large");
        (*this)(uint32_t(bytes.size())); room(bytes.size()); data.insert(data.end(), bytes.begin(), bytes.end());
    }
    std::vector<uint8_t> finish()
    {
        put32(data, 16, uint32_t(data.size() - 24)); put32(data, 20, checksum(data)); return std::move(data);
    }
};
class Reader
{
    std::span<const uint8_t> data;
    size_t offset = 24;
public:
    static constexpr bool reading = true;
    Reader(std::span<const uint8_t> bytes, std::array<uint8_t, 8> magic, uint32_t variant, size_t bound) : data(bytes)
    {
        if (bytes.size() < 24 || bytes.size() > bound || !std::equal(magic.begin(), magic.end(), bytes.begin()) ||
            get32(bytes, 8) != 1 || get32(bytes, 12) != variant || get32(bytes, 16) != bytes.size() - 24 ||
            get32(bytes, 20) != checksum(bytes))
            throw std::invalid_argument("invalid canonical device header, length or checksum");
    }
    size_t remaining() const { return data.size() - offset; }
    template <typename T> void operator()(T &value)
    {
        if constexpr (std::is_same_v<T, bool>)
        {
            uint8_t byte; (*this)(byte); if (byte > 1) throw std::invalid_argument("noncanonical device boolean"); value = byte != 0;
        }
        else if constexpr (std::is_integral_v<T>)
        {
            static_assert(sizeof(T) <= 8);
            if (remaining() < sizeof(T)) throw std::invalid_argument("truncated canonical device field");
            std::make_unsigned_t<T> bits = 0;
            for (unsigned i = 0; i < sizeof(T); ++i) bits |= std::make_unsigned_t<T>(data[offset++]) << (8 * i);
            value = std::bit_cast<T>(bits);
        }
        else if constexpr (std::is_same_v<T, float>) { uint32_t bits; (*this)(bits); value = std::bit_cast<float>(bits); }
        else if constexpr (std::is_same_v<T, double>) { uint64_t bits; (*this)(bits); value = std::bit_cast<double>(bits); }
        else if constexpr (std::is_enum_v<T>) { std::underlying_type_t<T> bits; (*this)(bits); value = static_cast<T>(bits); }
        else canonicalFields(*this, value);
    }
    void blob(std::vector<uint8_t> &bytes)
    {
        uint32_t length; (*this)(length);
        if (length > remaining()) throw std::invalid_argument("truncated canonical device blob");
        bytes.assign(data.begin() + offset, data.begin() + offset + length); offset += length;
    }
    void finish() const
    { if (remaining() != 0) throw std::invalid_argument("trailing canonical device fields"); }
};
template <typename A, typename T, size_t N> void canonicalFields(A &a, const std::array<T, N> &values)
{ for (const auto &v : values) a(v); }
template <typename A, typename T, size_t N> void canonicalFields(A &a, std::array<T, N> &values)
{ for (auto &v : values) a(v); }
template <typename A, typename T, size_t N> void canonicalFields(A &a, const T (&values)[N])
{ for (const auto &v : values) a(v); }
template <typename A, typename T, size_t N> void canonicalFields(A &a, T (&values)[N])
{ for (auto &v : values) a(v); }
template <typename A> void canonicalFields(A &a, const std::vector<uint8_t> &v) { a.blob(v); }
template <typename A> void canonicalFields(A &a, std::vector<uint8_t> &v) { a.blob(v); }
}
