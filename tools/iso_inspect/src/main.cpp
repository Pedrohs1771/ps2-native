#include "iso_inspector.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

void printUsage(const char* executable) {
    std::cerr << "Usage: " << executable << " [--json] <game.iso>\n"
              << "       " << executable << " extract <game.iso> <new-destination>\n"
              << "Inspect ISO9660 paths, PS2 SYSTEM.CNF/BOOT2, and the boot ELF.\n";
}

std::string permissions(std::uint32_t flags) {
    std::string result;
    result.push_back((flags & 4U) != 0U ? 'R' : '-');
    result.push_back((flags & 2U) != 0U ? 'W' : '-');
    result.push_back((flags & 1U) != 0U ? 'X' : '-');
    return result;
}

std::string hex32(std::uint32_t value) {
    std::ostringstream output;
    output << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return output.str();
}

std::string hex64(std::uint64_t value) {
    std::ostringstream output;
    output << "0x" << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << value;
    return output.str();
}

std::string elfTypeName(std::uint16_t type) {
    switch (type) {
    case 0U: return "ET_NONE";
    case 1U: return "ET_REL";
    case 2U: return "ET_EXEC";
    case 3U: return "ET_DYN";
    case 4U: return "ET_CORE";
    default: return "ET_OTHER";
    }
}

std::string jsonString(std::string_view value) {
    std::ostringstream output;
    output << '"';
    for (std::size_t index = 0U; index < value.size();) {
        const auto byte = static_cast<unsigned char>(value[index]);
        if (byte == '"') {
            output << "\\\"";
            ++index;
        } else if (byte == '\\') {
            output << "\\\\";
            ++index;
        } else if (byte == '\b') {
            output << "\\b";
            ++index;
        } else if (byte == '\f') {
            output << "\\f";
            ++index;
        } else if (byte == '\n') {
            output << "\\n";
            ++index;
        } else if (byte == '\r') {
            output << "\\r";
            ++index;
        } else if (byte == '\t') {
            output << "\\t";
            ++index;
        } else if (byte < 0x20U) {
            output << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                   << static_cast<unsigned int>(byte) << std::dec;
            ++index;
        } else if (byte < 0x80U) {
            output << static_cast<char>(byte);
            ++index;
        } else {
            std::size_t sequenceLength = 0U;
            std::uint32_t codePoint = 0U;
            std::uint32_t minimum = 0U;
            if ((byte & 0xe0U) == 0xc0U) {
                sequenceLength = 2U;
                codePoint = byte & 0x1fU;
                minimum = 0x80U;
            } else if ((byte & 0xf0U) == 0xe0U) {
                sequenceLength = 3U;
                codePoint = byte & 0x0fU;
                minimum = 0x800U;
            } else if ((byte & 0xf8U) == 0xf0U) {
                sequenceLength = 4U;
                codePoint = byte & 0x07U;
                minimum = 0x10000U;
            }
            bool valid = sequenceLength != 0U && index + sequenceLength <= value.size();
            if (valid) {
                for (std::size_t offset = 1U; offset < sequenceLength; ++offset) {
                    const auto continuation = static_cast<unsigned char>(value[index + offset]);
                    if ((continuation & 0xc0U) != 0x80U) {
                        valid = false;
                        break;
                    }
                    codePoint = (codePoint << 6U) | (continuation & 0x3fU);
                }
                valid = valid && codePoint >= minimum && codePoint <= 0x10ffffU &&
                        !(codePoint >= 0xd800U && codePoint <= 0xdfffU);
            }
            if (valid) {
                output.write(value.data() + static_cast<std::ptrdiff_t>(index),
                             static_cast<std::streamsize>(sequenceLength));
                index += sequenceLength;
            } else {
                output << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<unsigned int>(byte) << std::dec;
                ++index;
            }
        }
    }
    output << '"';
    return output.str();
}

