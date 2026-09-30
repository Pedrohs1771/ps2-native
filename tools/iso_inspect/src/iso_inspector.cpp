#include "iso_inspector.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace ps2iso {
namespace {

constexpr std::uint64_t kSectorSize = 2048U;
constexpr std::uint64_t kFirstVolumeDescriptorLba = 16U;
constexpr std::uint64_t kMaxDescriptorCount = 256U;
constexpr std::uint64_t kMaxDirectoryBytes = 64U * 1024U * 1024U;
constexpr std::uint64_t kMaxCumulativeDirectoryBytes = 256U * 1024U * 1024U;
constexpr std::size_t kMaxDirectoryDepth = 128U;
constexpr std::size_t kMaxEntryCount = 250'000U;
constexpr std::size_t kMaxTotalPathBytes = 64U * 1024U * 1024U;
constexpr std::uint64_t kMaxElfProgramHeaderBytes = 16U * 1024U * 1024U;
constexpr std::uint64_t kMaxSystemCnfBytes = 64U * 1024U;
constexpr std::uint32_t kEfMipsArchMask = 0xf0000000U;
constexpr std::uint32_t kEfMipsArch3 = 0x20000000U;

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

bool addOverflows(std::uint64_t left, std::uint64_t right) {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
}

std::uint16_t readLe16(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 2U) {
        fail("truncated 16-bit field");
    }
    return static_cast<std::uint16_t>(data[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1U]) << 8U);
}

std::uint16_t readBe16(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 2U) {
        fail("truncated big-endian 16-bit field");
    }
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset]) << 8U) |
           static_cast<std::uint16_t>(data[offset + 1U]);
}

std::uint32_t readLe32(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4U) {
        fail("truncated 32-bit field");
    }
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(data[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(data[offset + 3U]) << 24U);
}

std::uint32_t readBe32(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4U) {
        fail("truncated 32-bit field");
    }
    return (static_cast<std::uint32_t>(data[offset]) << 24U) |
           (static_cast<std::uint32_t>(data[offset + 1U]) << 16U) |
           (static_cast<std::uint32_t>(data[offset + 2U]) << 8U) |
           static_cast<std::uint32_t>(data[offset + 3U]);
}

std::uint64_t readLe64(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 8U) {
        fail("truncated little-endian 64-bit field");
    }
    std::uint64_t value = 0U;
    for (unsigned int index = 0U; index < 8U; ++index) {
        value |= static_cast<std::uint64_t>(data[offset + index]) << (index * 8U);
    }
    return value;
}

std::uint64_t readBe64(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 8U) {
        fail("truncated big-endian 64-bit field");
    }
    std::uint64_t value = 0U;
    for (unsigned int index = 0U; index < 8U; ++index) {
        value = (value << 8U) | data[offset + index];
    }
    return value;
}

std::uint16_t readBoth16(std::span<const std::uint8_t> data, std::size_t offset,
                         const std::string& field) {
    if (offset > data.size() || data.size() - offset < 4U) {
        fail("truncated ISO9660 both-endian field: " + field);
    }
    const auto little = readLe16(data, offset);
    const auto big = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[offset + 2U]) << 8U) | data[offset + 3U]);
    if (little != big) {
        fail("ISO9660 both-endian field mismatch: " + field);
    }
    return little;
}

std::uint32_t readBoth32(std::span<const std::uint8_t> data, std::size_t offset,
                         const std::string& field) {
    const auto little = readLe32(data, offset);
    const auto big = readBe32(data, offset + 4U);
    if (little != big) {
        fail("ISO9660 both-endian field mismatch: " + field);
    }
    return little;
}

std::string trim(std::string value) {
    const auto isSpace = [](unsigned char character) { return std::isspace(character) != 0; };
    auto first = std::find_if_not(value.begin(), value.end(), isSpace);
    auto last = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

std::string hexByte(std::uint8_t value) {
    std::ostringstream output;
    output << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<unsigned int>(value);
    return output.str();
}

void appendUtf8(std::string& target, std::uint32_t codePoint) {
    if (codePoint <= 0x7fU) {
        target.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7ffU) {
        target.push_back(static_cast<char>(0xc0U | (codePoint >> 6U)));
        target.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    } else if (codePoint <= 0xffffU) {
        target.push_back(static_cast<char>(0xe0U | (codePoint >> 12U)));
        target.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
        target.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    } else if (codePoint <= 0x10ffffU) {
        target.push_back(static_cast<char>(0xf0U | (codePoint >> 18U)));
        target.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3fU)));
        target.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
        target.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    } else {
        appendUtf8(target, 0xfffdU);
    }
}

std::string decodeIsoName(std::span<const std::uint8_t> bytes, bool joliet) {
    std::string result;
    if (joliet) {
        if ((bytes.size() % 2U) != 0U) {
            fail("Joliet file identifier has an odd byte length");
        }
        for (std::size_t index = 0; index < bytes.size(); index += 2U) {
            std::uint32_t codePoint = (static_cast<std::uint32_t>(bytes[index]) << 8U) |
                                      static_cast<std::uint32_t>(bytes[index + 1U]);
            if (codePoint >= 0xd800U && codePoint <= 0xdbffU) {
                if (index + 3U < bytes.size()) {
                    const std::uint32_t low = (static_cast<std::uint32_t>(bytes[index + 2U]) << 8U) |
                                              static_cast<std::uint32_t>(bytes[index + 3U]);
                    if (low >= 0xdc00U && low <= 0xdfffU) {
                        codePoint = 0x10000U + ((codePoint - 0xd800U) << 10U) + (low - 0xdc00U);
                        index += 2U;
                    } else {
                        codePoint = 0xfffdU;
                    }
                } else {
                    codePoint = 0xfffdU;
                }
            } else if (codePoint >= 0xdc00U && codePoint <= 0xdfffU) {
                codePoint = 0xfffdU;
            }
            if (codePoint == '/' || codePoint == '\\') {
                result += codePoint == '/' ? "%2F" : "%5C";
            } else if (codePoint < 0x20U || (codePoint >= 0x7fU && codePoint <= 0x9fU)) {
                if (codePoint <= 0xffU) {
                    result += hexByte(static_cast<std::uint8_t>(codePoint));
                } else {
                    appendUtf8(result, 0xfffdU);
                }
            } else {
                appendUtf8(result, codePoint);
            }
        }
        return result;
    }

    for (const std::uint8_t byte : bytes) {
        if (byte == '/' || byte == '\\') {
            result += byte == '/' ? "%2F" : "%5C";
        } else if (byte >= 0x20U && byte <= 0x7eU) {
            result.push_back(static_cast<char>(byte));
        } else {
            result += hexByte(byte);
        }
    }
    return result;
}

