// T-202: SCALE loader mapping. These tests map images and check memory
// state; they never execute mapped code (execution happens only in the
// worker, see tests/integration/native_execution_test.cpp).

#include "scale/loader/image_map.h"

#include "pe_builder.h"

#include <gtest/gtest.h>

#include <windows.h>

#include <cstring>

using namespace scale::loader;
using scale::test::Bytes;
using scale::test::PeBuilder;
using scale::test::put_u16;
using scale::test::put_u32;
using scale::test::put_u64;

namespace {

// mov eax, 42 ; ret
Bytes ret42_code() {
    return Bytes{std::byte{0xB8}, std::byte{0x2A}, std::byte{0x00}, std::byte{0x00},
                 std::byte{0x00}, std::byte{0xC3}};
}

PeBuilder code_image() {
    PeBuilder pe;
    pe.entry_rva = pe.add_section(".text", ret42_code(), PeBuilder::kText);
    return pe;
}

struct Built {
    Bytes bytes;
    PeImage image;
};

Built build(const PeBuilder& pe) {
    Built b{pe.build(), {}};
    auto r = inspect_pe(scale::ByteView(b.bytes));
    EXPECT_TRUE(r.has_value()) << r.error().to_string();
    if (r) b.image = *r;
    return b;
}

scale::Result<MappedImage> map(const Built& b) {
    return map_image(scale::ByteView(b.bytes), b.image);
}

void expect_error(const PeBuilder& pe, std::string_view code) {
    const Built b = build(pe);
    const auto r = map(b);
    ASSERT_FALSE(r.has_value()) << "expected loader/" << code;
    EXPECT_EQ(r.error().code, code) << r.error().to_string();
}

DWORD protection_at(std::uintptr_t address) {
    MEMORY_BASIC_INFORMATION mbi{};
    EXPECT_NE(::VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)), 0u);
    return mbi.Protect;
}

}  // namespace

TEST(ImageMap, MapsSectionsWithProtections) {
    PeBuilder pe = code_image();
    const std::uint32_t rdata = pe.add_section(".rdata", Bytes(16, std::byte{0x11}), PeBuilder::kRdata);
    const std::uint32_t data = pe.add_section(".data", Bytes(16, std::byte{0x22}), PeBuilder::kData);
    const Built b = build(pe);

    auto m = map(b);
    ASSERT_TRUE(m.has_value()) << m.error().to_string();
    EXPECT_EQ(m->size(), b.image.size_of_image);
    EXPECT_EQ(m->entry_point(), m->base() + 0x1000);
    EXPECT_EQ(m->relocation_delta(), static_cast<std::int64_t>(m->base() - b.image.image_base));

    EXPECT_EQ(protection_at(m->base()), static_cast<DWORD>(PAGE_READONLY));  // headers
    EXPECT_EQ(protection_at(m->base() + 0x1000), static_cast<DWORD>(PAGE_EXECUTE_READ));
    EXPECT_EQ(protection_at(m->base() + rdata), static_cast<DWORD>(PAGE_READONLY));
    EXPECT_EQ(protection_at(m->base() + data), static_cast<DWORD>(PAGE_READWRITE));

    EXPECT_EQ(std::memcmp(reinterpret_cast<const void*>(m->base() + 0x1000), ret42_code().data(), 6), 0);
    EXPECT_EQ(*reinterpret_cast<const std::uint8_t*>(m->base() + data), 0x22u);
    // Bytes past the section's raw data are zero.
    EXPECT_EQ(*reinterpret_cast<const std::uint8_t*>(m->base() + data + 16), 0u);
}

TEST(ImageMap, AppliesDir64Relocations) {
    PeBuilder pe = code_image();
    const std::uint32_t data_rva = pe.next_section_rva();
    Bytes data;
    put_u64(data, 0, pe.image_base + data_rva + 8);  // absolute pointer to data+8
    put_u64(data, 8, 0x1234);
    pe.add_section(".data", data, PeBuilder::kData);
    Bytes reloc;
    put_u32(reloc, 0, data_rva);
    put_u32(reloc, 4, 12);
    put_u16(reloc, 8, static_cast<std::uint16_t>((10u << 12) | 0));
    put_u16(reloc, 10, 0);
    const std::uint32_t reloc_rva = pe.add_section(".reloc", reloc, PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::base_relocation_table, reloc_rva, 12);
    const Built b = build(pe);

    auto m = map(b);
    ASSERT_TRUE(m.has_value()) << m.error().to_string();
    EXPECT_NE(m->relocation_delta(), 0);  // relocatable images are always moved
    EXPECT_EQ(m->relocations_applied(), 1u);
    const auto pointer = *reinterpret_cast<const std::uint64_t*>(m->base() + data_rva);
    EXPECT_EQ(pointer, m->base() + data_rva + 8);
    EXPECT_EQ(*reinterpret_cast<const std::uint64_t*>(pointer), 0x1234u);
}

