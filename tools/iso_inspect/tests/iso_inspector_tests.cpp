#include "iso_inspector.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;
constexpr std::size_t kSector = 2048;
constexpr std::uint32_t kSectorCount = 26U;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void put16le(Bytes& bytes, std::size_t offset, std::uint16_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value & 0xffU);
    bytes.at(offset + 1U) = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
}

void put32le(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned int index = 0; index < 4U; ++index) {
        bytes.at(offset + index) = static_cast<std::uint8_t>((value >> (index * 8U)) & 0xffU);
    }
}

void put32be(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned int index = 0; index < 4U; ++index) {
        bytes.at(offset + index) = static_cast<std::uint8_t>((value >> ((3U - index) * 8U)) & 0xffU);
    }
}

void putBoth16(Bytes& bytes, std::size_t offset, std::uint16_t value) {
    put16le(bytes, offset, value);
    bytes.at(offset + 2U) = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes.at(offset + 3U) = static_cast<std::uint8_t>(value & 0xffU);
}

void putBoth32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    put32le(bytes, offset, value);
    put32be(bytes, offset + 4U, value);
}

void putText(Bytes& bytes, std::size_t offset, std::size_t length, const std::string& text) {
    for (std::size_t index = 0; index < length; ++index) {
        bytes.at(offset + index) = index < text.size() ? static_cast<std::uint8_t>(text[index]) : static_cast<std::uint8_t>(' ');
    }
}

void putJolietText(Bytes& bytes, std::size_t offset, std::size_t byteLength, const std::string& text) {
    for (std::size_t index = 0; index < byteLength / 2U; ++index) {
        const char value = index < text.size() ? text[index] : ' ';
        bytes.at(offset + index * 2U) = 0U;
        bytes.at(offset + index * 2U + 1U) = static_cast<std::uint8_t>(value);
    }
}

std::vector<std::uint8_t> identifier(const std::string& text, bool joliet) {
    std::vector<std::uint8_t> result;
    for (const char character : text) {
        const auto value = static_cast<std::uint8_t>(static_cast<unsigned char>(character));
        if (joliet) {
            result.push_back(0U);
        }
        result.push_back(value);
    }
    return result;
}

Bytes directoryRecord(std::uint32_t lba, std::uint32_t size, std::uint8_t flags,
                      const std::vector<std::uint8_t>& name) {
    const std::size_t padding = (name.size() % 2U == 0U) ? 1U : 0U;
    Bytes record(33U + name.size() + padding, 0U);
    record[0] = static_cast<std::uint8_t>(record.size());
    putBoth32(record, 2U, lba);
    putBoth32(record, 10U, size);
    record[25] = flags;
    putBoth16(record, 28U, 1U);
    record[32] = static_cast<std::uint8_t>(name.size());
    for (std::size_t index = 0; index < name.size(); ++index) {
        record[33U + index] = name[index];
    }
    return record;
}

void append(Bytes& target, const Bytes& source) {
    target.insert(target.end(), source.begin(), source.end());
}

Bytes syntheticElf() {
    Bytes elf(0x120U, 0U);
    elf[0] = 0x7fU;
    elf[1] = 'E';
    elf[2] = 'L';
    elf[3] = 'F';
    elf[4] = 1U; // ELF32
    elf[5] = 1U; // little-endian
    elf[6] = 1U; // ELF version
    put16le(elf, 16U, 2U);
    put16le(elf, 18U, 8U); // MIPS
    put32le(elf, 20U, 1U);
    put32le(elf, 24U, 0x00100000U);
    put32le(elf, 28U, 52U);
    put16le(elf, 40U, 52U);
    put16le(elf, 42U, 32U);
    put16le(elf, 44U, 1U);

    put32le(elf, 52U, 1U); // PT_LOAD
    put32le(elf, 56U, 0x100U);
    put32le(elf, 60U, 0x00100000U);
    put32le(elf, 64U, 0x00100000U);
    put32le(elf, 68U, 0x20U);
    put32le(elf, 72U, 0x40U);
    put32le(elf, 76U, 5U); // R + X
    put32le(elf, 80U, 0x10U);
    for (std::size_t index = 0x100U; index < elf.size(); ++index) {
        elf[index] = static_cast<std::uint8_t>(index & 0xffU);
    }
    return elf;
}

