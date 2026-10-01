#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ps2native::nexo::iop_lab
{
    inline std::vector<uint8_t> readImage(const std::filesystem::path &path)
    {
        constexpr uint64_t maxBytes = 64u * 1024u * 1024u;
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input)
            throw std::runtime_error("cannot open IRX image");
        const auto size = input.tellg();
        if (size <= 0 || static_cast<uint64_t>(size) > maxBytes)
            throw std::runtime_error("IRX image size outside 1..64 MiB");
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
            throw std::runtime_error("cannot read complete IRX image");
        return bytes;
    }

    inline uint32_t parseCursor(std::string_view value)
    {
        size_t consumed = 0;
        const auto result = std::stoull(std::string(value), &consumed, 0);
        if (consumed != value.size() || value.empty() || value.front() == '-' ||
            result < 0x10000u || result >= 0x120000u)
            throw std::runtime_error("module cursor outside its RAM arena");
        return static_cast<uint32_t>(result);
    }

    inline void writeBytes(const std::filesystem::path &path, std::span<const uint8_t> bytes)
    {
        std::ofstream output(path, std::ios::binary);
        if (!output || !output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
            throw std::runtime_error("cannot write laboratory artifact");
    }

    inline std::string jsonString(std::string_view value)
    {
        constexpr char hex[] = "0123456789abcdef";
        std::string result = "\"";
        for (const unsigned char byte : value)
        {
            if (byte == '\\' || byte == '"')
            {
                result += '\\'; result += static_cast<char>(byte);
            }
            else if (byte < 0x20u || byte >= 0x80u)
            {
                result += "\\u00"; result += hex[byte >> 4u]; result += hex[byte & 15u];
            }
            else
                result += static_cast<char>(byte);
        }
        return result + '"';
    }
}