bool isAsciiJoliet(std::span<const std::uint8_t> identifier) {
    return identifier.size() >= 3U && identifier[0] == 0x25U && identifier[1] == 0x2fU &&
           (identifier[2] == 0x40U || identifier[2] == 0x43U || identifier[2] == 0x45U);
}

std::string decodeVolumeId(std::span<const std::uint8_t> bytes, bool joliet) {
    auto value = trim(decodeIsoName(bytes, joliet));
    return value;
}

std::string canonicalIsoPath(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    const auto colon = path.find(':');
    if (colon != std::string::npos) {
        path.erase(0U, colon + 1U);
    }
    while (!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }

    std::string normalized;
    std::size_t start = 0U;
    while (start <= path.size()) {
        const auto end = path.find('/', start);
        auto component = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const auto semicolon = component.rfind(';');
        if (semicolon != std::string::npos && semicolon + 1U < component.size() &&
            std::all_of(component.begin() + static_cast<std::ptrdiff_t>(semicolon + 1U), component.end(),
                        [](unsigned char character) { return std::isdigit(character) != 0; })) {
            component.erase(semicolon);
        }
        if (!normalized.empty()) {
            normalized.push_back('/');
        }
        normalized += uppercase(std::move(component));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1U;
    }
    return normalized;
}

std::string stripIsoVersionSuffix(std::string component) {
    const auto semicolon = component.rfind(';');
    if (semicolon != std::string::npos && semicolon + 1U < component.size() &&
        std::all_of(component.begin() + static_cast<std::ptrdiff_t>(semicolon + 1U), component.end(),
                    [](unsigned char character) { return std::isdigit(character) != 0; })) {
        component.erase(semicolon);
    }
    return component;
}

bool pathIsWithin(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end(); ++rootPart, ++candidatePart) {
        if (candidatePart == candidate.end() || *rootPart != *candidatePart) {
            return false;
        }
    }
    return true;
}

struct ExtractionItem {
    const DirectoryEntry* entry{};
    std::filesystem::path relative_path;
};

class Sha256 {
public:
    void update(std::span<const std::uint8_t> data) {
        for (const auto byte : data) {
            buffer_[buffer_size_++] = byte;
            bit_count_ += 8U;
            if (buffer_size_ == buffer_.size()) {
                transform(buffer_);
                buffer_size_ = 0U;
            }
        }
    }

    std::string finish() {
        buffer_[buffer_size_++] = 0x80U;
        if (buffer_size_ > 56U) {
            while (buffer_size_ < buffer_.size()) {
                buffer_[buffer_size_++] = 0U;
            }
            transform(buffer_);
            buffer_size_ = 0U;
        }
        while (buffer_size_ < 56U) {
            buffer_[buffer_size_++] = 0U;
        }
        for (int shift = 56; shift >= 0; shift -= 8) {
            buffer_[buffer_size_++] = static_cast<std::uint8_t>((bit_count_ >> shift) & 0xffU);
        }
        transform(buffer_);

        std::ostringstream output;
        output << std::hex << std::setfill('0');
        for (const auto word : state_) {
            output << std::setw(8) << word;
        }
        return output.str();
    }

private:
    static constexpr std::array<std::uint32_t, 64U> kRoundConstants{
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};

    static std::uint32_t rotateRight(std::uint32_t value, unsigned int shift) {
        return (value >> shift) | (value << (32U - shift));
    }

