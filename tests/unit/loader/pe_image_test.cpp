#include "scale/loader/pe_image.h"

#include "pe_builder.h"

#include <gtest/gtest.h>

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

using namespace scale::loader;
using scale::test::align_up;
using scale::test::Bytes;
using scale::test::ImportSpec;
using scale::test::PeBuilder;
using scale::test::put_u16;
using scale::test::put_u32;
using scale::test::put_u64;
using scale::test::section_field;

namespace {

Bytes filled(std::size_t size, std::uint8_t value = 0xCC) {
    return Bytes(size, std::byte{value});
}

scale::Result<PeImage> inspect(const Bytes& b) {
    return inspect_pe(scale::ByteView(b));
}

// A minimal executable: one .text section, entry point at its start.
PeBuilder minimal_builder() {
    PeBuilder pe;
    pe.entry_rva = pe.add_section(".text", filled(0x30), PeBuilder::kText);
    return pe;
}

Bytes minimal_image() {
    return minimal_builder().build();
}

// Asserts that inspection fails with the given loader error code.
void expect_error(const Bytes& b, std::string_view code) {
    const auto r = inspect(b);
    ASSERT_FALSE(r.has_value()) << "expected loader/" << code << " but inspection succeeded";
    EXPECT_EQ(r.error().component, "loader");
    EXPECT_EQ(r.error().code, code) << r.error().to_string();
}

}  // namespace

// ---- Positive cases -------------------------------------------------------------

TEST(PeInspect, MinimalImage) {
    const auto r = inspect(minimal_image());
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    const PeImage& img = *r;
    EXPECT_EQ(img.machine, kMachineAmd64);
    EXPECT_EQ(img.magic, kOptionalMagicPe32Plus);
    EXPECT_FALSE(img.is_dll());
    EXPECT_EQ(img.image_base, 0x140000000ull);
    EXPECT_EQ(img.address_of_entry_point, 0x1000u);
    EXPECT_EQ(img.section_alignment, 0x1000u);
    EXPECT_EQ(img.file_alignment, 0x200u);
    EXPECT_EQ(img.size_of_image, 0x2000u);
    EXPECT_EQ(img.size_of_headers, 0x200u);
    EXPECT_EQ(img.subsystem, 3u);
    EXPECT_EQ(img.dll_characteristics, 0x8160u);
    EXPECT_EQ(img.number_of_rva_and_sizes, 16u);
    EXPECT_EQ(img.major_linker_version, 14u);

    ASSERT_EQ(img.sections.size(), 1u);
    const Section& text = img.sections[0];
    EXPECT_EQ(text.name, ".text");
    EXPECT_EQ(text.virtual_address, 0x1000u);
    EXPECT_EQ(text.virtual_size, 0x30u);
    EXPECT_EQ(text.raw_offset, 0x200u);
    EXPECT_EQ(text.raw_size, 0x200u);
    EXPECT_EQ(text.characteristics, PeBuilder::kText);

    for (const DataDirectory& d : img.directories) {
        EXPECT_FALSE(d.present());
    }
    EXPECT_TRUE(img.imports.empty());
    EXPECT_FALSE(img.relocations.has_value());
    EXPECT_FALSE(img.tls.has_value());
    EXPECT_FALSE(img.exceptions.has_value());
}

TEST(PeInspect, DllFlagAndFewerDirectories) {
    PeBuilder pe = minimal_builder();
    pe.characteristics |= kFileDll;
    Bytes b = pe.build();
    put_u32(b, scale::test::kOffNumberOfRvaAndSizes, 6);  // directories 6..15 are ignored
    put_u32(b, scale::test::kOffDirectories + 9 * 8, 0x1000);  // TLS entry outside the count
    const auto r = inspect(b);
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    EXPECT_TRUE(r->is_dll());
    EXPECT_EQ(r->number_of_rva_and_sizes, 6u);
    EXPECT_FALSE(r->directory(DirectoryIndex::tls_table).present());
}

