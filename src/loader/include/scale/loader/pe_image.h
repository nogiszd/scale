#pragma once

// Read-only, bounded PE32+ (AMD64) image inspection.
//
// Source: Microsoft PE Format specification,
// https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
//
// The inspector never maps or executes the file. All structures are parsed
// field by field from a scale::ByteView with checked arithmetic. Inputs that
// are malformed, truncated or outside the supported subset are rejected with
// a scale::Error whose component is "loader" and whose code is one of the
// error codes listed at the end of this header.

#include "scale/common/byte_reader.h"
#include "scale/common/error.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scale::loader {

// ---- Limits (inputs beyond these are rejected with `limit_exceeded`) --------

inline constexpr std::uint64_t kDefaultMaxFileSize = 512ull * 1024 * 1024;
// The PE specification notes that the Windows loader limits images to 96 sections.
inline constexpr std::uint16_t kMaxSections = 96;
inline constexpr std::size_t kMaxImportModules = 4096;
inline constexpr std::size_t kMaxImportsPerModule = 65536;
inline constexpr std::size_t kMaxNameLength = 4096;
inline constexpr std::size_t kMaxRelocationBlocks = 1u << 20;
inline constexpr std::size_t kMaxTlsCallbacks = 1024;

// ---- Constants from the PE specification -------------------------------------

inline constexpr std::uint16_t kMachineAmd64 = 0x8664;
inline constexpr std::uint16_t kOptionalMagicPe32 = 0x10B;
inline constexpr std::uint16_t kOptionalMagicPe32Plus = 0x20B;
inline constexpr std::uint16_t kFileExecutableImage = 0x0002;
inline constexpr std::uint16_t kFileDll = 0x2000;
inline constexpr std::uint32_t kSectionMemExecute = 0x20000000;
inline constexpr std::uint32_t kSectionMemRead = 0x40000000;
inline constexpr std::uint32_t kSectionMemWrite = 0x80000000;

enum class DirectoryIndex : std::size_t {
    export_table = 0,
    import_table = 1,
    resource_table = 2,
    exception_table = 3,
    certificate_table = 4,  // file offset, not an RVA
    base_relocation_table = 5,
    debug = 6,
    architecture = 7,
    global_ptr = 8,
    tls_table = 9,
    load_config_table = 10,
    bound_import = 11,
    iat = 12,
    delay_import_descriptor = 13,
    clr_runtime_header = 14,
    reserved = 15,
};
inline constexpr std::size_t kDirectoryCount = 16;

// Stable snake_case name, e.g. "import_table".
[[nodiscard]] std::string_view directory_name(DirectoryIndex index) noexcept;

// ---- Inspection result -------------------------------------------------------------

struct DataDirectory {
    std::uint32_t rva = 0;  // file offset for certificate_table
    std::uint32_t size = 0;
    [[nodiscard]] bool present() const noexcept { return rva != 0 || size != 0; }
};

struct Section {
    std::string name;  // up to 8 bytes, trailing NULs removed
    std::uint32_t virtual_address = 0;
    std::uint32_t virtual_size = 0;
    std::uint32_t raw_offset = 0;
    std::uint32_t raw_size = 0;
    std::uint32_t characteristics = 0;
};

struct ImportEntry {
    bool by_ordinal = false;
    std::uint16_t ordinal = 0;  // valid when by_ordinal
    std::uint16_t hint = 0;     // valid when !by_ordinal
    std::string name;           // valid when !by_ordinal
};

struct ImportModule {
    std::string dll_name;
    std::uint32_t import_lookup_table_rva = 0;  // 0 if absent (IAT used instead)
    std::uint32_t import_address_table_rva = 0;
    std::vector<ImportEntry> entries;
};

struct RelocationSummary {
    std::uint32_t block_count = 0;
    std::uint32_t entry_count = 0;                      // including IMAGE_REL_BASED_ABSOLUTE padding
    std::array<std::uint32_t, 16> count_by_type = {};   // index = 4-bit relocation type
};

struct TlsInfo {
    std::uint64_t start_address_of_raw_data = 0;  // VAs as stored (preferred base)
    std::uint64_t end_address_of_raw_data = 0;
    std::uint64_t address_of_index = 0;
    std::uint64_t address_of_callbacks = 0;
    std::uint32_t size_of_zero_fill = 0;
    std::uint32_t characteristics = 0;
    std::vector<std::uint32_t> callback_rvas;  // relative to image_base
};

