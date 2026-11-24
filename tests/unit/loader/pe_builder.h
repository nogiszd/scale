#pragma once

// Test-only builder for small, valid PE32+ AMD64 images laid out from the PE
// format specification. Tests corrupt individual fields afterwards (using the
// offset constants below) to exercise each rejection path.
//
// Fixed layout:
//   0x000  DOS header (e_lfanew = 0x40)
//   0x040  "PE\0\0"
//   0x044  COFF file header (20 bytes)
//   0x058  PE32+ optional header (240 bytes, 16 data directories)
//   0x148  section table (40 bytes per section)
//   SizeOfHeaders = section table end rounded up to FileAlignment (0x200)
// Sections get consecutive VAs from 0x1000 (SectionAlignment 0x1000) and
// consecutive raw data aligned to 0x200.

#include "scale/loader/pe_image.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <variant>
#include <vector>

namespace scale::test {

using Bytes = std::vector<std::byte>;

inline constexpr std::size_t kFileHeaderOffset = 0x44;
inline constexpr std::size_t kOptionalHeaderOffset = 0x58;
inline constexpr std::size_t kOptionalHeaderSize = 240;
inline constexpr std::size_t kSectionTableOffset = kOptionalHeaderOffset + kOptionalHeaderSize;  // 0x148
inline constexpr std::size_t kSectionHeaderSize = 40;

// Offsets of individual fields (absolute file offsets).
inline constexpr std::size_t kOffMachine = kFileHeaderOffset + 0;
inline constexpr std::size_t kOffNumberOfSections = kFileHeaderOffset + 2;
inline constexpr std::size_t kOffSizeOfOptionalHeader = kFileHeaderOffset + 16;
inline constexpr std::size_t kOffCharacteristics = kFileHeaderOffset + 18;
inline constexpr std::size_t kOffMagic = kOptionalHeaderOffset + 0;
inline constexpr std::size_t kOffEntryPoint = kOptionalHeaderOffset + 16;
inline constexpr std::size_t kOffSectionAlignment = kOptionalHeaderOffset + 32;
inline constexpr std::size_t kOffFileAlignment = kOptionalHeaderOffset + 36;
inline constexpr std::size_t kOffSizeOfImage = kOptionalHeaderOffset + 56;
inline constexpr std::size_t kOffSizeOfHeaders = kOptionalHeaderOffset + 60;
inline constexpr std::size_t kOffNumberOfRvaAndSizes = kOptionalHeaderOffset + 108;
inline constexpr std::size_t kOffDirectories = kOptionalHeaderOffset + 112;

inline std::size_t section_field(std::size_t index, std::size_t field_offset) {
    return kSectionTableOffset + index * kSectionHeaderSize + field_offset;
}
inline constexpr std::size_t kSecVirtualSize = 8;
inline constexpr std::size_t kSecVirtualAddress = 12;
inline constexpr std::size_t kSecRawSize = 16;
inline constexpr std::size_t kSecRawPointer = 20;

template <class T>
void put(Bytes& b, std::size_t offset, T value) {
    if (b.size() < offset + sizeof(T)) {
        b.resize(offset + sizeof(T));
    }
    std::memcpy(b.data() + offset, &value, sizeof(T));
}
inline void put_u16(Bytes& b, std::size_t o, std::uint16_t v) { put(b, o, v); }
inline void put_u32(Bytes& b, std::size_t o, std::uint32_t v) { put(b, o, v); }
inline void put_u64(Bytes& b, std::size_t o, std::uint64_t v) { put(b, o, v); }
inline void put_str(Bytes& b, std::size_t o, const std::string& s) {
    if (b.size() < o + s.size()) {
        b.resize(o + s.size());
    }
    std::memcpy(b.data() + o, s.data(), s.size());
}

constexpr std::uint32_t align_up(std::uint32_t v, std::uint32_t a) {
    return (v + a - 1) / a * a;
}

struct BuilderSection {
    std::string name;
    Bytes data;
    std::uint32_t virtual_size = 0;  // 0 = data size
    std::uint32_t characteristics = 0;
};

class PeBuilder {
public:
    static constexpr std::uint32_t kSectionAlignment = 0x1000;
    static constexpr std::uint32_t kFileAlignment = 0x200;
    static constexpr std::uint32_t kText = 0x60000020;   // code | execute | read
    static constexpr std::uint32_t kRdata = 0x40000040;  // initialized data | read
    static constexpr std::uint32_t kData = 0xC0000040;   // initialized data | read | write