TEST(PeInspect, ImportsByNameAndOrdinal) {
    PeBuilder pe = minimal_builder();
    const std::uint32_t rva = pe.next_section_rva();
    const auto idata = scale::test::build_import_section(
        rva, {ImportSpec{"KERNEL32.dll", {std::string("ExitProcess"), std::string("GetStdHandle")}},
              ImportSpec{"fixture_platform.dll", {std::uint16_t{7}}}});
    pe.add_section(".idata", idata.data, PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::import_table, rva, idata.directory_size);

    const auto r = inspect(pe.build());
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    ASSERT_EQ(r->imports.size(), 2u);

    const ImportModule& k32 = r->imports[0];
    EXPECT_EQ(k32.dll_name, "KERNEL32.dll");
    EXPECT_NE(k32.import_lookup_table_rva, 0u);
    EXPECT_NE(k32.import_address_table_rva, 0u);
    ASSERT_EQ(k32.entries.size(), 2u);
    EXPECT_FALSE(k32.entries[0].by_ordinal);
    EXPECT_EQ(k32.entries[0].name, "ExitProcess");
    EXPECT_EQ(k32.entries[0].hint, 0x10u);
    EXPECT_EQ(k32.entries[1].name, "GetStdHandle");
    EXPECT_EQ(k32.entries[1].hint, 0x11u);

    const ImportModule& plat = r->imports[1];
    EXPECT_EQ(plat.dll_name, "fixture_platform.dll");
    ASSERT_EQ(plat.entries.size(), 1u);
    EXPECT_TRUE(plat.entries[0].by_ordinal);
    EXPECT_EQ(plat.entries[0].ordinal, 7u);
}

TEST(PeInspect, ImportsFallBackToIatWithoutLookupTable) {
    PeBuilder pe = minimal_builder();
    const std::uint32_t rva = pe.next_section_rva();
    auto idata = scale::test::build_import_section(rva, {ImportSpec{"KERNEL32.dll", {std::string("ExitProcess")}}});
    put_u32(idata.data, 0, 0);  // OriginalFirstThunk = 0
    pe.add_section(".idata", idata.data, PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::import_table, rva, idata.directory_size);
    const auto r = inspect(pe.build());
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    ASSERT_EQ(r->imports.size(), 1u);
    EXPECT_EQ(r->imports[0].import_lookup_table_rva, 0u);
    ASSERT_EQ(r->imports[0].entries.size(), 1u);
    EXPECT_EQ(r->imports[0].entries[0].name, "ExitProcess");
}

TEST(PeInspect, BaseRelocations) {
    PeBuilder pe = minimal_builder();
    Bytes reloc;
    put_u32(reloc, 0, 0x1000);              // page RVA
    put_u32(reloc, 4, 12);                  // block size: header + 2 entries
    put_u16(reloc, 8, (10u << 12) | 0x010);  // DIR64 at 0x1010
    put_u16(reloc, 10, 0);                   // ABSOLUTE padding
    const std::uint32_t rva = pe.add_section(".reloc", reloc, PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::base_relocation_table, rva, 12);

    const auto r = inspect(pe.build());
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    ASSERT_TRUE(r->relocations.has_value());
    EXPECT_EQ(r->relocations->block_count, 1u);
    EXPECT_EQ(r->relocations->entry_count, 2u);
    EXPECT_EQ(r->relocations->count_by_type[10], 1u);
    EXPECT_EQ(r->relocations->count_by_type[0], 1u);
}

TEST(PeInspect, TlsDirectoryWithCallbacks) {
    PeBuilder pe = minimal_builder();
    const std::uint32_t rva = pe.next_section_rva();
    const std::uint64_t base = pe.image_base;
    Bytes tls;
    put_u64(tls, 0, base + rva + 0x80);   // StartAddressOfRawData
    put_u64(tls, 8, base + rva + 0x90);   // EndAddressOfRawData
    put_u64(tls, 16, base + rva + 0xA0);  // AddressOfIndex
    put_u64(tls, 24, base + rva + 0x40);  // AddressOfCallBacks
    put_u32(tls, 32, 0x10);               // SizeOfZeroFill
    put_u64(tls, 0x40, base + 0x1000);    // callback 0 -> .text
    put_u64(tls, 0x48, base + 0x1010);    // callback 1
    put_u64(tls, 0x50, 0);                // terminator
    put_u64(tls, 0xA0, 0);
    pe.add_section(".tls", tls, PeBuilder::kData);
    pe.set_directory(DirectoryIndex::tls_table, rva, 40);

    const auto r = inspect(pe.build());
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    ASSERT_TRUE(r->tls.has_value());
    EXPECT_EQ(r->tls->address_of_callbacks, base + rva + 0x40);
    EXPECT_EQ(r->tls->size_of_zero_fill, 0x10u);
    ASSERT_EQ(r->tls->callback_rvas.size(), 2u);
    EXPECT_EQ(r->tls->callback_rvas[0], 0x1000u);
    EXPECT_EQ(r->tls->callback_rvas[1], 0x1010u);
}