void printJson(const ps2iso::InspectionReport& report, const std::filesystem::path& imagePath) {
    std::cout << "{\n"
              << "  \"schema_version\": 1,\n"
              << "  \"image\": {\n"
              << "    \"path\": " << jsonString(imagePath.string()) << ",\n"
              << "    \"size_bytes\": " << report.image_size << ",\n"
              << "    \"filesystem\": \"ISO9660\",\n"
              << "    \"volume_id\": " << jsonString(report.volume_id) << ",\n"
              << "    \"logical_block_size\": " << report.logical_block_size << ",\n"
              << "    \"joliet_names\": " << (report.used_joliet_names ? "true" : "false") << ",\n"
              << "    \"sha256\": " << jsonString(report.iso_sha256) << "\n"
              << "  },\n"
              << "  \"entries\": [\n";
    for (std::size_t index = 0U; index < report.entries.size(); ++index) {
        const auto& entry = report.entries[index];
        std::cout << "    {\"path\": " << jsonString(entry.path)
                  << ", \"kind\": " << jsonString(entry.is_directory ? "directory" : "file")
                  << ", \"size_bytes\": " << entry.size
                  << ", \"extent_lba\": " << entry.extent_lba << "}"
                  << (index + 1U == report.entries.size() ? "\n" : ",\n");
    }
    std::cout << "  ],\n"
              << "  \"elf_inventory\": {\n"
              << "    \"schema_version\": 1,\n"
              << "    \"deduplicated_by\": \"sha256\",\n"
              << "    \"items\": [\n";
    for (std::size_t index = 0U; index < report.elf_inventory.size(); ++index) {
        const auto& item = report.elf_inventory[index];
        std::cout << "      {\"id\": " << jsonString(item.id)
                  << ", \"sha256\": " << jsonString(item.sha256)
                  << ", \"size_bytes\": " << item.size_bytes
                  << ", \"elf_class\": " << (item.elf_class == 1U ? 32U : item.elf_class == 2U ? 64U : 0U)
                  << ", \"byte_order\": " << jsonString(item.data_encoding == 1U ? "little-endian" : item.data_encoding == 2U ? "big-endian" : "unknown")
                  << ", \"type_id\": " << item.type
                  << ", \"type\": " << jsonString(elfTypeName(item.type))
                  << ", \"machine_id\": " << item.machine
                  << ", \"flags\": " << jsonString(hex32(item.flags))
                  << ", \"entrypoint\": " << jsonString(hex64(item.entrypoint))
                  << ", \"role\": " << jsonString(item.role)
                  << ", \"compile_status\": " << jsonString(item.compile_status)
                  << ", \"reason\": " << jsonString(item.reason)
                  << ", \"paths\": [";
        for (std::size_t pathIndex = 0U; pathIndex < item.iso_paths.size(); ++pathIndex) {
            if (pathIndex != 0U) {
                std::cout << ", ";
            }
            std::cout << jsonString(item.iso_paths[pathIndex]);
        }
        std::cout << "], \"load_segments\": [";
        for (std::size_t segmentIndex = 0U; segmentIndex < item.load_segments.size(); ++segmentIndex) {
            const auto& segment = item.load_segments[segmentIndex];
            if (segmentIndex != 0U) {
                std::cout << ", ";
            }
            std::cout << "{\"file_offset\": " << jsonString(hex32(segment.file_offset))
                      << ", \"virtual_address\": " << jsonString(hex32(segment.virtual_address))
                      << ", \"physical_address\": " << jsonString(hex32(segment.physical_address))
                      << ", \"file_size_bytes\": " << segment.file_size
                      << ", \"memory_size_bytes\": " << segment.memory_size
                      << ", \"flags\": " << segment.flags
                      << ", \"alignment\": " << segment.alignment << "}";
        }
        std::cout << "]}"
                  << (index + 1U == report.elf_inventory.size() ? "\n" : ",\n");
    }
    std::cout << "    ]\n"
              << "  },\n"
              << "  \"boot\": {\n"
              << "    \"system_cnf\": {\n"
              << "      \"path\": " << jsonString(report.boot.system_cnf_path) << ",\n"
              << "      \"boot2\": " << jsonString(report.boot.boot2) << ",\n"
              << "      \"version\": " << jsonString(report.boot.version) << ",\n"
              << "      \"video_mode\": " << jsonString(report.boot.video_mode) << ",\n"
              << "      \"region_inferred\": " << jsonString(report.boot.region) << "\n"
              << "    },\n"
              << "    \"elf\": {\n"
              << "      \"path\": " << jsonString(report.boot.elf.path) << ",\n"
              << "      \"format\": \"ELF32\",\n"
              << "      \"byte_order\": \"little-endian\",\n"
              << "      \"machine\": \"MIPS R5900\",\n"
              << "      \"machine_id\": " << report.boot.elf.machine << ",\n"
              << "      \"entrypoint\": " << jsonString(hex32(report.boot.elf.entrypoint)) << ",\n"
              << "      \"sha256\": " << jsonString(report.boot.elf.sha256) << ",\n"
              << "      \"load_segments\": [\n";
    for (std::size_t index = 0U; index < report.boot.elf.segments.size(); ++index) {
        const auto& segment = report.boot.elf.segments[index];
        std::cout << "        {\"file_offset\": " << jsonString(hex32(segment.file_offset))
                  << ", \"virtual_address\": " << jsonString(hex32(segment.virtual_address))
                  << ", \"physical_address\": " << jsonString(hex32(segment.physical_address))
                  << ", \"file_size_bytes\": " << segment.file_size
                  << ", \"memory_size_bytes\": " << segment.memory_size
                  << ", \"flags\": " << segment.flags
                  << ", \"permissions\": " << jsonString(permissions(segment.flags))
                  << ", \"alignment\": " << segment.alignment << "}"
                  << (index + 1U == report.boot.elf.segments.size() ? "\n" : ",\n");
    }
    std::cout << "      ]\n"
              << "    }\n"
              << "  }\n"
              << "}\n";
}