    void transform(const std::array<std::uint8_t, 64U>& block) {
        std::array<std::uint32_t, 64U> words{};
        for (std::size_t index = 0; index < 16U; ++index) {
            const auto offset = index * 4U;
            words[index] = (static_cast<std::uint32_t>(block[offset]) << 24U) |
                           (static_cast<std::uint32_t>(block[offset + 1U]) << 16U) |
                           (static_cast<std::uint32_t>(block[offset + 2U]) << 8U) |
                           static_cast<std::uint32_t>(block[offset + 3U]);
        }
        for (std::size_t index = 16U; index < words.size(); ++index) {
            const auto s0 = rotateRight(words[index - 15U], 7U) ^ rotateRight(words[index - 15U], 18U) ^ (words[index - 15U] >> 3U);
            const auto s1 = rotateRight(words[index - 2U], 17U) ^ rotateRight(words[index - 2U], 19U) ^ (words[index - 2U] >> 10U);
            words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
        }

        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        auto e = state_[4];
        auto f = state_[5];
        auto g = state_[6];
        auto h = state_[7];
        for (std::size_t index = 0; index < 64U; ++index) {
            const auto sum1 = rotateRight(e, 6U) ^ rotateRight(e, 11U) ^ rotateRight(e, 25U);
            const auto choice = (e & f) ^ ((~e) & g);
            const auto temp1 = h + sum1 + choice + kRoundConstants[index] + words[index];
            const auto sum0 = rotateRight(a, 2U) ^ rotateRight(a, 13U) ^ rotateRight(a, 22U);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8U> state_{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                         0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    std::array<std::uint8_t, 64U> buffer_{};
    std::size_t buffer_size_{};
    std::uint64_t bit_count_{};
};

class Reader {
public:
    explicit Reader(const std::filesystem::path& path)
        : input_(path, std::ios::binary) {
        if (!input_) {
            fail("cannot open ISO image: " + path.string());
        }
        std::error_code error;
        size_ = std::filesystem::file_size(path, error);
        if (error) {
            fail("cannot determine ISO image size: " + path.string());
        }
        if (size_ == 0U) {
            fail("ISO image is empty");
        }
    }

    std::uint64_t size() const { return size_; }

    void read(std::uint64_t offset, std::span<std::uint8_t> destination) {
        const auto length = static_cast<std::uint64_t>(destination.size());
        if (addOverflows(offset, length) || offset + length > size_) {
            fail("read extends beyond ISO image bounds");
        }
        if (destination.empty()) {
            return;
        }
        if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()) ||
            length > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
            fail("requested ISO range exceeds stream limits");
        }
        input_.clear();
        input_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!input_) {
            fail("failed to seek within ISO image");
        }
        input_.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(length));
        if (input_.gcount() != static_cast<std::streamsize>(length)) {
            fail("short read from ISO image");
        }
    }

    std::vector<std::uint8_t> readVector(std::uint64_t offset, std::uint64_t length,
                                         std::uint64_t maximum, const std::string& label) {
        if (length > maximum || length > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            fail(label + " exceeds configured read limit");
        }
        std::vector<std::uint8_t> result(static_cast<std::size_t>(length));
        read(offset, result);
        return result;
    }

    std::string hash(std::uint64_t offset, std::uint64_t length) {
        if (addOverflows(offset, length) || offset + length > size_) {
            fail("hash range extends beyond ISO image bounds");
        }
        Sha256 digest;
        std::array<std::uint8_t, 64U * 1024U> buffer{};
        std::uint64_t consumed = 0U;
        while (consumed < length) {
            const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), length - consumed));
            read(offset + consumed, std::span<std::uint8_t>(buffer.data(), chunk));
            digest.update(std::span<const std::uint8_t>(buffer.data(), chunk));
            consumed += static_cast<std::uint64_t>(chunk);
        }
        return digest.finish();
    }

private:
    std::ifstream input_;
    std::uint64_t size_{};
};

struct VolumeDescriptor {
    std::string volume_id;
    std::uint32_t volume_sectors{};
    std::uint16_t block_size{};
    std::uint32_t root_extent{};
    std::uint32_t root_size{};
    bool joliet{};
};

struct ParsedDirectoryRecord {
    std::string name;
    std::uint32_t extent{};
    std::uint32_t size{};
    std::uint8_t flags{};
};

VolumeDescriptor parseDescriptor(std::span<const std::uint8_t> descriptor, bool joliet) {
    VolumeDescriptor result;
    result.volume_id = decodeVolumeId(descriptor.subspan(40U, 32U), joliet);
    result.volume_sectors = readBoth32(descriptor, 80U, "volume space size");
    result.block_size = readBoth16(descriptor, 128U, "logical block size");
    const auto recordLength = static_cast<std::size_t>(descriptor[156U]);
    if (recordLength < 34U || recordLength > descriptor.size() - 156U) {
        fail("invalid root directory record in volume descriptor");
    }
    const auto root = descriptor.subspan(156U, recordLength);
    result.root_extent = readBoth32(root, 2U, "root extent location");
    result.root_size = readBoth32(root, 10U, "root directory size");
    if ((root[25U] & 0x02U) == 0U) {
        fail("volume descriptor root record is not a directory");
    }
    result.joliet = joliet;
    return result;
}

ParsedDirectoryRecord parseDirectoryRecord(std::span<const std::uint8_t> data, bool joliet) {
    if (data.size() < 34U) {
        fail("truncated ISO9660 directory record");
    }
    const std::size_t recordLength = data[0U];
    if (recordLength < 34U || recordLength > data.size()) {
        fail("invalid ISO9660 directory record length");
    }
    const auto identifierLength = static_cast<std::size_t>(data[32U]);
    if (identifierLength == 0U || 33U + identifierLength > recordLength) {
        fail("invalid ISO9660 file identifier length");
    }
    const auto identifier = data.subspan(33U, identifierLength);
    ParsedDirectoryRecord result;
    result.extent = readBoth32(data, 2U, "directory entry extent");
    result.size = readBoth32(data, 10U, "directory entry size");
    result.flags = data[25U];
    if (identifierLength == 1U && (identifier[0] == 0U || identifier[0] == 1U)) {
        result.name.clear();
    } else {
        result.name = decodeIsoName(identifier, joliet);
    }
    return result;
}

std::string findConfigValue(const std::string& config, const std::string& requestedKey) {
    std::istringstream lines(config);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const auto key = uppercase(trim(line.substr(0U, equals)));
        if (key == requestedKey) {
            return trim(line.substr(equals + 1U));
        }
    }
    return {};
}

std::string inferRegion(const std::string& bootPath) {
    const auto canonical = uppercase(canonicalIsoPath(bootPath));
    const auto slash = canonical.find_last_of('/');
    const auto filename = canonical.substr(slash == std::string::npos ? 0U : slash + 1U);
    if (filename.rfind("SLPM", 0U) == 0U || filename.rfind("SLPS", 0U) == 0U ||
        filename.rfind("SCPS", 0U) == 0U || filename.rfind("SCAJ", 0U) == 0U) {
        return "Japan (inferred from " + filename.substr(0U, 4U) + ")";
    }
    if (filename.rfind("SLUS", 0U) == 0U || filename.rfind("SCUS", 0U) == 0U) {
        return "North America (inferred from " + filename.substr(0U, 4U) + ")";
    }
    if (filename.rfind("SLES", 0U) == 0U || filename.rfind("SCES", 0U) == 0U) {
        return "Europe (inferred from " + filename.substr(0U, 4U) + ")";
    }
    return "Unknown (unrecognized boot serial)";
}