Bytes syntheticIso(bool malformedElf = false, bool includeSystemCnf = true, bool useJoliet = false) {
    Bytes iso(static_cast<std::size_t>(kSectorCount) * kSector, 0U);

    const std::size_t pvd = 16U * kSector;
    iso[pvd] = 1U;
    putText(iso, pvd + 1U, 5U, "CD001");
    iso[pvd + 6U] = 1U;
    putText(iso, pvd + 40U, 32U, "SYNTHETIC_PS2");
    putBoth32(iso, pvd + 80U, kSectorCount);
    putBoth16(iso, pvd + 120U, 1U);
    putBoth16(iso, pvd + 124U, 1U);
    putBoth16(iso, pvd + 128U, static_cast<std::uint16_t>(kSector));
    const auto rootRecord = directoryRecord(20U, static_cast<std::uint32_t>(kSector), 2U, {0U});
    std::copy(rootRecord.begin(), rootRecord.end(), iso.begin() + static_cast<std::ptrdiff_t>(pvd + 156U));

    std::size_t terminatorLba = 17U;
    if (useJoliet) {
        const std::size_t svd = 17U * kSector;
        iso[svd] = 2U;
        putText(iso, svd + 1U, 5U, "CD001");
        iso[svd + 6U] = 1U;
        putJolietText(iso, svd + 40U, 32U, "SYNTHETIC_PS2");
        putBoth32(iso, svd + 80U, kSectorCount);
        putBoth16(iso, svd + 120U, 1U);
        putBoth16(iso, svd + 124U, 1U);
        putBoth16(iso, svd + 128U, static_cast<std::uint16_t>(kSector));
        iso[svd + 88U] = 0x25U;
        iso[svd + 89U] = 0x2fU;
        iso[svd + 90U] = 0x45U;
        std::copy(rootRecord.begin(), rootRecord.end(), iso.begin() + static_cast<std::ptrdiff_t>(svd + 156U));
        terminatorLba = 18U;
    }

    const std::size_t terminator = terminatorLba * kSector;
    iso[terminator] = 255U;
    putText(iso, terminator + 1U, 5U, "CD001");
    iso[terminator + 6U] = 1U;

    Bytes root;
    append(root, directoryRecord(20U, static_cast<std::uint32_t>(kSector), 2U, {0U}));
    append(root, directoryRecord(20U, static_cast<std::uint32_t>(kSector), 2U, {1U}));
    if (includeSystemCnf) {
        const std::string config = "BOOT2 = cdrom0:\\SLPM_551.08;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n";
        append(root, directoryRecord(21U, static_cast<std::uint32_t>(config.size()), 0U,
                                     identifier("SYSTEM.CNF;1", useJoliet)));
    }
    const Bytes elf = syntheticElf();
    append(root, directoryRecord(22U, static_cast<std::uint32_t>(elf.size()), 0U,
                                 identifier("SLPM_551.08;1", useJoliet)));
    append(root, directoryRecord(23U, static_cast<std::uint32_t>(kSector), 2U,
                                 identifier("DATA", useJoliet)));
    std::copy(root.begin(), root.end(), iso.begin() + static_cast<std::ptrdiff_t>(20U * kSector));

    if (includeSystemCnf) {
        const std::string config = "BOOT2 = cdrom0:\\SLPM_551.08;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n";
        std::copy(config.begin(), config.end(), iso.begin() + static_cast<std::ptrdiff_t>(21U * kSector));
    }
    Bytes elfImage = syntheticElf();
    if (malformedElf) {
        put32le(elfImage, 56U, 0xfffffff0U); // PT_LOAD file offset outside ELF
    }
    std::copy(elfImage.begin(), elfImage.end(), iso.begin() + static_cast<std::ptrdiff_t>(22U * kSector));

    Bytes nested;
    append(nested, directoryRecord(23U, static_cast<std::uint32_t>(kSector), 2U, {0U}));
    append(nested, directoryRecord(20U, static_cast<std::uint32_t>(kSector), 2U, {1U}));
    const std::string note = "fixture";
    append(nested, directoryRecord(24U, static_cast<std::uint32_t>(note.size()), 0U,
                                   identifier("README.TXT;1", useJoliet)));
    std::copy(nested.begin(), nested.end(), iso.begin() + static_cast<std::ptrdiff_t>(23U * kSector));
    std::copy(note.begin(), note.end(), iso.begin() + static_cast<std::ptrdiff_t>(24U * kSector));
    return iso;
}

