#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ps2iso {

struct DirectoryEntry {
    std::string path;
    std::uint32_t extent_lba{};
    std::uint32_t size{};
    bool is_directory{};
};

struct LoadSegment {
    std::uint32_t file_offset{};
    std::uint32_t virtual_address{};
    std::uint32_t physical_address{};
    std::uint32_t file_size{};
    std::uint32_t memory_size{};
    std::uint32_t flags{};
    std::uint32_t alignment{};
};

struct ElfReport {
    std::string path;
    std::string sha256;
    std::uint16_t machine{};
    std::uint32_t entrypoint{};
    std::vector<LoadSegment> segments;
};

struct ElfInventoryItem {
    std::string id;
    std::string sha256;
    std::vector<std::string> iso_paths;
    std::uint64_t size_bytes{};
    std::uint8_t elf_class{};
    std::uint8_t data_encoding{};
    std::uint16_t type{};
    std::uint16_t machine{};
    std::uint32_t flags{};
    std::uint64_t entrypoint{};
    std::vector<LoadSegment> load_segments;
    std::string role;
    std::string compile_status;
    std::string reason;
};

struct BootReport {
    std::string system_cnf_path;
    std::string boot2;
    std::string version;
    std::string video_mode;
    std::string region;
    ElfReport elf;
};

struct InspectionReport {
    std::uint64_t image_size{};
    std::uint16_t logical_block_size{};
    std::string volume_id;
    std::string iso_sha256;
    bool used_joliet_names{};
    std::vector<DirectoryEntry> entries;
    BootReport boot;
    std::vector<ElfInventoryItem> elf_inventory;
};

// Inspects a local, unencrypted ISO9660 image and its PS2 BOOT2 ELF.
// Throws std::runtime_error with a user-readable error for malformed or unsupported input.
InspectionReport inspectIso(const std::filesystem::path& image_path);

// Reads validated directory extents without hashing the disc or requiring a
// PS2 boot executable. Intended for runtime sector/file lookup.
InspectionReport readIsoDirectory(const std::filesystem::path& image_path);

// Extracts a validated ISO tree into a destination directory that does not yet exist.
// ISO ;version suffixes are removed from host names; path collisions abort before writing.
void extractIsoContents(const std::filesystem::path& image_path,
                        const std::filesystem::path& destination_path);

} // namespace ps2iso