TEST(PeInspect, ExceptionAndDelayImportDirectories) {
    PeBuilder pe = minimal_builder();
    const std::uint32_t pdata = pe.add_section(".pdata", filled(24, 0), PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::exception_table, pdata, 24);
    pe.set_directory(DirectoryIndex::delay_import_descriptor, pdata, 8);  // presence only

    const auto r = inspect(pe.build());
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    ASSERT_TRUE(r->exceptions.has_value());
    EXPECT_EQ(r->exceptions->runtime_function_count, 2u);
    EXPECT_TRUE(r->directory(DirectoryIndex::delay_import_descriptor).present());
    EXPECT_EQ(directory_name(DirectoryIndex::delay_import_descriptor), "delay_import_descriptor");
}

TEST(PeInspect, RealSystemDll) {
    // Read-only inspection of an MSVC-built system image; it is never loaded.
    wchar_t dir[MAX_PATH];
    const UINT len = ::GetSystemDirectoryW(dir, MAX_PATH);
    ASSERT_GT(len, 0u);
    ASSERT_LT(len, static_cast<UINT>(MAX_PATH));
    const auto r = inspect_pe_file(std::filesystem::path(dir) / L"kernel32.dll");
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    EXPECT_EQ(r->machine, kMachineAmd64);
    EXPECT_TRUE(r->is_dll());
    EXPECT_TRUE(r->directory(DirectoryIndex::export_table).present());
    EXPECT_FALSE(r->imports.empty());
    EXPECT_TRUE(r->exceptions.has_value());
    EXPECT_TRUE(r->relocations.has_value());
}

// ---- File access ------------------------------------------------------------------

namespace {
std::filesystem::path temp_file(std::string_view tag) {
    static std::atomic<int> counter{0};
    return std::filesystem::temp_directory_path() /
           std::format("scale_unit_loader_{}_{}_{}.bin", ::GetCurrentProcessId(), tag, counter++);
}
}  // namespace

TEST(PeInspectFile, ReadsFile) {
    const Bytes b = minimal_image();
    const auto path = temp_file("ok");
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    }
    const auto r = inspect_pe_file(path);
    std::filesystem::remove(path);
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    EXPECT_EQ(r->sections.size(), 1u);
}

TEST(PeInspectFile, RejectsOversizedFile) {
    const Bytes b = minimal_image();
    const auto path = temp_file("big");
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    }
    const auto r = inspect_pe_file(path, b.size() - 1);
    std::filesystem::remove(path);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, "file_too_large");
}

