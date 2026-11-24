#include "scale/loader/pe_image.h"
#include "scale/loader/image_map.h"

#include "scale/common/checked.h"
#include "scale/common/text.h"

#include <format>
#include <fstream>
#include <span>
#include <system_error>
#include <utility>

namespace scale::loader {
namespace {

// Structure sizes and offsets from the PE format specification.
constexpr std::size_t kDosHeaderSize = 64;
constexpr std::size_t kDosLfanewOffset = 0x3C;
constexpr std::size_t kPeSignatureSize = 4;
constexpr std::size_t kFileHeaderSize = 20;
constexpr std::size_t kOptionalFixedSize = 112;  // PE32+ fields before the data directories
constexpr std::size_t kDataDirectorySize = 8;
constexpr std::size_t kSectionHeaderSize = 40;
constexpr std::uint32_t kImportDescriptorSize = 20;
constexpr std::uint32_t kThunkSize = 8;
constexpr std::uint32_t kTlsDirectorySize = 40;
constexpr std::uint32_t kRuntimeFunctionSize = 12;
constexpr std::uint64_t kOrdinalFlag = 1ull << 63;
constexpr std::uint32_t kPageSize = 4096;

// UTF-8 form of a path for messages; never throws.
std::string display(const std::filesystem::path& path) {
    auto utf8 = to_utf8(path.native());
    return utf8 ? *utf8 : std::string("<path not representable as UTF-8>");
}

std::unexpected<Error> loader_error(std::string_view code, std::string message) {
    return fail("loader", code, std::move(message));
}

bool is_power_of_two(std::uint32_t v) {
    return v != 0 && (v & (v - 1)) == 0;
}

// Width in bytes of the location patched by a base relocation type.
std::uint32_t relocation_width(unsigned type) {
    switch (type) {
        case 1:  // IMAGE_REL_BASED_HIGH
        case 2:  // IMAGE_REL_BASED_LOW
            return 2;
        case 3:  // IMAGE_REL_BASED_HIGHLOW
            return 4;
        case 10:  // IMAGE_REL_BASED_DIR64
            return 8;
        default:
            return 1;
    }
}

class Parser {
public:
    explicit Parser(ByteView file) : file_(file) {}

    Result<PeImage> run() {
        if (auto r = parse_headers(); !r) return std::unexpected(r.error());
        if (auto r = parse_sections(); !r) return std::unexpected(r.error());
        if (auto r = parse_directories(); !r) return std::unexpected(r.error());
        if (auto r = parse_imports(); !r) return std::unexpected(r.error());
        if (auto r = parse_relocations(); !r) return std::unexpected(r.error());
        if (auto r = parse_tls(); !r) return std::unexpected(r.error());
        if (auto r = parse_exceptions(); !r) return std::unexpected(r.error());
        return std::move(img_);
    }

private:
    // ---- RVA access -----------------------------------------------------------