class Inspector {
public:
    Inspector(const std::filesystem::path& path) : reader_(path) {}

    InspectionReport run(bool includeHashes = true, bool includeBoot = true) {
        const auto descriptor = findVolumeDescriptor();
        validateVolume(descriptor);
        InspectionReport report;
        report.image_size = reader_.size();
        report.logical_block_size = descriptor.block_size;
        report.volume_id = descriptor.volume_id;
        report.used_joliet_names = descriptor.joliet;
        if (includeHashes) {
            report.iso_sha256 = reader_.hash(0U, reader_.size());
        }

        walkDirectory(descriptor.root_extent, descriptor.root_size, "", 0U, descriptor);
        report.entries = std::move(entries_);
        if (includeBoot) {
            report.boot = inspectBootConfiguration(report.entries, descriptor, includeHashes);
        }
        if (includeHashes) {
            report.elf_inventory = inspectElfInventory(report.entries, descriptor, report.boot.elf.path);
        }
        return report;
    }

    void extractTo(const std::filesystem::path& destination) {
        const auto report = run(false);
        std::vector<ExtractionItem> plan;
        std::set<std::string> normalizedPaths;
        plan.reserve(report.entries.size());
        for (const auto& entry : report.entries) {
            if (entry.path.size() < 2U || entry.path.front() != '/') {
                fail("ISO entry does not have a rooted ISO path");
            }
            ExtractionItem item;
            item.entry = &entry;
            std::size_t start = 1U;
            while (start <= entry.path.size()) {
                const auto end = entry.path.find('/', start);
                auto component = entry.path.substr(start, end == std::string::npos ? std::string::npos : end - start);
                if (component.empty()) {
                    fail("ISO entry contains an empty path component: " + entry.path);
                }
                component = stripIsoVersionSuffix(std::move(component));
                if (component.empty() || component == "." || component == ".." ||
                    component.find(':') != std::string::npos || component.find('/') != std::string::npos ||
                    component.find('\\') != std::string::npos ||
                    std::any_of(component.begin(), component.end(), [](unsigned char character) {
                        return character < 0x20U || character == 0x7fU;
                    })) {
                    fail("unsafe or empty path component in ISO entry: " + entry.path);
                }
                item.relative_path /= component;
                if (item.relative_path.native().size() > 4096U) {
                    fail("normalized extraction path exceeds the configured length limit");
                }
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1U;
            }
            const auto collisionKey = uppercase(item.relative_path.generic_string());
            if (!normalizedPaths.insert(collisionKey).second) {
                fail("extraction path collision after removing ISO version suffixes: " +
                     item.relative_path.generic_string());
            }
            plan.push_back(std::move(item));
        }

        std::error_code error;
        const auto destinationStatus = std::filesystem::symlink_status(destination, error);
        if (error && error != std::errc::no_such_file_or_directory) {
            fail("cannot inspect extraction destination: " + error.message());
        }
        if (!error && destinationStatus.type() != std::filesystem::file_type::not_found) {
            fail("extraction destination already exists: " + destination.string());
        }
        error.clear();
        if (!std::filesystem::create_directory(destination, error) || error) {
            fail("cannot create extraction destination: " +
                 (error ? error.message() : destination.string()));
        }
        error.clear();
        const auto canonicalRoot = std::filesystem::canonical(destination, error);
        if (error) {
            std::error_code cleanupError;
            std::filesystem::remove_all(destination, cleanupError);
            fail("cannot resolve extraction destination: " + error.message());
        }

        try {
            for (const auto& item : plan) {
                const auto target = (canonicalRoot / item.relative_path).lexically_normal();
                if (!pathIsWithin(canonicalRoot, target)) {
                    fail("normalized extraction target escaped the destination directory");
                }
                if (item.entry->is_directory) {
                    ensureDirectory(canonicalRoot, item.relative_path);
                } else {
                    auto parent = item.relative_path.parent_path();
                    if (!parent.empty()) {
                        ensureDirectory(canonicalRoot, parent);
                    }
                    writeEntry(*item.entry, target);
                }
            }
        } catch (...) {
            std::error_code cleanupError;
            std::filesystem::remove_all(destination, cleanupError);
            throw;
        }
    }

private:
    VolumeDescriptor findVolumeDescriptor() {
        std::optional<VolumeDescriptor> primary;
        std::optional<VolumeDescriptor> joliet;
        std::array<std::uint8_t, static_cast<std::size_t>(kSectorSize)> sector{};
        bool terminated = false;
        for (std::uint64_t index = 0; index < kMaxDescriptorCount; ++index) {
            const std::uint64_t lba = kFirstVolumeDescriptorLba + index;
            const std::uint64_t offset = lba * kSectorSize;
            if (offset > reader_.size() || reader_.size() - offset < kSectorSize) {
                break;
            }
            reader_.read(offset, sector);
            const std::span<const std::uint8_t> view(sector);
            if (std::string_view(reinterpret_cast<const char*>(sector.data() + 1U), 5U) != "CD001" ||
                sector[6U] != 1U) {
                fail("invalid ISO9660 volume descriptor signature or version");
            }
            if (sector[0U] == 1U) {
                primary = parseDescriptor(view, false);
            } else if (sector[0U] == 2U && isAsciiJoliet(view.subspan(88U, 3U))) {
                joliet = parseDescriptor(view, true);
            } else if (sector[0U] == 255U) {
                terminated = true;
                break;
            }
        }
        if (!terminated) {
            fail("ISO9660 volume descriptor terminator was not found");
        }
        if (!primary.has_value()) {
            fail("ISO9660 primary volume descriptor was not found");
        }
        return joliet.value_or(*primary);
    }