TEST(PeInspectFile, MissingFile) {
    const auto r = inspect_pe_file(temp_file("missing"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, "file_open_failed");
}

// ---- Header rejection ---------------------------------------------------------------

TEST(PeReject, TruncatedDosHeader) {
    expect_error(Bytes(63, std::byte{0}), "truncated_dos_header");
}

TEST(PeReject, BadDosMagic) {
    Bytes b = minimal_image();
    b[0] = std::byte{'Z'};
    expect_error(b, "bad_dos_magic");
}

TEST(PeReject, BadPeOffset) {
    Bytes b = minimal_image();
    put_u32(b, 0x3C, static_cast<std::uint32_t>(b.size() - 2));
    expect_error(b, "bad_pe_offset");
    put_u32(b, 0x3C, 0xFFFFFFFFu);
    expect_error(b, "bad_pe_offset");
}

TEST(PeReject, BadPeSignature) {
    Bytes b = minimal_image();
    b[0x41] = std::byte{'X'};
    expect_error(b, "bad_pe_signature");
}

TEST(PeReject, TruncatedFileHeader) {
    Bytes b = minimal_image();
    b.resize(scale::test::kFileHeaderOffset + 10);
    expect_error(b, "truncated_file_header");
}

TEST(PeReject, UnsupportedMachine) {
    Bytes b = minimal_image();
    put_u16(b, scale::test::kOffMachine, 0x014C);  // i386
    expect_error(b, "unsupported_machine");
}

TEST(PeReject, ObjectFileIsNotAnImage) {
    Bytes b = minimal_image();
    put_u16(b, scale::test::kOffCharacteristics, 0x0020);
    expect_error(b, "not_an_image");
}

TEST(PeReject, TruncatedOptionalHeader) {
    Bytes b = minimal_image();
    b.resize(scale::test::kOptionalHeaderOffset + 100);
    expect_error(b, "truncated_optional_header");
}

TEST(PeReject, Pe32IsUnsupported) {
    Bytes b = minimal_image();
    put_u16(b, scale::test::kOffMagic, kOptionalMagicPe32);
    expect_error(b, "unsupported_optional_header");
}

TEST(PeReject, UnknownOptionalMagic) {
    Bytes b = minimal_image();
    put_u16(b, scale::test::kOffMagic, 0x1234);
    expect_error(b, "bad_optional_header_magic");
}

TEST(PeReject, OptionalHeaderTooSmall) {
    Bytes b = minimal_image();
    put_u16(b, scale::test::kOffSizeOfOptionalHeader, 100);
    expect_error(b, "optional_header_too_small");

    Bytes c = minimal_image();
    put_u16(c, scale::test::kOffSizeOfOptionalHeader, 112 + 8 * 8);  // 16 directories declared
    expect_error(c, "optional_header_too_small");
}

TEST(PeReject, TooManyDirectories) {
    Bytes b = minimal_image();
    put_u32(b, scale::test::kOffNumberOfRvaAndSizes, 17);
    expect_error(b, "bad_directory_count");
}

TEST(PeReject, BadAlignment) {
    Bytes b = minimal_image();
    put_u32(b, scale::test::kOffFileAlignment, 0x300);  // not a power of two
    expect_error(b, "bad_alignment");

    Bytes c = minimal_image();
    put_u32(c, scale::test::kOffSectionAlignment, 0x100);  // smaller than FileAlignment
    expect_error(c, "bad_alignment");
}

TEST(PeReject, BadSizeOfImage) {
    Bytes b = minimal_image();
    put_u32(b, scale::test::kOffSizeOfImage, 0x2100);
    expect_error(b, "bad_size_of_image");
}

TEST(PeReject, TruncatedSectionTable) {
    Bytes b = minimal_image();
    put_u16(b, scale::test::kOffNumberOfSections, 50);
    expect_error(b, "truncated_section_table");
}

TEST(PeReject, BadSizeOfHeaders) {
    Bytes b = minimal_image();
    put_u32(b, scale::test::kOffSizeOfHeaders, 0x100);  // smaller than the headers
    expect_error(b, "bad_size_of_headers");

    Bytes c = minimal_image();
    put_u32(c, scale::test::kOffSizeOfHeaders, 0x10000);  // larger than the file
    expect_error(c, "bad_size_of_headers");
}

TEST(PeReject, EntryPointOutOfImage) {
    Bytes b = minimal_image();
    put_u32(b, scale::test::kOffEntryPoint, 0x2000);
    expect_error(b, "entry_point_out_of_image");
}

// ---- Section rejection -----------------------------------------------------------------

TEST(PeReject, SectionMisaligned) {
    Bytes b = minimal_image();
    put_u32(b, section_field(0, scale::test::kSecVirtualAddress), 0x1100);
    expect_error(b, "section_misaligned");

    Bytes c = minimal_image();
    put_u32(c, section_field(0, scale::test::kSecRawPointer), 0x210);
    expect_error(c, "section_misaligned");
}

TEST(PeReject, SectionsOverlap) {
    PeBuilder pe = minimal_builder();
    pe.add_section(".data", filled(0x10), PeBuilder::kData);
    Bytes b = pe.build();
    put_u32(b, section_field(1, scale::test::kSecVirtualAddress), 0x1000);
    expect_error(b, "sections_overlap");

    Bytes c = minimal_image();
    put_u32(c, section_field(0, scale::test::kSecVirtualAddress), 0);  // overlaps headers
    expect_error(c, "sections_overlap");
}

TEST(PeReject, SectionOutOfImage) {
    Bytes b = minimal_image();
    put_u32(b, section_field(0, scale::test::kSecVirtualSize), 0x1001);
    expect_error(b, "section_out_of_image");
}

TEST(PeReject, SectionOutOfFile) {
    Bytes b = minimal_image();
    put_u32(b, section_field(0, scale::test::kSecRawSize), 0x400);
    expect_error(b, "section_out_of_file");
}

TEST(PeReject, TooManySections) {
    Bytes b = minimal_image();
    put_u16(b, scale::test::kOffNumberOfSections, kMaxSections + 1);
    expect_error(b, "limit_exceeded");
}

// ---- Directory rejection -----------------------------------------------------------------

TEST(PeReject, DirectoryOutOfImage) {
    PeBuilder pe = minimal_builder();
    pe.set_directory(DirectoryIndex::resource_table, 0x1FF8, 0x10);
    expect_error(pe.build(), "directory_out_of_image");

    PeBuilder wrap = minimal_builder();
    wrap.set_directory(DirectoryIndex::debug, 0xFFFFFFF0u, 0x20);  // rva + size wraps in 32 bits
    expect_error(wrap.build(), "directory_out_of_image");
}

TEST(PeReject, CertificateOutOfFile) {
    PeBuilder pe = minimal_builder();
    pe.set_directory(DirectoryIndex::certificate_table, 0x400, 0x10);  // file is 0x400 bytes
    expect_error(pe.build(), "directory_out_of_file");
}

TEST(PeReject, DirectoryInZeroFillIsNotInFile) {
    PeBuilder pe = minimal_builder();
    // 0x10 bytes of file data, 0x800 bytes in memory: the tail is zero-fill.
    const std::uint32_t rva = pe.add_section(".bss", filled(0x10), PeBuilder::kData, 0x800);
    pe.set_directory(DirectoryIndex::base_relocation_table, rva + 0x400, 8);
    expect_error(pe.build(), "rva_not_in_file");
}

// ---- Import rejection ----------------------------------------------------------------------

namespace {
struct ImportFixture {
    PeBuilder pe = minimal_builder();
    std::uint32_t rva = 0;
    scale::test::ImportSection idata;

    explicit ImportFixture(std::vector<ImportSpec> modules) {
        rva = pe.next_section_rva();
        idata = scale::test::build_import_section(rva, modules);
    }
    Bytes build(std::uint32_t virtual_size = 0) {
        pe.add_section(".idata", idata.data, PeBuilder::kRdata, virtual_size);
        pe.set_directory(DirectoryIndex::import_table, rva, idata.directory_size);
        return pe.build();
    }
};
}  // namespace

TEST(PeReject, ImportDescriptorWithoutName) {
    ImportFixture f({ImportSpec{"KERNEL32.dll", {std::string("ExitProcess")}}});
    put_u32(f.idata.data, 12, 0);  // Name = 0
    expect_error(f.build(), "import_table_malformed");
}

TEST(PeReject, ImportOrdinalReservedBits) {
    ImportFixture f({ImportSpec{"fixture_platform.dll", {std::uint16_t{1}}}});
    // ILT starts right after the two descriptors (offset 40).
    put_u64(f.idata.data, 40, (1ull << 63) | (1ull << 20) | 1);
    expect_error(f.build(), "import_table_malformed");
}

TEST(PeReject, ImportNameThunkReservedBits) {
    ImportFixture f({ImportSpec{"KERNEL32.dll", {std::string("ExitProcess")}}});
    put_u64(f.idata.data, 40, 1ull << 40);
    expect_error(f.build(), "import_table_malformed");
}

TEST(PeReject, ImportThunkTableNotBacked) {
    ImportFixture f({ImportSpec{"KERNEL32.dll", {std::string("ExitProcess")}}});
    put_u32(f.idata.data, 0, 0x7000);  // ILT RVA outside every section
    expect_error(f.build(), "import_table_malformed");
}

TEST(PeReject, ImportNameNotTerminated) {
    // The DLL name runs to the very end of the section's file data.
    PeBuilder pe = minimal_builder();
    const std::uint32_t rva = pe.next_section_rva();
    Bytes data(0x200, std::byte{'A'});
    for (std::size_t i = 0; i < 0x40; ++i) data[i] = std::byte{0};
    put_u32(data, 0, rva + 0x20);       // ILT -> zero terminator at 0x20
    put_u32(data, 12, rva + 0x100);     // Name: 'A' * 0x100, no NUL before the end
    put_u32(data, 16, rva + 0x20);      // IAT
    pe.add_section(".idata", data, PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::import_table, rva, 40);
    expect_error(pe.build(), "import_table_malformed");
}

TEST(PeReject, ImportNameTooLong) {
    PeBuilder pe = minimal_builder();
    const std::uint32_t rva = pe.next_section_rva();
    Bytes data(0x1400, std::byte{'A'});
    for (std::size_t i = 0; i < 0x40; ++i) data[i] = std::byte{0};
    put_u32(data, 0, rva + 0x20);
    put_u32(data, 12, rva + 0x40);  // 0x13C0 bytes of 'A' > kMaxNameLength
    put_u32(data, 16, rva + 0x20);
    data.back() = std::byte{0};
    pe.add_section(".idata", data, PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::import_table, rva, 40);
    expect_error(pe.build(), "limit_exceeded");
}

TEST(PeReject, TooManyImportDescriptors) {
    // kMaxImportModules + 1 identical, valid descriptors with no terminator in range.
    PeBuilder pe = minimal_builder();
    const std::uint32_t rva = pe.next_section_rva();
    const std::size_t count = kMaxImportModules + 1;
    const std::uint32_t tail = static_cast<std::uint32_t>(count * 20);
    Bytes data;
    put_u64(data, tail, 0);             // shared empty ILT
    scale::test::put_str(data, tail + 8, "x.dll");
    scale::test::put(data, tail + 13, std::uint8_t{0});
    for (std::size_t i = 0; i < count; ++i) {
        put_u32(data, i * 20 + 0, rva + tail);
        put_u32(data, i * 20 + 12, rva + tail + 8);
        put_u32(data, i * 20 + 16, rva + tail);
    }
    pe.add_section(".idata", data, PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::import_table, rva, 20);
    expect_error(pe.build(), "limit_exceeded");
}

// ---- Relocation, TLS and exception rejection ---------------------------------------------

TEST(PeReject, RelocationBlockSize) {
    PeBuilder pe = minimal_builder();
    Bytes reloc;
    put_u32(reloc, 0, 0x1000);
    put_u32(reloc, 4, 6);  // smaller than the 8-byte header
    const std::uint32_t rva = pe.add_section(".reloc", reloc, PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::base_relocation_table, rva, 8);
    expect_error(pe.build(), "relocation_table_malformed");

    PeBuilder overrun = minimal_builder();
    Bytes r2;
    put_u32(r2, 0, 0x1000);
    put_u32(r2, 4, 16);  // claims more than the 12-byte directory
    put_u32(r2, 8, 0);
    const std::uint32_t rva2 = overrun.add_section(".reloc", r2, PeBuilder::kRdata);
    overrun.set_directory(DirectoryIndex::base_relocation_table, rva2, 12);
    expect_error(overrun.build(), "relocation_table_malformed");
}

TEST(PeReject, RelocationOutOfImage) {
    PeBuilder pe = minimal_builder();
    Bytes reloc;
    put_u32(reloc, 0, 0x2000);  // page past the 0x1000-byte .text
    put_u32(reloc, 4, 12);
    put_u16(reloc, 8, (10u << 12) | 0xFFC);  // DIR64 at 0x2FFC + 8 > SizeOfImage 0x3000
    put_u16(reloc, 10, 0);
    const std::uint32_t rva = pe.add_section(".reloc", reloc, PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::base_relocation_table, rva, 12);
    expect_error(pe.build(), "relocation_out_of_image");
}

TEST(PeReject, TlsDirectoryTooSmall) {
    PeBuilder pe = minimal_builder();
    const std::uint32_t rva = pe.add_section(".tls", filled(64, 0), PeBuilder::kData);
    pe.set_directory(DirectoryIndex::tls_table, rva, 20);
    expect_error(pe.build(), "tls_directory_malformed");
}

TEST(PeReject, TlsCallbackOutsideImage) {
    PeBuilder pe = minimal_builder();
    const std::uint32_t rva = pe.next_section_rva();
    Bytes tls;
    put_u64(tls, 24, pe.image_base + rva + 0x40);
    put_u64(tls, 0x40, 0x1000);  // below image_base
    put_u64(tls, 0x48, 0);
    pe.add_section(".tls", tls, PeBuilder::kData);
    pe.set_directory(DirectoryIndex::tls_table, rva, 40);
    expect_error(pe.build(), "tls_directory_malformed");
}

TEST(PeReject, ExceptionDirectorySize) {
    PeBuilder pe = minimal_builder();
    const std::uint32_t rva = pe.add_section(".pdata", filled(24, 0), PeBuilder::kRdata);
    pe.set_directory(DirectoryIndex::exception_table, rva, 13);
    expect_error(pe.build(), "exception_directory_malformed");
}