TEST(ImageMap, ReleasesMemoryOnDestruction) {
    const Built b = build(code_image());
    std::uintptr_t base = 0;
    {
        auto m = map(b);
        ASSERT_TRUE(m.has_value());
        base = m->base();
    }
    MEMORY_BASIC_INFORMATION mbi{};
    ASSERT_NE(::VirtualQuery(reinterpret_cast<void*>(base), &mbi, sizeof(mbi)), 0u);
    EXPECT_EQ(mbi.State, static_cast<DWORD>(MEM_FREE));
}

TEST(ImageMapReject, Dll) {
    PeBuilder pe = code_image();
    pe.characteristics |= kFileDll;
    expect_error(pe, "unsupported_image");
}

TEST(ImageMapReject, SmallSectionAlignment) {
    PeBuilder pe = code_image();
    Bytes bytes = pe.build();
    put_u32(bytes, scale::test::kOffSectionAlignment, 0x200);
    const auto image = inspect_pe(scale::ByteView(bytes));
    ASSERT_TRUE(image.has_value()) << image.error().to_string();
    const auto r = map_image(scale::ByteView(bytes), *image);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, "unsupported_image");
}

TEST(ImageMapReject, UnsupportedDirectories) {
    for (const auto index : {DirectoryIndex::import_table, DirectoryIndex::tls_table, DirectoryIndex::exception_table,
                             DirectoryIndex::load_config_table, DirectoryIndex::delay_import_descriptor,
                             DirectoryIndex::export_table}) {
        PeBuilder pe = code_image();
        const std::uint32_t rva = pe.add_section(".rdata", Bytes(0x100, std::byte{0}), PeBuilder::kRdata);
        // Valid contents for the directories the inspector parses.
        pe.set_directory(index, rva, index == DirectoryIndex::exception_table ? 12 : 40);
        const Built b = build(pe);
        const auto r = map(b);
        ASSERT_FALSE(r.has_value()) << directory_name(index);
        EXPECT_EQ(r.error().code, "unsupported_directory") << directory_name(index);
        EXPECT_NE(r.error().message.find(directory_name(index)), std::string::npos);
    }
}

TEST(ImageMapReject, HighLowRelocation) {
    PeBuilder pe = code_image();
    Bytes reloc;
    put_u32(reloc, 0, 0x1000);
    put_u32(reloc, 4, 12);
    put_u16(reloc, 8, static_cast<std::uint16_t>(3u << 12));  // HIGHLOW
    put_u16(reloc, 10, 0);
    const std::uint32_t rva = pe.add_section(".reloc", reloc, PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::base_relocation_table, rva, 12);
    expect_error(pe, "unsupported_relocation");
}

TEST(ImageMapReject, WritableExecutableSection) {
    PeBuilder pe;
    pe.entry_rva = pe.add_section(".wx", ret42_code(), PeBuilder::kText | kSectionMemWrite);
    expect_error(pe, "writable_executable_section");
}

TEST(ImageMapReject, EntryPointNotExecutable) {
    PeBuilder none = code_image();
    none.entry_rva = 0;
    expect_error(none, "entry_not_executable");

    PeBuilder data_entry = code_image();
    data_entry.entry_rva = data_entry.add_section(".data", Bytes(16, std::byte{0}), PeBuilder::kData);
    expect_error(data_entry, "entry_not_executable");
}

TEST(ImageMapReject, FixedImageWhosePreferredBaseIsTaken) {
    PeBuilder pe = code_image();
    // The test executable's own base is certainly occupied.
    pe.image_base = reinterpret_cast<std::uint64_t>(::GetModuleHandleW(nullptr));
    expect_error(pe, "relocation_required");
}

TEST(ReadFile, MissingFile) {
    const auto r = read_file(std::filesystem::temp_directory_path() / "scale_missing_executable.exe");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, "file_open_failed");
}