    void validateVolume(const VolumeDescriptor& descriptor) const {
        if (descriptor.block_size != kSectorSize) {
            fail("unsupported ISO9660 logical block size (expected 2048 bytes)");
        }
        if (descriptor.volume_sectors == 0U) {
            fail("ISO9660 volume declares zero sectors");
        }
        const auto volumeBytes = static_cast<std::uint64_t>(descriptor.volume_sectors) * descriptor.block_size;
        if (volumeBytes > reader_.size()) {
            fail("ISO9660 volume size exceeds the image file size");
        }
    }

    void validateExtent(std::uint32_t extent, std::uint32_t size,
                        const VolumeDescriptor& descriptor, const std::string& label) const {
        const auto offset = static_cast<std::uint64_t>(extent) * descriptor.block_size;
        const auto length = static_cast<std::uint64_t>(size);
        const auto volumeBytes = static_cast<std::uint64_t>(descriptor.volume_sectors) * descriptor.block_size;
        if (addOverflows(offset, length) || offset + length > volumeBytes || offset + length > reader_.size()) {
            fail(label + " extends beyond ISO volume bounds");
        }
    }

    void walkDirectory(std::uint32_t extent, std::uint32_t size, const std::string& parent,
                       std::size_t depth, const VolumeDescriptor& descriptor) {
        if (depth > kMaxDirectoryDepth) {
            fail("ISO9660 directory nesting exceeds the configured limit");
        }
        if (size > kMaxDirectoryBytes) {
            fail("ISO9660 directory exceeds the configured read limit");
        }
        validateExtent(extent, size, descriptor, "directory");
        const auto identity = std::to_string(extent) + ":" + std::to_string(size);
        if (!visited_directories_.insert(identity).second) {
            return;
        }
        if (size > kMaxCumulativeDirectoryBytes - directory_bytes_scanned_) {
            fail("ISO9660 directory data exceeds the cumulative scan limit");
        }
        directory_bytes_scanned_ += size;

        const auto offset = static_cast<std::uint64_t>(extent) * descriptor.block_size;
        const auto bytes = reader_.readVector(offset, size, kMaxDirectoryBytes, "directory");
        std::vector<DirectoryEntry> childDirectories;
        std::size_t position = 0U;
        while (position < bytes.size()) {
            const auto recordLength = static_cast<std::size_t>(bytes[position]);
            if (recordLength == 0U) {
                const auto nextSector = ((position / static_cast<std::size_t>(descriptor.block_size)) + 1U) *
                                        static_cast<std::size_t>(descriptor.block_size);
                if (nextSector <= position) {
                    fail("invalid ISO9660 directory sector padding");
                }
                position = std::min(nextSector, bytes.size());
                continue;
            }
            if (recordLength > bytes.size() - position) {
                fail("ISO9660 directory record crosses directory bounds");
            }
            const auto record = parseDirectoryRecord(
                std::span<const std::uint8_t>(bytes.data() + position, recordLength), descriptor.joliet);
            position += recordLength;
            if (record.name.empty()) {
                continue;
            }
            if ((record.flags & 0x80U) != 0U) {
                fail("ISO9660 multi-extent files are not supported yet");
            }
            if (record.name == "." || record.name == "..") {
                continue;
            }
            if (entries_.size() >= kMaxEntryCount) {
                fail("ISO9660 entry count exceeds the configured limit");
            }
            const std::string current = parent.empty() ? "/" + record.name : parent + "/" + record.name;
            if (current.size() > 4096U) {
                fail("ISO9660 path exceeds the configured length limit");
            }
            if (current.size() > kMaxTotalPathBytes - path_bytes_scanned_) {
                fail("ISO9660 path data exceeds the cumulative limit");
            }
            path_bytes_scanned_ += current.size();
            validateExtent(record.extent, record.size, descriptor, "entry " + current);
            DirectoryEntry entry{current, record.extent, record.size, (record.flags & 0x02U) != 0U};
            entries_.push_back(entry);
            if (entry.is_directory) {
                childDirectories.push_back(std::move(entry));
            }
        }
        for (const auto& child : childDirectories) {
            walkDirectory(child.extent_lba, child.size, child.path, depth + 1U, descriptor);
        }
    }

    BootReport inspectBootConfiguration(const std::vector<DirectoryEntry>& entries,
                                        const VolumeDescriptor& descriptor, bool includeHashes) {
        const DirectoryEntry* configEntry = nullptr;
        for (const auto& entry : entries) {
            if (!entry.is_directory && canonicalIsoPath(entry.path) == "SYSTEM.CNF") {
                configEntry = &entry;
                break;
            }
        }
        if (configEntry == nullptr) {
            fail("SYSTEM.CNF was not found in the ISO9660 tree");
        }
        const auto configOffset = static_cast<std::uint64_t>(configEntry->extent_lba) * descriptor.block_size;
        const auto configBytes = reader_.readVector(configOffset, configEntry->size,
                                                    kMaxSystemCnfBytes, "SYSTEM.CNF");
        const std::string config(configBytes.begin(), configBytes.end());
        BootReport result;
        result.system_cnf_path = configEntry->path;
        result.boot2 = findConfigValue(config, "BOOT2");
        result.version = findConfigValue(config, "VER");
        result.video_mode = findConfigValue(config, "VMODE");
        if (result.boot2.empty()) {
            fail("SYSTEM.CNF does not contain a BOOT2 entry");
        }

        const auto wanted = canonicalIsoPath(result.boot2);
        const DirectoryEntry* elfEntry = nullptr;
        for (const auto& entry : entries) {
            if (!entry.is_directory && canonicalIsoPath(entry.path) == wanted) {
                elfEntry = &entry;
                break;
            }
        }
        if (elfEntry == nullptr) {
            fail("BOOT2 executable was not found in the ISO9660 tree: " + result.boot2);
        }
        result.region = inferRegion(result.boot2);
        result.elf = inspectElf(*elfEntry, descriptor, includeHashes);
        return result;
    }