    std::uint16_t characteristics = 0x0022;  // EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
    std::uint64_t image_base = 0x140000000ull;
    std::uint32_t entry_rva = 0;
    std::uint16_t subsystem = 3;  // console
    std::uint16_t dll_characteristics = 0x8160;
    std::array<loader::DataDirectory, loader::kDirectoryCount> directories = {};

    // RVA the next added section will get.
    [[nodiscard]] std::uint32_t next_section_rva() const {
        std::uint32_t rva = kSectionAlignment;
        for (const BuilderSection& s : sections_) {
            rva += align_up(std::max<std::uint32_t>(extent(s), 1), kSectionAlignment);
        }
        return rva;
    }

    std::uint32_t add_section(std::string name, Bytes data, std::uint32_t section_characteristics,
                              std::uint32_t virtual_size = 0) {
        const std::uint32_t rva = next_section_rva();
        sections_.push_back(BuilderSection{std::move(name), std::move(data), virtual_size, section_characteristics});
        return rva;
    }

    void set_directory(loader::DirectoryIndex index, std::uint32_t rva, std::uint32_t size) {
        directories[static_cast<std::size_t>(index)] = loader::DataDirectory{rva, size};
    }

    [[nodiscard]] std::uint32_t size_of_headers() const {
        return align_up(static_cast<std::uint32_t>(kSectionTableOffset + kSectionHeaderSize * sections_.size()),
                        kFileAlignment);
    }

    [[nodiscard]] Bytes build() const {
        const std::uint32_t headers = size_of_headers();
        Bytes b(headers, std::byte{0});

        put_u16(b, 0, 0x5A4D);       // "MZ"
        put_u32(b, 0x3C, 0x40);      // e_lfanew
        put_u32(b, 0x40, 0x00004550);  // "PE\0\0"

        put_u16(b, kOffMachine, loader::kMachineAmd64);
        put_u16(b, kOffNumberOfSections, static_cast<std::uint16_t>(sections_.size()));
        put_u16(b, kOffSizeOfOptionalHeader, static_cast<std::uint16_t>(kOptionalHeaderSize));
        put_u16(b, kOffCharacteristics, characteristics);

        const std::size_t oh = kOptionalHeaderOffset;
        put_u16(b, oh + 0, loader::kOptionalMagicPe32Plus);
        b[oh + 2] = std::byte{14};  // linker version 14.0
        put_u32(b, oh + 16, entry_rva);
        put_u64(b, oh + 24, image_base);
        put_u32(b, oh + 32, kSectionAlignment);
        put_u32(b, oh + 36, kFileAlignment);
        put_u16(b, oh + 40, 6);  // OS version 6.0
        put_u16(b, oh + 48, 6);  // subsystem version 6.0
        put_u32(b, oh + 56, next_section_rva());  // SizeOfImage
        put_u32(b, oh + 60, headers);
        put_u16(b, oh + 68, subsystem);
        put_u16(b, oh + 70, dll_characteristics);
        put_u64(b, oh + 72, 0x100000);
        put_u64(b, oh + 80, 0x1000);
        put_u64(b, oh + 88, 0x100000);
        put_u64(b, oh + 96, 0x1000);
        put_u32(b, oh + 108, static_cast<std::uint32_t>(loader::kDirectoryCount));
        for (std::size_t i = 0; i < loader::kDirectoryCount; ++i) {
            put_u32(b, kOffDirectories + i * 8, directories[i].rva);
            put_u32(b, kOffDirectories + i * 8 + 4, directories[i].size);
        }

        std::uint32_t rva = kSectionAlignment;
        for (std::size_t i = 0; i < sections_.size(); ++i) {
            const BuilderSection& s = sections_[i];
            const auto raw_size = align_up(static_cast<std::uint32_t>(s.data.size()), kFileAlignment);
            const auto raw_offset = static_cast<std::uint32_t>(b.size());
            const std::size_t h = kSectionTableOffset + i * kSectionHeaderSize;
            put_str(b, h, s.name.substr(0, 8));
            put_u32(b, h + 8, extent(s));
            put_u32(b, h + 12, rva);
            put_u32(b, h + 16, raw_size);
            put_u32(b, h + 20, raw_size != 0 ? raw_offset : 0);
            put_u32(b, h + 36, s.characteristics);
            if (raw_size != 0) {
                b.insert(b.end(), s.data.begin(), s.data.end());
                b.resize(raw_offset + raw_size, std::byte{0});
            }
            rva += align_up(std::max<std::uint32_t>(extent(s), 1), kSectionAlignment);
        }
        return b;
    }

private:
    static std::uint32_t extent(const BuilderSection& s) {
        return s.virtual_size != 0 ? s.virtual_size : static_cast<std::uint32_t>(s.data.size());
    }