Bytes isoWithVersionCollision() {
    Bytes iso = syntheticIso();
    std::size_t position = 20U * kSector;
    while (position < 21U * kSector && iso[position] != 0U) {
        position += iso[position];
    }
    const std::string config = "BOOT2 = cdrom0:\\SLPM_551.08;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n";
    const auto duplicate = directoryRecord(21U, static_cast<std::uint32_t>(config.size()), 0U,
                                           {'S','Y','S','T','E','M','.','C','N','F',';','2'});
    if (position + duplicate.size() > 21U * kSector) {
        throw std::runtime_error("synthetic collision entry does not fit in root directory");
    }
    std::copy(duplicate.begin(), duplicate.end(), iso.begin() + static_cast<std::ptrdiff_t>(position));
    return iso;
}

Bytes isoWithTraversalLikeName() {
    Bytes iso = syntheticIso();
    std::size_t position = 20U * kSector;
    while (position < 21U * kSector && iso[position] != 0U) {
        position += iso[position];
    }
    const auto suspicious = directoryRecord(25U, 1U, 0U, {'.', '.', ';', '1'});
    std::copy(suspicious.begin(), suspicious.end(), iso.begin() + static_cast<std::ptrdiff_t>(position));
    iso[25U * kSector] = 'x';
    return iso;
}

std::filesystem::path writeFixture(const Bytes& bytes, const std::string& label) {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      ("ps2iso_" + label + "_" + std::to_string(nonce) + ".iso");
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw std::runtime_error("failed to write synthetic ISO fixture");
    }
    return path;
}

Bytes readFileBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to read extracted fixture file");
    }
    input.seekg(0, std::ios::end);
    const auto length = input.tellg();
    if (length < 0) {
        throw std::runtime_error("failed to determine extracted fixture size");
    }
    input.seekg(0, std::ios::beg);
    Bytes result(static_cast<std::size_t>(length));
    input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
    if (!input && !result.empty()) {
        throw std::runtime_error("short read from extracted fixture file");
    }
    return result;
}

void testReadsTreeBootConfigAndMipsElf() {
    const auto path = writeFixture(syntheticIso(), "valid");
    try {
        const auto report = ps2iso::inspectIso(path);
        require(report.volume_id == "SYNTHETIC_PS2", "volume id should be read from the PVD");
        require(report.logical_block_size == 2048U, "logical block size should be reported");
        require(report.entries.size() == 4U, "root and nested entries should be enumerated without dot entries");
        require(report.entries[0].path == "/SYSTEM.CNF;1", "SYSTEM.CNF path should be listed");
        require(report.entries[1].path == "/SLPM_551.08;1", "boot ELF path should be listed");
        require(report.entries[2].path == "/DATA", "directory path should be listed");
        require(report.entries[2].is_directory, "directory entry should be marked as a directory");
        require(report.entries[3].path == "/DATA/README.TXT;1", "nested file path should be listed");
        require(report.boot.system_cnf_path == "/SYSTEM.CNF;1", "SYSTEM.CNF should be located");
        require(report.boot.boot2 == "cdrom0:\\SLPM_551.08;1", "BOOT2 should be parsed verbatim");
        require(report.boot.version == "1.00", "VER should be parsed");
        require(report.boot.video_mode == "NTSC", "VMODE should be parsed");
        require(report.boot.region == "Japan (inferred from SLPM)", "region should be inferred from serial prefix");
        require(report.boot.elf.path == "/SLPM_551.08;1", "BOOT2 should resolve to the ELF path");
        require(report.boot.elf.machine == 8U, "ELF machine should be MIPS");
        require(report.boot.elf.entrypoint == 0x00100000U, "ELF entrypoint should be parsed");
        require(report.boot.elf.segments.size() == 1U, "PT_LOAD segment should be parsed");
        require(report.boot.elf.segments[0].file_offset == 0x100U, "ELF segment file offset should be parsed");
        require(report.boot.elf.segments[0].file_size == 0x20U, "ELF segment file size should be parsed");
        require(report.boot.elf.segments[0].memory_size == 0x40U, "ELF segment memory size should be parsed");
        require(report.iso_sha256 == "1fe60cdc9d71236b202c594582388362205c3e6e68f3428b1d35c51629ff5c40",
                "ISO SHA-256 should match an independently generated fixture digest");
        require(report.boot.elf.sha256 == "e6f753cba55fc548a59c3fb43a177f34180c3e3565cab927b4ee32b1c302c3c3",
                "ELF SHA-256 should match the independent known digest");
    } catch (...) {
        std::filesystem::remove(path);
        throw;
    }
    std::filesystem::remove(path);
}