    ElfReport inspectElf(const DirectoryEntry& entry, const VolumeDescriptor& descriptor, bool includeHash) {
        const std::uint64_t fileOffset = static_cast<std::uint64_t>(entry.extent_lba) * descriptor.block_size;
        const auto header = reader_.readVector(fileOffset, std::min<std::uint64_t>(entry.size, 52U), 52U, "ELF header");
        if (header.size() < 52U || header[0U] != 0x7fU || header[1U] != 'E' || header[2U] != 'L' ||
            header[3U] != 'F') {
            fail("BOOT2 target is not a valid ELF32 executable");
        }
        if (header[4U] != 1U || header[5U] != 1U || header[6U] != 1U) {
            fail("BOOT2 ELF must be ELF32 little-endian version 1");
        }
        const std::span<const std::uint8_t> headerSpan(header);
        if (readLe16(headerSpan, 16U) != 2U) {
            fail("BOOT2 target is not an ELF ET_EXEC executable");
        }
        if (readLe32(headerSpan, 20U) != 1U) {
            fail("BOOT2 ELF header version is unsupported");
        }
        const auto machine = readLe16(headerSpan, 18U);
        if (machine != 8U) {
            fail("BOOT2 ELF machine is not MIPS (EM_MIPS)");
        }
        const auto entrypoint = readLe32(headerSpan, 24U);
        const auto programHeaderOffset = readLe32(headerSpan, 28U);
        const auto headerSize = readLe16(headerSpan, 40U);
        const auto programHeaderSize = readLe16(headerSpan, 42U);
        const auto programHeaderCount = readLe16(headerSpan, 44U);
        if (headerSize < 52U) {
            fail("ELF header declares a size smaller than ELF32 header");
        }
        const std::uint64_t tableBytes = static_cast<std::uint64_t>(programHeaderSize) * programHeaderCount;
        if (programHeaderCount > 0U && programHeaderSize < 32U) {
            fail("ELF program header entries are smaller than ELF32 program headers");
        }
        if (tableBytes > kMaxElfProgramHeaderBytes || addOverflows(programHeaderOffset, tableBytes) ||
            static_cast<std::uint64_t>(programHeaderOffset) + tableBytes > entry.size) {
            fail("ELF program header table is outside the executable bounds");
        }
        const auto table = reader_.readVector(fileOffset + programHeaderOffset, tableBytes,
                                              kMaxElfProgramHeaderBytes, "ELF program header table");
        ElfReport result;
        result.path = entry.path;
        if (includeHash) {
            result.sha256 = reader_.hash(fileOffset, entry.size);
        }
        result.machine = machine;
        result.entrypoint = entrypoint;
        bool entrypointIsExecutable = false;
        const std::span<const std::uint8_t> tableSpan(table);
        for (std::uint16_t index = 0U; index < programHeaderCount; ++index) {
            const auto offset = static_cast<std::size_t>(index) * programHeaderSize;
            if (readLe32(tableSpan, offset) != 1U) { // PT_LOAD only
                continue;
            }
            LoadSegment segment;
            segment.file_offset = readLe32(tableSpan, offset + 4U);
            segment.virtual_address = readLe32(tableSpan, offset + 8U);
            segment.physical_address = readLe32(tableSpan, offset + 12U);
            segment.file_size = readLe32(tableSpan, offset + 16U);
            segment.memory_size = readLe32(tableSpan, offset + 20U);
            segment.flags = readLe32(tableSpan, offset + 24U);
            segment.alignment = readLe32(tableSpan, offset + 28U);
            const auto segmentEnd = static_cast<std::uint64_t>(segment.file_offset) + segment.file_size;
            const auto memoryEnd = static_cast<std::uint64_t>(segment.virtual_address) + segment.memory_size;
            if (segment.file_size > segment.memory_size || segmentEnd > entry.size || memoryEnd > 0x1'0000'0000ULL) {
                fail("ELF PT_LOAD segment has a range outside the executable or address space");
            }
            if ((segment.flags & 1U) != 0U && entrypoint >= segment.virtual_address &&
                static_cast<std::uint64_t>(entrypoint) < memoryEnd) {
                entrypointIsExecutable = true;
            }
            result.segments.push_back(segment);
        }
        if (result.segments.empty()) {
            fail("BOOT2 ELF contains no PT_LOAD segments");
        }
        if (!entrypointIsExecutable) {
            fail("BOOT2 ELF entrypoint is outside executable PT_LOAD segments");
        }
        return result;
    }