    // File bytes backing the image from `rva` to the end of its region (the
    // headers or one section's raw data). nullopt if the RVA is not backed by
    // file data (e.g. uninitialized section tail).
    [[nodiscard]] std::optional<ByteView> rva_tail(std::uint32_t rva) const {
        if (rva < img_.size_of_headers) {
            return file_.subview(rva, img_.size_of_headers - rva);
        }
        for (const Section& s : img_.sections) {
            const std::uint32_t extent = s.virtual_size != 0 ? s.virtual_size : s.raw_size;
            const std::uint32_t backed = std::min(extent, s.raw_size);
            if (rva >= s.virtual_address && rva - s.virtual_address < backed) {
                const std::uint32_t delta = rva - s.virtual_address;
                return file_.subview(static_cast<std::size_t>(s.raw_offset) + delta, backed - delta);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<ByteView> rva_range(std::uint32_t rva, std::uint32_t size) const {
        const auto tail = rva_tail(rva);
        if (!tail || tail->size() < size) {
            return std::nullopt;
        }
        return tail->subview(0, size);
    }

    // RVA of element `index` of `elem_size` bytes in a table at `base`.
    [[nodiscard]] static std::optional<std::uint32_t> element_rva(std::uint32_t base, std::size_t index,
                                                                  std::uint32_t elem_size) {
        const auto idx = checked_cast<std::uint32_t>(index);
        if (!idx) return std::nullopt;
        const auto off = checked_mul<std::uint32_t>(*idx, elem_size);
        if (!off) return std::nullopt;
        return checked_add<std::uint32_t>(base, *off);
    }

    // NUL-terminated string at `rva`, at most kMaxNameLength bytes.
    [[nodiscard]] Result<std::string> read_cstring(std::uint32_t rva, std::string_view code,
                                                   std::string_view what) const {
        const auto tail = rva_tail(rva);
        if (!tail) {
            return loader_error(code, std::format("{} at RVA 0x{:x} is not backed by file data", what, rva));
        }
        const auto bytes = tail->bytes();
        const std::size_t limit = std::min(bytes.size(), kMaxNameLength + 1);
        for (std::size_t i = 0; i < limit; ++i) {
            if (bytes[i] == std::byte{0}) {
                return std::string(reinterpret_cast<const char*>(bytes.data()), i);
            }
        }
        if (limit > kMaxNameLength) {
            return loader_error("limit_exceeded",
                                std::format("{} at RVA 0x{:x} exceeds {} bytes", what, rva, kMaxNameLength));
        }
        return loader_error(code, std::format("{} at RVA 0x{:x} is not NUL-terminated", what, rva));
    }

    // ---- Headers -----------------------------------------------------------------

    Result<void> parse_headers() {
        if (file_.size() < kDosHeaderSize) {
            return loader_error("truncated_dos_header",
                                std::format("file is {} bytes; the DOS header needs {}", file_.size(), kDosHeaderSize));
        }
        if (*file_.u16(0) != 0x5A4D) {
            return loader_error("bad_dos_magic", "DOS header magic is not 'MZ'");
        }
        pe_offset_ = *file_.u32(kDosLfanewOffset);
        if (!file_.contains(pe_offset_, kPeSignatureSize)) {
            return loader_error("bad_pe_offset",
                                std::format("e_lfanew 0x{:x} is outside the {}-byte file", pe_offset_, file_.size()));
        }
        if (*file_.u32(pe_offset_) != 0x00004550) {
            return loader_error("bad_pe_signature", std::format("no 'PE\\0\\0' signature at 0x{:x}", pe_offset_));
        }

        const std::size_t fh = pe_offset_ + kPeSignatureSize;
        if (!file_.contains(fh, kFileHeaderSize)) {
            return loader_error("truncated_file_header", "COFF file header extends past end of file");
        }
        img_.machine = *file_.u16(fh + 0);
        const std::uint16_t section_count = *file_.u16(fh + 2);
        img_.time_date_stamp = *file_.u32(fh + 4);
        const std::uint16_t optional_size = *file_.u16(fh + 16);
        img_.characteristics = *file_.u16(fh + 18);

        if (img_.machine != kMachineAmd64) {
            return loader_error("unsupported_machine",
                                std::format("machine 0x{:04x} is not AMD64 (0x8664)", img_.machine));
        }
        if ((img_.characteristics & kFileExecutableImage) == 0) {
            return loader_error("not_an_image", "IMAGE_FILE_EXECUTABLE_IMAGE is not set (object file?)");
        }
        if (section_count > kMaxSections) {
            return loader_error("limit_exceeded",
                                std::format("{} sections exceeds the limit of {}", section_count, kMaxSections));
        }

        const std::size_t oh = fh + kFileHeaderSize;
        if (optional_size < 2 || !file_.contains(oh, optional_size)) {
            return loader_error("truncated_optional_header",
                                std::format("optional header ({} bytes at 0x{:x}) extends past end of file",
                                            optional_size, oh));
        }
        img_.magic = *file_.u16(oh);
        if (img_.magic == kOptionalMagicPe32) {
            return loader_error("unsupported_optional_header", "PE32 (0x10B) image; only PE32+ is supported");
        }
        if (img_.magic != kOptionalMagicPe32Plus) {
            return loader_error("bad_optional_header_magic",
                                std::format("unknown optional header magic 0x{:x}", img_.magic));
        }
        if (optional_size < kOptionalFixedSize) {
            return loader_error("optional_header_too_small",
                                std::format("SizeOfOptionalHeader {} < {}", optional_size, kOptionalFixedSize));
        }

        img_.major_linker_version = *file_.u8(oh + 2);
        img_.minor_linker_version = *file_.u8(oh + 3);
        img_.address_of_entry_point = *file_.u32(oh + 16);
        img_.image_base = *file_.u64(oh + 24);
        img_.section_alignment = *file_.u32(oh + 32);
        img_.file_alignment = *file_.u32(oh + 36);
        img_.major_subsystem_version = *file_.u16(oh + 48);
        img_.minor_subsystem_version = *file_.u16(oh + 50);
        img_.size_of_image = *file_.u32(oh + 56);
        img_.size_of_headers = *file_.u32(oh + 60);
        img_.checksum = *file_.u32(oh + 64);
        img_.subsystem = *file_.u16(oh + 68);
        img_.dll_characteristics = *file_.u16(oh + 70);
        img_.size_of_stack_reserve = *file_.u64(oh + 72);
        img_.size_of_stack_commit = *file_.u64(oh + 80);
        img_.size_of_heap_reserve = *file_.u64(oh + 88);
        img_.size_of_heap_commit = *file_.u64(oh + 96);
        img_.number_of_rva_and_sizes = *file_.u32(oh + 108);

        if (img_.number_of_rva_and_sizes > kDirectoryCount) {
            return loader_error("bad_directory_count",
                                std::format("NumberOfRvaAndSizes {} > {}", img_.number_of_rva_and_sizes,
                                            kDirectoryCount));
        }
        if (kOptionalFixedSize + kDataDirectorySize * img_.number_of_rva_and_sizes > optional_size) {
            return loader_error("optional_header_too_small",
                                std::format("SizeOfOptionalHeader {} cannot hold {} data directories",
                                            optional_size, img_.number_of_rva_and_sizes));
        }
        for (std::uint32_t i = 0; i < img_.number_of_rva_and_sizes; ++i) {
            const std::size_t at = oh + kOptionalFixedSize + i * kDataDirectorySize;
            img_.directories[i] = DataDirectory{*file_.u32(at), *file_.u32(at + 4)};
        }

        // Alignment rules (PE spec, optional header Windows-specific fields).
        const std::uint32_t sa = img_.section_alignment;
        const std::uint32_t fa = img_.file_alignment;
        if (!is_power_of_two(sa) || !is_power_of_two(fa) || sa < fa || fa > 0x10000 ||
            (sa < kPageSize && fa != sa)) {
            return loader_error("bad_alignment",
                                std::format("SectionAlignment 0x{:x} / FileAlignment 0x{:x} are invalid", sa, fa));
        }
        if (img_.size_of_image == 0 || img_.size_of_image % sa != 0) {
            return loader_error("bad_size_of_image",
                                std::format("SizeOfImage 0x{:x} is not a non-zero multiple of SectionAlignment 0x{:x}",
                                            img_.size_of_image, sa));
        }

        section_table_offset_ = oh + optional_size;
        section_count_ = section_count;
        const std::size_t table_end = section_table_offset_ + kSectionHeaderSize * section_count;
        if (table_end > file_.size()) {
            return loader_error("truncated_section_table", "section table extends past end of file");
        }
        if (img_.size_of_headers > file_.size() || img_.size_of_headers < table_end ||
            img_.size_of_headers % fa != 0 || img_.size_of_headers > img_.size_of_image) {
            return loader_error("bad_size_of_headers",
                                std::format("SizeOfHeaders 0x{:x} is invalid (headers end at 0x{:x}, file is 0x{:x} "
                                            "bytes, FileAlignment 0x{:x})",
                                            img_.size_of_headers, table_end, file_.size(), fa));
        }
        if (img_.address_of_entry_point >= img_.size_of_image) {
            return loader_error("entry_point_out_of_image",
                                std::format("AddressOfEntryPoint 0x{:x} >= SizeOfImage 0x{:x}",
                                            img_.address_of_entry_point, img_.size_of_image));
        }
        return {};
    }

    // ---- Sections ------------------------------------------------------------------

    Result<void> parse_sections() {
        std::uint64_t previous_end = img_.size_of_headers;
        img_.sections.reserve(section_count_);
        for (std::size_t i = 0; i < section_count_; ++i) {
            const std::size_t at = section_table_offset_ + i * kSectionHeaderSize;
            Section s;
            const auto name_bytes = file_.subview(at, 8)->bytes();
            std::size_t name_len = 0;
            while (name_len < 8 && name_bytes[name_len] != std::byte{0}) {
                ++name_len;
            }
            s.name.assign(reinterpret_cast<const char*>(name_bytes.data()), name_len);
            s.virtual_size = *file_.u32(at + 8);
            s.virtual_address = *file_.u32(at + 12);
            s.raw_size = *file_.u32(at + 16);
            s.raw_offset = *file_.u32(at + 20);
            s.characteristics = *file_.u32(at + 36);

            if (s.virtual_address % img_.section_alignment != 0 ||
                (s.raw_size != 0 && s.raw_offset % img_.file_alignment != 0)) {
                return loader_error("section_misaligned",
                                    std::format("section {} '{}' VA 0x{:x} / raw 0x{:x} violates alignment", i,
                                                s.name, s.virtual_address, s.raw_offset));
            }
            if (s.virtual_address < previous_end) {
                return loader_error("sections_overlap",
                                    std::format("section {} '{}' at VA 0x{:x} overlaps the previous region ending at "
                                                "0x{:x}",
                                                i, s.name, s.virtual_address, previous_end));
            }
            const std::uint64_t extent = s.virtual_size != 0 ? s.virtual_size : s.raw_size;
            const std::uint64_t end = std::uint64_t{s.virtual_address} + extent;
            if (end > img_.size_of_image) {
                return loader_error("section_out_of_image",
                                    std::format("section {} '{}' ends at 0x{:x}, past SizeOfImage 0x{:x}", i, s.name,
                                                end, img_.size_of_image));
            }
            if (s.raw_size != 0 && !file_.contains(s.raw_offset, s.raw_size)) {
                return loader_error("section_out_of_file",
                                    std::format("section {} '{}' raw data 0x{:x}+0x{:x} extends past end of file", i,
                                                s.name, s.raw_offset, s.raw_size));
            }
            // Round up to the section alignment; cannot overflow (end <= SizeOfImage).
            previous_end = (end + img_.section_alignment - 1) / img_.section_alignment * img_.section_alignment;
            img_.sections.push_back(std::move(s));
        }
        return {};
    }

    // ---- Data directories ------------------------------------------------------------

    Result<void> parse_directories() {
        for (std::size_t i = 0; i < kDirectoryCount; ++i) {
            const DataDirectory& d = img_.directories[i];
            if (!d.present()) {
                continue;
            }
            const auto index = static_cast<DirectoryIndex>(i);
            const std::uint64_t end = std::uint64_t{d.rva} + d.size;
            if (index == DirectoryIndex::certificate_table) {
                if (end > file_.size()) {
                    return loader_error("directory_out_of_file",
                                        std::format("certificate_table 0x{:x}+0x{:x} extends past end of file", d.rva,
                                                    d.size));
                }
            } else if (end > img_.size_of_image) {
                return loader_error("directory_out_of_image",
                                    std::format("{} 0x{:x}+0x{:x} extends past SizeOfImage 0x{:x}",
                                                directory_name(index), d.rva, d.size, img_.size_of_image));
            }
        }
        return {};
    }

    // ---- Imports ----------------------------------------------------------------------

    Result<void> parse_imports() {
        const DataDirectory& dir = img_.directory(DirectoryIndex::import_table);
        if (!dir.present()) {
            return {};
        }
        for (std::size_t i = 0;; ++i) {
            if (i >= kMaxImportModules) {
                return loader_error("limit_exceeded",
                                    std::format("more than {} import descriptors", kMaxImportModules));
            }
            const auto at = element_rva(dir.rva, i, kImportDescriptorSize);
            const auto desc = at ? rva_range(*at, kImportDescriptorSize) : std::nullopt;
            if (!desc) {
                return loader_error("import_table_malformed",
                                    std::format("import descriptor {} is not backed by file data", i));
            }
            const std::uint32_t ilt = *desc->u32(0);
            const std::uint32_t name_rva = *desc->u32(12);
            const std::uint32_t iat = *desc->u32(16);
            if (ilt == 0 && *desc->u32(4) == 0 && *desc->u32(8) == 0 && name_rva == 0 && iat == 0) {
                break;  // null terminator descriptor
            }
            if (name_rva == 0 || iat == 0) {
                return loader_error("import_table_malformed",
                                    std::format("import descriptor {} has a zero Name or FirstThunk RVA", i));
            }
            auto name = read_cstring(name_rva, "import_table_malformed", "import module name");
            if (!name) return std::unexpected(name.error());
            if (name->empty()) {
                return loader_error("import_table_malformed", std::format("import descriptor {} has an empty name", i));
            }

            ImportModule module{std::move(*name), ilt, iat, {}};
            if (auto r = parse_thunks(module, ilt != 0 ? ilt : iat); !r) return std::unexpected(r.error());
            img_.imports.push_back(std::move(module));
        }
        return {};
    }

    Result<void> parse_thunks(ImportModule& module, std::uint32_t table_rva) {
        for (std::size_t j = 0;; ++j) {
            if (j >= kMaxImportsPerModule) {
                return loader_error("limit_exceeded", std::format("module '{}' imports more than {} symbols",
                                                                  module.dll_name, kMaxImportsPerModule));
            }
            const auto at = element_rva(table_rva, j, kThunkSize);
            const auto thunk = at ? rva_range(*at, kThunkSize) : std::nullopt;
            if (!thunk) {
                return loader_error("import_table_malformed",
                                    std::format("import thunk {} of '{}' is not backed by file data", j,
                                                module.dll_name));
            }
            const std::uint64_t value = *thunk->u64(0);
            if (value == 0) {
                return {};
            }
            ImportEntry entry;
            if ((value & kOrdinalFlag) != 0) {
                // Bits 62-16 must be zero for an ordinal import.
                if ((value & 0x7FFF'FFFF'FFFF'0000ull) != 0) {
                    return loader_error("import_table_malformed",
                                        std::format("ordinal thunk 0x{:x} of '{}' has reserved bits set", value,
                                                    module.dll_name));
                }
                entry.by_ordinal = true;
                entry.ordinal = static_cast<std::uint16_t>(value & 0xFFFF);
            } else {
                // Bits 62-31 must be zero; bits 30-0 are the hint/name RVA.
                if ((value >> 31) != 0) {
                    return loader_error("import_table_malformed",
                                        std::format("name thunk 0x{:x} of '{}' has reserved bits set", value,
                                                    module.dll_name));
                }
                const auto hint_name_rva = static_cast<std::uint32_t>(value);
                const auto hint = rva_range(hint_name_rva, 2);
                if (!hint) {
                    return loader_error("import_table_malformed",
                                        std::format("hint/name entry at RVA 0x{:x} of '{}' is not backed by file data",
                                                    hint_name_rva, module.dll_name));
                }
                entry.hint = *hint->u16(0);
                auto name = read_cstring(hint_name_rva + 2, "import_table_malformed", "import symbol name");
                if (!name) return std::unexpected(name.error());
                if (name->empty()) {
                    return loader_error("import_table_malformed",
                                        std::format("empty import symbol name in '{}'", module.dll_name));
                }
                entry.name = std::move(*name);
            }
            module.entries.push_back(std::move(entry));
        }
    }

    // ---- Base relocations -----------------------------------------------------------

    Result<void> parse_relocations() {
        const DataDirectory& dir = img_.directory(DirectoryIndex::base_relocation_table);
        if (!dir.present()) {
            return {};
        }
        const auto table = rva_range(dir.rva, dir.size);
        if (!table) {
            return loader_error("rva_not_in_file",
                                std::format("base relocation table 0x{:x}+0x{:x} is not backed by file data", dir.rva,
                                            dir.size));
        }
        RelocationSummary summary;
        std::size_t offset = 0;
        while (offset < table->size()) {
            if (summary.block_count >= kMaxRelocationBlocks) {
                return loader_error("limit_exceeded",
                                    std::format("more than {} relocation blocks", kMaxRelocationBlocks));
            }
            const std::size_t remaining = table->size() - offset;
            if (remaining < 8) {
                return loader_error("relocation_table_malformed",
                                    std::format("truncated relocation block header at offset 0x{:x}", offset));
            }
            const std::uint32_t page = *table->u32(offset);
            const std::uint32_t block_size = *table->u32(offset + 4);
            if (block_size < 8 || block_size % 4 != 0 || block_size > remaining) {
                return loader_error("relocation_table_malformed",
                                    std::format("relocation block at offset 0x{:x} has invalid size 0x{:x}", offset,
                                                block_size));
            }
            const std::size_t entries = (block_size - 8) / 2;
            for (std::size_t k = 0; k < entries; ++k) {
                const std::uint16_t e = *table->u16(offset + 8 + k * 2);
                const unsigned type = e >> 12;
                ++summary.count_by_type[type];
                ++summary.entry_count;
                if (type == 0) {
                    continue;  // IMAGE_REL_BASED_ABSOLUTE padding
                }
                const std::uint64_t target = std::uint64_t{page} + (e & 0x0FFFu);
                if (target + relocation_width(type) > img_.size_of_image) {
                    return loader_error("relocation_out_of_image",
                                        std::format("type {} relocation at RVA 0x{:x} is outside the image", type,
                                                    target));
                }
            }
            ++summary.block_count;
            offset += block_size;
        }
        img_.relocations = summary;
        return {};
    }

    // ---- TLS --------------------------------------------------------------------------

    [[nodiscard]] std::optional<std::uint32_t> va_to_rva(std::uint64_t va) const {
        const auto rva = checked_sub<std::uint64_t>(va, img_.image_base);
        if (!rva || *rva >= img_.size_of_image) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(*rva);
    }

    Result<void> parse_tls() {
        const DataDirectory& dir = img_.directory(DirectoryIndex::tls_table);
        if (!dir.present()) {
            return {};
        }
        if (dir.size < kTlsDirectorySize) {
            return loader_error("tls_directory_malformed",
                                std::format("TLS directory size {} < {}", dir.size, kTlsDirectorySize));
        }
        const auto view = rva_range(dir.rva, kTlsDirectorySize);
        if (!view) {
            return loader_error("rva_not_in_file",
                                std::format("TLS directory at RVA 0x{:x} is not backed by file data", dir.rva));
        }
        TlsInfo tls;
        tls.start_address_of_raw_data = *view->u64(0);
        tls.end_address_of_raw_data = *view->u64(8);
        tls.address_of_index = *view->u64(16);
        tls.address_of_callbacks = *view->u64(24);
        tls.size_of_zero_fill = *view->u32(32);
        tls.characteristics = *view->u32(36);

        if (tls.address_of_callbacks != 0) {
            const auto array_rva = va_to_rva(tls.address_of_callbacks);
            if (!array_rva) {
                return loader_error("tls_directory_malformed",
                                    std::format("TLS callback array VA 0x{:x} is outside the image",
                                                tls.address_of_callbacks));
            }
            for (std::size_t k = 0;; ++k) {
                const auto at = element_rva(*array_rva, k, 8);
                const auto slot = at ? rva_range(*at, 8) : std::nullopt;
                if (!slot) {
                    return loader_error("tls_directory_malformed",
                                        std::format("TLS callback slot {} is not backed by file data", k));
                }
                const std::uint64_t callback = *slot->u64(0);
                if (callback == 0) {
                    break;
                }
                if (tls.callback_rvas.size() == kMaxTlsCallbacks) {
                    return loader_error("limit_exceeded",
                                        std::format("more than {} TLS callbacks", kMaxTlsCallbacks));
                }
                const auto callback_rva = va_to_rva(callback);
                if (!callback_rva) {
                    return loader_error("tls_directory_malformed",
                                        std::format("TLS callback VA 0x{:x} is outside the image", callback));
                }
                tls.callback_rvas.push_back(*callback_rva);
            }
        }
        img_.tls = std::move(tls);
        return {};
    }

    // ---- Exception directory ------------------------------------------------------------

    Result<void> parse_exceptions() {
        const DataDirectory& dir = img_.directory(DirectoryIndex::exception_table);
        if (!dir.present()) {
            return {};
        }
        if (dir.size % kRuntimeFunctionSize != 0) {
            return loader_error("exception_directory_malformed",
                                std::format("exception directory size {} is not a multiple of {}", dir.size,
                                            kRuntimeFunctionSize));
        }
        if (!rva_range(dir.rva, dir.size)) {
            return loader_error("rva_not_in_file",
                                std::format("exception directory 0x{:x}+0x{:x} is not backed by file data", dir.rva,
                                            dir.size));
        }
        img_.exceptions = ExceptionInfo{dir.size / kRuntimeFunctionSize};
        return {};
    }

    ByteView file_;
    PeImage img_;
    std::size_t pe_offset_ = 0;
    std::size_t section_table_offset_ = 0;
    std::size_t section_count_ = 0;
};

}  // namespace

std::string_view directory_name(DirectoryIndex index) noexcept {
    switch (index) {
        case DirectoryIndex::export_table: return "export_table";
        case DirectoryIndex::import_table: return "import_table";
        case DirectoryIndex::resource_table: return "resource_table";
        case DirectoryIndex::exception_table: return "exception_table";
        case DirectoryIndex::certificate_table: return "certificate_table";
        case DirectoryIndex::base_relocation_table: return "base_relocation_table";
        case DirectoryIndex::debug: return "debug";
        case DirectoryIndex::architecture: return "architecture";
        case DirectoryIndex::global_ptr: return "global_ptr";
        case DirectoryIndex::tls_table: return "tls_table";
        case DirectoryIndex::load_config_table: return "load_config_table";
        case DirectoryIndex::bound_import: return "bound_import";
        case DirectoryIndex::iat: return "iat";
        case DirectoryIndex::delay_import_descriptor: return "delay_import_descriptor";
        case DirectoryIndex::clr_runtime_header: return "clr_runtime_header";
        case DirectoryIndex::reserved: return "reserved";
    }
    return "unknown";
}

Result<PeImage> inspect_pe(ByteView file) {
    return Parser(file).run();
}

Result<std::vector<std::byte>> read_file(const std::filesystem::path& path, std::uint64_t max_size) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        return loader_error("file_open_failed", std::format("cannot stat '{}': {}", display(path), ec.message()));
    }
    if (size > max_size) {
        return loader_error("file_too_large",
                            std::format("'{}' is {} bytes; the limit is {}", display(path), size, max_size));
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return loader_error("file_open_failed", std::format("cannot open '{}' for reading", display(path)));
    }
    std::vector<std::byte> data(static_cast<std::size_t>(size));
    if (size != 0 && !in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size))) {
        return loader_error("file_read_failed", std::format("short read from '{}'", display(path)));
    }
    return data;
}

Result<PeImage> inspect_pe_file(const std::filesystem::path& path, std::uint64_t max_size) {
    const auto data = read_file(path, max_size);
    if (!data) {
        return std::unexpected(data.error());
    }
    return inspect_pe(ByteView(*data));
}

}  // namespace scale::loader