struct ExceptionInfo {
    std::uint32_t runtime_function_count = 0;  // x64 RUNTIME_FUNCTION entries (12 bytes)
};

struct PeImage {
    // COFF file header
    std::uint16_t machine = 0;
    std::uint32_t time_date_stamp = 0;
    std::uint16_t characteristics = 0;

    // Optional header (PE32+)
    std::uint16_t magic = 0;
    std::uint8_t major_linker_version = 0;
    std::uint8_t minor_linker_version = 0;
    std::uint32_t address_of_entry_point = 0;  // RVA, 0 if none
    std::uint64_t image_base = 0;
    std::uint32_t section_alignment = 0;
    std::uint32_t file_alignment = 0;
    std::uint16_t major_subsystem_version = 0;
    std::uint16_t minor_subsystem_version = 0;
    std::uint32_t size_of_image = 0;
    std::uint32_t size_of_headers = 0;
    std::uint32_t checksum = 0;
    std::uint16_t subsystem = 0;
    std::uint16_t dll_characteristics = 0;
    std::uint64_t size_of_stack_reserve = 0;
    std::uint64_t size_of_stack_commit = 0;
    std::uint64_t size_of_heap_reserve = 0;
    std::uint64_t size_of_heap_commit = 0;
    std::uint32_t number_of_rva_and_sizes = 0;

    std::array<DataDirectory, kDirectoryCount> directories = {};
    std::vector<Section> sections;
    std::vector<ImportModule> imports;
    std::optional<RelocationSummary> relocations;  // set when the directory is present
    std::optional<TlsInfo> tls;
    std::optional<ExceptionInfo> exceptions;

    [[nodiscard]] bool is_dll() const noexcept { return (characteristics & kFileDll) != 0; }
    [[nodiscard]] const DataDirectory& directory(DirectoryIndex index) const noexcept {
        return directories[static_cast<std::size_t>(index)];
    }
};

// Inspects an in-memory image file.
[[nodiscard]] Result<PeImage> inspect_pe(ByteView file);

// Reads (read-only) and inspects a file of at most `max_size` bytes.
// Additional error codes: file_open_failed, file_read_failed, file_too_large.
[[nodiscard]] Result<PeImage> inspect_pe_file(const std::filesystem::path& path,
                                             std::uint64_t max_size = kDefaultMaxFileSize);

// ---- Error codes (component "loader") ---------------------------------------
//
//   truncated_dos_header        file shorter than the 64-byte DOS header
//   bad_dos_magic               e_magic != "MZ"
//   bad_pe_offset               e_lfanew points outside the file
//   bad_pe_signature            missing "PE\0\0"
//   truncated_file_header       COFF header extends past end of file
//   unsupported_machine         Machine != IMAGE_FILE_MACHINE_AMD64
//   not_an_image                IMAGE_FILE_EXECUTABLE_IMAGE not set
//   truncated_optional_header   optional header extends past end of file
//   optional_header_too_small   SizeOfOptionalHeader too small for its contents
//   unsupported_optional_header PE32 (0x10B); only PE32+ is supported
//   bad_optional_header_magic   unknown optional header magic
//   bad_directory_count         NumberOfRvaAndSizes > 16
//   bad_alignment               invalid SectionAlignment / FileAlignment
//   bad_size_of_image           SizeOfImage not a multiple of SectionAlignment
//   bad_size_of_headers         SizeOfHeaders outside the file or smaller than the headers
//   truncated_section_table     section table extends past SizeOfHeaders or file
//   section_misaligned          section VA or raw pointer violates alignment
//   sections_overlap            section VAs not ascending / overlapping
//   section_out_of_image        section extends past SizeOfImage
//   section_out_of_file         section raw data extends past end of file
//   entry_point_out_of_image    AddressOfEntryPoint >= SizeOfImage
//   directory_out_of_image      RVA directory extends past SizeOfImage
//   directory_out_of_file       certificate table extends past end of file
//   rva_not_in_file             required data is not backed by file bytes
//   import_table_malformed      bad descriptor, thunk or name
//   relocation_table_malformed  bad block header or size
//   relocation_out_of_image     relocation target outside the image
//   tls_directory_malformed     TLS directory too small or callbacks invalid
//   exception_directory_malformed  size not a multiple of 12
//   limit_exceeded              a documented limit above was exceeded

}  // namespace scale::loader