    std::vector<ElfInventoryItem> inspectElfInventory(const std::vector<DirectoryEntry>& entries,
                                                        const VolumeDescriptor& descriptor,
                                                        const std::string& bootPath) {
        std::vector<ElfInventoryItem> result;
        std::map<std::string, std::size_t> itemByHash;
        const std::string canonicalBootPath = canonicalIsoPath(bootPath);

        for (const auto& entry : entries) {
            if (entry.is_directory || entry.size < 4U) {
                continue;
            }

            const std::uint64_t fileOffset = static_cast<std::uint64_t>(entry.extent_lba) * descriptor.block_size;
            const auto prefix = reader_.readVector(fileOffset, std::min<std::uint64_t>(entry.size, 64U),
                                                   64U, "ELF inventory header");
            if (prefix.size() < 4U || prefix[0U] != 0x7fU || prefix[1U] != 'E' ||
                prefix[2U] != 'L' || prefix[3U] != 'F') {
                continue;
            }

            ElfInventoryItem item;
            item.size_bytes = entry.size;
            item.sha256 = reader_.hash(fileOffset, entry.size);
            item.id = "sha256:" + item.sha256;
            item.iso_paths.push_back(entry.path);
            item.role = "unclassified_elf";
            item.compile_status = "unsupported";

            try {
                if (prefix.size() < 16U) {
                    fail("ELF identification is truncated");
                }
                item.elf_class = prefix[4U];
                item.data_encoding = prefix[5U];
                if (prefix[6U] != 1U) {
                    fail("ELF identification version is unsupported");
                }
                if (item.data_encoding != 1U && item.data_encoding != 2U) {
                    fail("ELF byte order is unsupported");
                }

                const bool littleEndian = item.data_encoding == 1U;
                const auto read16 = [littleEndian](std::span<const std::uint8_t> data, std::size_t offset) {
                    return littleEndian ? readLe16(data, offset) : readBe16(data, offset);
                };
                const auto read32 = [littleEndian](std::span<const std::uint8_t> data, std::size_t offset) {
                    return littleEndian ? readLe32(data, offset) : readBe32(data, offset);
                };
                const auto read64 = [littleEndian](std::span<const std::uint8_t> data, std::size_t offset) {
                    return littleEndian ? readLe64(data, offset) : readBe64(data, offset);
                };

                if (item.elf_class == 1U) {
                    const auto header = reader_.readVector(fileOffset, std::min<std::uint64_t>(entry.size, 52U),
                                                           52U, "ELF32 inventory header");
                    if (header.size() < 52U) {
                        fail("ELF32 header is truncated");
                    }
                    const std::span<const std::uint8_t> bytes(header);
                    if (read32(bytes, 20U) != 1U) {
                        fail("ELF32 header version is unsupported");
                    }
                    item.type = read16(bytes, 16U);
                    item.machine = read16(bytes, 18U);
                    item.entrypoint = read32(bytes, 24U);
                    const std::uint32_t programHeaderOffset = read32(bytes, 28U);
                    const std::uint16_t headerSize = read16(bytes, 40U);
                    const std::uint16_t programHeaderSize = read16(bytes, 42U);
                    const std::uint16_t programHeaderCount = read16(bytes, 44U);
                    item.flags = read32(bytes, 36U);
                    if (headerSize < 52U) {
                        fail("ELF32 header declares an undersized header");
                    }
                    const std::uint64_t tableBytes = static_cast<std::uint64_t>(programHeaderSize) * programHeaderCount;
                    if (programHeaderCount > 0U && programHeaderSize < 32U) {
                        fail("ELF32 program header entries are undersized");
                    }
                    if (tableBytes > kMaxElfProgramHeaderBytes || addOverflows(programHeaderOffset, tableBytes) ||
                        static_cast<std::uint64_t>(programHeaderOffset) + tableBytes > entry.size) {
                        fail("ELF32 program header table is outside the file bounds");
                    }
                    const auto table = reader_.readVector(fileOffset + programHeaderOffset, tableBytes,
                                                          kMaxElfProgramHeaderBytes, "ELF32 inventory program headers");
                    const std::span<const std::uint8_t> tableSpan(table);
                    bool entrypointIsExecutable = false;
                    for (std::uint16_t index = 0U; index < programHeaderCount; ++index) {
                        const auto offset = static_cast<std::size_t>(index) * programHeaderSize;
                        if (read32(tableSpan, offset) != 1U) {
                            continue;
                        }
                        LoadSegment segment;
                        segment.file_offset = read32(tableSpan, offset + 4U);
                        segment.virtual_address = read32(tableSpan, offset + 8U);
                        segment.physical_address = read32(tableSpan, offset + 12U);
                        segment.file_size = read32(tableSpan, offset + 16U);
                        segment.memory_size = read32(tableSpan, offset + 20U);
                        segment.flags = read32(tableSpan, offset + 24U);
                        segment.alignment = read32(tableSpan, offset + 28U);
                        const auto fileEnd = static_cast<std::uint64_t>(segment.file_offset) + segment.file_size;
                        const auto memoryEnd = static_cast<std::uint64_t>(segment.virtual_address) + segment.memory_size;
                        if (segment.file_size > segment.memory_size || fileEnd > entry.size ||
                            memoryEnd > 0x1'0000'0000ULL) {
                            fail("ELF32 PT_LOAD segment has invalid bounds");
                        }
                        if ((segment.flags & 1U) != 0U && item.entrypoint >= segment.virtual_address &&
                            item.entrypoint < memoryEnd) {
                            entrypointIsExecutable = true;
                        }
                        item.load_segments.push_back(segment);
                    }
                    if (item.type == 2U && !item.load_segments.empty() && !entrypointIsExecutable) {
                        fail("ELF32 ET_EXEC entrypoint is outside executable PT_LOAD segments");
                    }
                } else if (item.elf_class == 2U) {
                    const auto header = reader_.readVector(fileOffset, std::min<std::uint64_t>(entry.size, 64U),
                                                           64U, "ELF64 inventory header");
                    if (header.size() < 64U) {
                        fail("ELF64 header is truncated");
                    }
                    const std::span<const std::uint8_t> bytes(header);
                    if (read32(bytes, 20U) != 1U) {
                        fail("ELF64 header version is unsupported");
                    }
                    item.type = read16(bytes, 16U);
                    item.machine = read16(bytes, 18U);
                    item.entrypoint = read64(bytes, 24U);
                    item.flags = read32(bytes, 48U);
                    item.reason = "ELF64 is inventoried but outside the current PS2 recompiler input format.";
                } else {
                    fail("ELF class is not ELF32 or ELF64");
                }

                const bool isBoot = canonicalIsoPath(entry.path) == canonicalBootPath;
                const bool mips = item.machine == 8U;
                const bool elf32Little = item.elf_class == 1U && item.data_encoding == 1U;
                if (isBoot) {
                    item.role = "boot_elf";
                    if (mips && elf32Little && item.type == 2U && !item.load_segments.empty()) {
                        item.compile_status = "boot_candidate";
                        item.reason.clear();
                    } else {
                        item.reason = "The SYSTEM.CNF boot target does not match the current ELF32 little-endian MIPS ET_EXEC input format.";
                    }
                } else if (mips && elf32Little && item.type == 2U && !item.load_segments.empty()) {
                    if ((item.flags & kEfMipsArchMask) == kEfMipsArch3) {
                        item.role = "secondary_ee_exec_candidate";
                        item.compile_status = "ee_candidate_by_mips3_abi";
                        item.reason = "The ELF declares the MIPS III ISA used by the PS2 EE toolchain; this is a conservative subsystem heuristic, not proof of EE ownership.";
                    } else {
                        item.role = "secondary_mips_exec_candidate";
                        item.compile_status = "needs_ee_iop_classification";
                        item.reason = "The MIPS ET_EXEC flags do not identify the PS2 EE MIPS III ISA; retained for manual classification.";
                    }
                } else if (mips && item.type == 1U) {
                    const std::string canonicalPath = canonicalIsoPath(entry.path);
                    if (canonicalPath.size() >= 4U && canonicalPath.ends_with(".IRX")) {
                        item.role = "iop_irx_candidate";
                        item.compile_status = "runtime_module_candidate";
                        item.reason = "Relocatable MIPS IRX remains on the IOP module path; it is not sent to the R5900 analyzer.";
                    } else {
                        item.role = "relocatable_mips_module_candidate";
                        item.compile_status = "needs_subsystem_classification";
                        item.reason = "Relocatable MIPS object requires subsystem and relocation-format classification.";
                    }
                } else {
                    item.role = mips ? "other_mips_elf" : "non_mips_elf";
                    item.compile_status = "inventory_only";
                    if (item.reason.empty()) {
                        item.reason = "The ELF is recorded for inspection but is outside the current boot-ELF recompilation path.";
                    }
                }
            } catch (const std::exception& error) {
                item.role = "unclassified_elf";
                item.compile_status = "unsupported";
                item.reason = error.what();
            }

            const auto found = itemByHash.find(item.sha256);
            if (found == itemByHash.end()) {
                itemByHash.emplace(item.sha256, result.size());
                result.push_back(std::move(item));
            } else {
                auto& existing = result[found->second];
                existing.iso_paths.push_back(entry.path);
                if (item.role == "boot_elf") {
                    existing.role = item.role;
                    existing.compile_status = item.compile_status;
                    existing.reason = item.reason;
                }
            }
        }
        return result;
    }