void testDirectoryIndexDoesNotRequireBootOrHashing() {
    const auto path = writeFixture(syntheticIso(false, false), "directory-only");
    try {
        const auto report = ps2iso::readIsoDirectory(path);
        require(report.logical_block_size == 2048U, "directory index must validate sector size");
        require(!report.entries.empty(), "directory index must retain physical extents");
        require(report.iso_sha256.empty() && report.elf_inventory.empty(),
                "directory index must skip whole-disc hashes and executable scanning");
        require(report.boot.boot2.empty(), "directory index must not require boot metadata");
    } catch (...) {
        std::filesystem::remove(path);
        throw;
    }
    std::filesystem::remove(path);
}

void testReadsJolietNamesAndMetadata() {
    const auto path = writeFixture(syntheticIso(false, true, true), "joliet");
    try {
        const auto report = ps2iso::inspectIso(path);
        require(report.used_joliet_names, "Joliet supplementary volume descriptor should be selected");
        require(report.volume_id == "SYNTHETIC_PS2", "Joliet volume id should be decoded from UCS-2BE");
        require(report.entries[0].path == "/SYSTEM.CNF;1", "Joliet ASCII paths should be decoded");
        require(report.entries[3].path == "/DATA/README.TXT;1", "nested Joliet names should be decoded");
        require(report.boot.elf.sha256 == "e6f753cba55fc548a59c3fb43a177f34180c3e3565cab927b4ee32b1c302c3c3",
                "Joliet boot ELF hash should match the fixture");
    } catch (...) {
        std::filesystem::remove(path);
        throw;
    }
    std::filesystem::remove(path);
}

void testRejectsBootElfSegmentOutsideFile() {
    const auto path = writeFixture(syntheticIso(true), "bad_elf");
    bool rejected = false;
    try {
        (void)ps2iso::inspectIso(path);
    } catch (const std::exception& error) {
        rejected = std::string(error.what()).find("segment") != std::string::npos;
    }
    std::filesystem::remove(path);
    require(rejected, "ELF with a PT_LOAD range outside the file should be rejected");
}

void testRejectsMissingSystemCnf() {
    const auto path = writeFixture(syntheticIso(false, false), "missing_cnf");
    bool rejected = false;
    try {
        (void)ps2iso::inspectIso(path);
    } catch (const std::exception& error) {
        rejected = std::string(error.what()).find("SYSTEM.CNF") != std::string::npos;
    }
    std::filesystem::remove(path);
    require(rejected, "ISO without SYSTEM.CNF should fail with a useful message");
}

void testRejectsTruncatedDescriptorSet() {
    const auto bytes = Bytes(10U, 0U);
    const auto path = writeFixture(bytes, "truncated");
    bool rejected = false;
    try {
        (void)ps2iso::inspectIso(path);
    } catch (const std::exception&) {
        rejected = true;
    }
    std::filesystem::remove(path);
    require(rejected, "truncated image should be rejected");
}