    std::vector<BuilderSection> sections_;
};

// ---- Import section helper --------------------------------------------------------

struct ImportSpec {
    std::string dll;
    // std::string = import by name (hint 0x10 + index); std::uint16_t = by ordinal
    std::vector<std::variant<std::string, std::uint16_t>> symbols;
};

struct ImportSection {
    Bytes data;
    std::uint32_t directory_size = 0;  // descriptor array including the terminator
};

// Lays out descriptors, lookup tables, address tables, hint/name entries and
// DLL names for a section that will be placed at `rva`.
inline ImportSection build_import_section(std::uint32_t rva, const std::vector<ImportSpec>& modules) {
    ImportSection out;
    Bytes& b = out.data;
    const auto descriptors_size = static_cast<std::uint32_t>((modules.size() + 1) * 20);
    out.directory_size = descriptors_size;
    std::uint32_t cursor = descriptors_size;

    for (std::size_t m = 0; m < modules.size(); ++m) {
        const ImportSpec& mod = modules[m];
        const auto table_size = static_cast<std::uint32_t>((mod.symbols.size() + 1) * 8);
        const std::uint32_t ilt = cursor;
        const std::uint32_t iat = ilt + table_size;
        cursor = iat + table_size;

        for (std::size_t s = 0; s < mod.symbols.size(); ++s) {
            std::uint64_t thunk = 0;
            if (const auto* ordinal = std::get_if<std::uint16_t>(&mod.symbols[s])) {
                thunk = (1ull << 63) | *ordinal;
            } else {
                const std::string& name = std::get<std::string>(mod.symbols[s]);
                put_u16(b, cursor, static_cast<std::uint16_t>(0x10 + s));
                put_str(b, cursor + 2, name);
                put(b, cursor + 2 + name.size(), std::uint8_t{0});
                thunk = rva + cursor;
                cursor = align_up(static_cast<std::uint32_t>(cursor + 2 + name.size() + 1), 2);
            }
            put_u64(b, ilt + s * 8, thunk);
            put_u64(b, iat + s * 8, thunk);
        }
        put_u64(b, ilt + mod.symbols.size() * 8, 0);
        put_u64(b, iat + mod.symbols.size() * 8, 0);

        const std::uint32_t name_offset = cursor;
        put_str(b, name_offset, mod.dll);
        put(b, name_offset + mod.dll.size(), std::uint8_t{0});
        cursor = align_up(static_cast<std::uint32_t>(name_offset + mod.dll.size() + 1), 2);

        const std::size_t d = m * 20;
        put_u32(b, d + 0, rva + ilt);
        put_u32(b, d + 12, rva + name_offset);
        put_u32(b, d + 16, rva + iat);
    }
    put_u32(b, modules.size() * 20 + 16, 0);  // ensure the terminator exists
    b.resize(std::max<std::size_t>(b.size(), cursor));
    return out;
}

}  // namespace scale::test