void printHuman(const ps2iso::InspectionReport& report, const std::filesystem::path& imagePath) {
    std::cout << "Image: " << imagePath.string() << '\n'
              << "Size: " << report.image_size << " bytes\n"
              << "Filesystem: ISO9660" << (report.used_joliet_names ? " (Joliet names)" : "") << '\n'
              << "Volume ID: " << report.volume_id << '\n'
              << "Logical block size: " << report.logical_block_size << " bytes\n"
              << "ISO SHA-256: " << report.iso_sha256 << "\n\n"
              << "Tree (" << report.entries.size() << " entries):\n"
              << "/\n";
    for (const auto& entry : report.entries) {
        std::cout << "  " << entry.path << (entry.is_directory ? "/" : "")
                  << (entry.is_directory ? " [dir]" : " [" + std::to_string(entry.size) + " bytes]")
                  << '\n';
    }

    std::cout << "\nSYSTEM.CNF: " << report.boot.system_cnf_path << '\n'
              << "BOOT2: " << report.boot.boot2 << '\n'
              << "Boot ELF path: " << report.boot.elf.path << '\n'
              << "VER: " << (report.boot.version.empty() ? "(not present)" : report.boot.version) << '\n'
              << "VMODE: " << (report.boot.video_mode.empty() ? "(not present)" : report.boot.video_mode) << '\n'
              << "Region: " << report.boot.region << '\n'
              << "ELF: ELF32 little-endian, MIPS R5900 (EM_MIPS)\n"
              << "ELF entrypoint: " << hex32(report.boot.elf.entrypoint) << '\n'
              << "ELF SHA-256: " << report.boot.elf.sha256 << '\n'
              << "PT_LOAD segments (" << report.boot.elf.segments.size() << "):\n";
    for (std::size_t index = 0U; index < report.boot.elf.segments.size(); ++index) {
        const auto& segment = report.boot.elf.segments[index];
        std::cout << "  [" << index << "] file=" << hex32(segment.file_offset)
                  << " vaddr=" << hex32(segment.virtual_address)
                  << " paddr=" << hex32(segment.physical_address)
                  << " filesz=" << segment.file_size << " memsz=" << segment.memory_size
                  << " flags=" << permissions(segment.flags) << " align=" << segment.alignment << '\n';
    }
    std::cout << "\nELF inventory (" << report.elf_inventory.size() << " unique contents):\n";
    for (const auto& item : report.elf_inventory) {
        std::cout << "  " << item.role << " | " << elfTypeName(item.type)
                  << " | machine=" << item.machine
                  << " | " << item.compile_status
                  << " | " << (item.iso_paths.empty() ? "<unknown path>" : item.iso_paths.front())
                  << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 4 && std::string_view(argv[1]) == "extract") {
        try {
            ps2iso::extractIsoContents(argv[2], argv[3]);
            std::cout << "Extraction complete: " << argv[3] << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "Error: " << error.what() << '\n';
            return 1;
        }
    }

    bool json = false;
    std::filesystem::path imagePath;
    if (argc == 2) {
        imagePath = argv[1];
    } else if (argc == 3 && std::string_view(argv[1]) == "--json") {
        json = true;
        imagePath = argv[2];
    } else {
        printUsage(argv[0]);
        return 2;
    }

    try {
        const auto report = ps2iso::inspectIso(imagePath);
        if (json) {
            printJson(report, imagePath);
        } else {
            printHuman(report, imagePath);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