void testExtractsTreeAndRemovesIsoVersionSuffixes() {
    const auto image = writeFixture(syntheticIso(), "extract_ok");
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto parent = std::filesystem::temp_directory_path() / ("ps2iso_extract_" + std::to_string(nonce));
    const auto destination = parent / "contents";
    std::filesystem::create_directory(parent);
    try {
        ps2iso::extractIsoContents(image, destination);
        require(std::filesystem::is_directory(destination / "DATA"), "extract should preserve directories");
        require(std::filesystem::is_regular_file(destination / "SYSTEM.CNF"),
                "extract should remove ;version from SYSTEM.CNF host name");
        require(std::filesystem::is_regular_file(destination / "SLPM_551.08"),
                "extract should remove ;version from boot ELF host name");
        require(std::filesystem::is_regular_file(destination / "DATA" / "README.TXT"),
                "extract should preserve nested file path and remove its version suffix");
        require(readFileBytes(destination / "SLPM_551.08") == syntheticElf(),
                "extracted boot ELF bytes should match their ISO extent");
        const auto note = readFileBytes(destination / "DATA" / "README.TXT");
        require(std::string(note.begin(), note.end()) == "fixture", "nested file content should be copied exactly");

        bool rejectedExisting = false;
        try {
            ps2iso::extractIsoContents(image, destination);
        } catch (const std::exception& error) {
            rejectedExisting = std::string(error.what()).find("already exists") != std::string::npos;
        }
        require(rejectedExisting, "extract should refuse to overwrite an existing destination");
        require(std::filesystem::is_regular_file(destination / "SYSTEM.CNF"),
                "existing destination contents should remain untouched");
    } catch (...) {
        std::filesystem::remove_all(parent);
        std::filesystem::remove(image);
        throw;
    }
    std::filesystem::remove_all(parent);
    std::filesystem::remove(image);
}

void testRejectsCollisionAfterRemovingVersionSuffix() {
    const auto image = writeFixture(isoWithVersionCollision(), "extract_collision");
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto parent = std::filesystem::temp_directory_path() / ("ps2iso_collision_" + std::to_string(nonce));
    const auto destination = parent / "contents";
    std::filesystem::create_directory(parent);
    bool rejected = false;
    try {
        ps2iso::extractIsoContents(image, destination);
    } catch (const std::exception& error) {
        rejected = std::string(error.what()).find("collision") != std::string::npos;
    }
    require(rejected, ";1 and ;2 paths should collide after host-name normalization");
    require(!std::filesystem::exists(destination), "collision should be rejected before creating destination");
    std::filesystem::remove_all(parent);
    std::filesystem::remove(image);
}

void testRejectsTraversalAfterVersionNormalization() {
    const auto image = writeFixture(isoWithTraversalLikeName(), "extract_traversal");
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto parent = std::filesystem::temp_directory_path() / ("ps2iso_traversal_" + std::to_string(nonce));
    const auto destination = parent / "contents";
    std::filesystem::create_directory(parent);
    bool rejected = false;
    try {
        ps2iso::extractIsoContents(image, destination);
    } catch (const std::exception& error) {
        rejected = std::string(error.what()).find("unsafe") != std::string::npos;
    }
    require(rejected, "version removal must not turn an ISO name into a traversal component");
    require(!std::filesystem::exists(destination), "unsafe path should be rejected before creating destination");
    require(!std::filesystem::exists(parent / "x"), "unsafe path should not create a sibling outside destination");
    std::filesystem::remove_all(parent);
    std::filesystem::remove(image);
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--write-fixture") {
        const auto bytes = syntheticIso();
        std::ofstream output(argv[2], std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return output ? 0 : 2;
    }
    if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << " [--write-fixture <path>]\n";
        return 2;
    }
    try {
        testReadsTreeBootConfigAndMipsElf();
        testDirectoryIndexDoesNotRequireBootOrHashing();
        testReadsJolietNamesAndMetadata();
        testRejectsBootElfSegmentOutsideFile();
        testRejectsMissingSystemCnf();
        testRejectsTruncatedDescriptorSet();
        testExtractsTreeAndRemovesIsoVersionSuffixes();
        testRejectsCollisionAfterRemovingVersionSuffix();
        testRejectsTraversalAfterVersionNormalization();
        std::cout << "All ISO inspector tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