    void ensureDirectory(const std::filesystem::path& root, const std::filesystem::path& relative) {
        std::filesystem::path current = root;
        for (const auto& component : relative) {
            current /= component;
            std::error_code error;
            const auto status = std::filesystem::symlink_status(current, error);
            if (error && error != std::errc::no_such_file_or_directory) {
                fail("cannot inspect output directory: " + error.message());
            }
            if (!error && status.type() != std::filesystem::file_type::not_found) {
                if (std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
                    fail("extraction path conflicts with a non-directory or symlink: " + current.string());
                }
                continue;
            }
            error.clear();
            if (!std::filesystem::create_directory(current, error) || error) {
                fail("cannot create output directory: " + (error ? error.message() : current.string()));
            }
            error.clear();
            const auto canonicalCurrent = std::filesystem::canonical(current, error);
            if (error || !pathIsWithin(root, canonicalCurrent)) {
                fail("created output directory is outside the extraction destination");
            }
        }
    }

    void writeEntry(const DirectoryEntry& entry, const std::filesystem::path& target) {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(target, error);
        if (error && error != std::errc::no_such_file_or_directory) {
            fail("cannot inspect output file: " + error.message());
        }
        if (!error && status.type() != std::filesystem::file_type::not_found) {
            fail("extraction target already exists: " + target.string());
        }
        std::ofstream output(target, std::ios::binary | std::ios::out);
        if (!output) {
            fail("cannot create output file: " + target.string());
        }

        const std::uint64_t offset = static_cast<std::uint64_t>(entry.extent_lba) * kSectorSize;
        std::uint64_t remaining = entry.size;
        std::array<std::uint8_t, 64U * 1024U> buffer{};
        while (remaining > 0U) {
            const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), remaining));
            reader_.read(offset + static_cast<std::uint64_t>(entry.size) - remaining,
                         std::span<std::uint8_t>(buffer.data(), chunk));
            output.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(chunk));
            if (!output) {
                fail("failed while writing extracted file: " + target.string());
            }
            remaining -= static_cast<std::uint64_t>(chunk);
        }
    }

    Reader reader_;
    std::vector<DirectoryEntry> entries_;
    std::set<std::string> visited_directories_;
    std::uint64_t directory_bytes_scanned_{};
    std::size_t path_bytes_scanned_{};
};

} // namespace

InspectionReport inspectIso(const std::filesystem::path& image_path) {
    return Inspector(image_path).run();
}

InspectionReport readIsoDirectory(const std::filesystem::path& image_path) {
    return Inspector(image_path).run(false, false);
}

void extractIsoContents(const std::filesystem::path& image_path,
                        const std::filesystem::path& destination_path) {
    Inspector(image_path).extractTo(destination_path);
}

} // namespace ps2iso
