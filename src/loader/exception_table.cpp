#include "scale/loader/exception_table.h"

#include <format>
#include <string>

namespace scale::loader {
namespace {

constexpr std::uint32_t kRuntimeFunctionSize = 12;
constexpr std::uint32_t kUnwindHeaderSize = 4;
constexpr std::uint8_t kFlagEHandler = 0x1;
constexpr std::uint8_t kFlagUHandler = 0x2;
constexpr std::uint8_t kFlagChainInfo = 0x4;
constexpr std::uint8_t kKnownFlags = kFlagEHandler | kFlagUHandler | kFlagChainInfo;

std::unexpected<Error> table_error(std::string_view code, std::string message) {
    return fail("loader", code, std::move(message));
}

std::uint32_t section_extent(const Section& s) {
    return s.virtual_size != 0 ? s.virtual_size : s.raw_size;
}

// Section whose virtual range contains [rva, rva + length), or nullptr.
const Section* section_for(const PeImage& image, std::uint64_t rva, std::uint64_t length) {
    for (const Section& s : image.sections) {
        const std::uint64_t start = s.virtual_address;
        const std::uint64_t end = start + section_extent(s);
        if (rva >= start && length <= end - start && rva - start <= end - start - length) return &s;
    }
    return nullptr;
}

bool executable(const Section* s) {
    return s != nullptr && (s->characteristics & kSectionMemExecute) != 0;
}

// File bytes backing [rva, rva + length) inside one section's raw data.
std::optional<ByteView> file_range(ByteView file, const PeImage& image, std::uint64_t rva, std::uint64_t length) {
    const Section* s = section_for(image, rva, length);
    if (s == nullptr) return std::nullopt;
    const std::uint64_t offset_in_section = rva - s->virtual_address;
    if (offset_in_section + length > s->raw_size) return std::nullopt;  // zero-fill, not file data
    return file.subview(static_cast<std::size_t>(s->raw_offset + offset_in_section), static_cast<std::size_t>(length));
}

struct RuntimeFunction {
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    std::uint32_t unwind = 0;
};

RuntimeFunction read_function(ByteView v, std::size_t offset) {
    return {*v.u32(offset), *v.u32(offset + 4), *v.u32(offset + 8)};
}

Result<void> check_range(const PeImage& image, const RuntimeFunction& f, std::string_view what) {
    if (f.begin >= f.end) {
        return table_error("runtime_function_invalid",
                           std::format("{} [0x{:x}, 0x{:x}) is empty or reversed", what, f.begin, f.end));
    }
    if (!executable(section_for(image, f.begin, f.end - f.begin))) {
        return table_error("runtime_function_invalid",
                           std::format("{} [0x{:x}, 0x{:x}) is not inside one executable section", what, f.begin,
                                       f.end));
    }
    return {};
}

struct UnwindSummary {
    bool handler = false;
    bool chained = false;
};

// Validates the UNWIND_INFO of `f` and, through CHAININFO, the entries it
// chains to. The prolog-length check applies to the directory entry only.
Result<UnwindSummary> check_unwind(ByteView file, const PeImage& image, RuntimeFunction f) {
    UnwindSummary summary;
    for (std::size_t depth = 0;; ++depth) {
        if (depth > kMaxUnwindChainDepth) {
            return table_error("unwind_chain_invalid",
                               std::format("unwind chain from function 0x{:x} is longer than {} entries", f.begin,
                                           kMaxUnwindChainDepth));
        }
        if (f.unwind % 4 != 0) {
            return table_error("unwind_info_invalid",
                               std::format("unwind info RVA 0x{:x} of function 0x{:x} is not 4-byte aligned", f.unwind,
                                           f.begin));
        }
        const auto header = file_range(file, image, f.unwind, kUnwindHeaderSize);
        if (!header) {
            return table_error("unwind_info_invalid",
                               std::format("unwind info header at RVA 0x{:x} (function 0x{:x}) is not backed by file "
                                           "data of one section",
                                           f.unwind, f.begin));
        }
        const std::uint8_t version_flags = *header->u8(0);
        const std::uint8_t version = version_flags & 0x7;
        const std::uint8_t flags = version_flags >> 3;
        const std::uint8_t prolog = *header->u8(1);
        const std::uint8_t codes = *header->u8(2);
        if (version != 1 && version != 2) {
            return table_error("unwind_info_invalid",
                               std::format("unwind info at RVA 0x{:x} has unsupported version {}", f.unwind, version));
        }
        if ((flags & ~kKnownFlags) != 0 ||
            ((flags & kFlagChainInfo) != 0 && (flags & (kFlagEHandler | kFlagUHandler)) != 0)) {
            return table_error("unwind_info_invalid",
                               std::format("unwind info at RVA 0x{:x} has invalid flags 0x{:x}", f.unwind, flags));
        }
        if (depth == 0 && prolog > f.end - f.begin) {
            return table_error("unwind_info_invalid",
                               std::format("prolog size {} of function 0x{:x} exceeds its length {}", prolog, f.begin,
                                           f.end - f.begin));
        }
        const std::uint64_t slots = (static_cast<std::uint64_t>(codes) + 1) & ~std::uint64_t{1};
        const std::uint64_t trailer_rva = static_cast<std::uint64_t>(f.unwind) + kUnwindHeaderSize + slots * 2;
        const std::uint64_t trailer_size = (flags & kFlagChainInfo) != 0 ? kRuntimeFunctionSize
                                           : flags != 0                  ? 4
                                                                         : 0;
        if (!file_range(file, image, f.unwind, trailer_rva + trailer_size - f.unwind)) {
            return table_error("unwind_info_invalid",
                               std::format("unwind info at RVA 0x{:x} ({} unwind codes) extends past its section's "
                                           "file data",
                                           f.unwind, codes));
        }
        if (trailer_size == 0) return summary;

        const auto trailer = *file_range(file, image, trailer_rva, trailer_size);
        if ((flags & kFlagChainInfo) == 0) {
            const std::uint32_t handler = *trailer.u32(0);
            if (!executable(section_for(image, handler, 1))) {
                return table_error("exception_handler_invalid",
                                   std::format("handler RVA 0x{:x} of function 0x{:x} is not inside an executable "
                                               "section",
                                               handler, f.begin));
            }
            if (depth == 0) summary.handler = true;
            return summary;
        }
        if (depth == 0) summary.chained = true;
        const RuntimeFunction chained = read_function(trailer, 0);
        if (auto r = check_range(image, chained, std::format("chained function of 0x{:x}", f.begin)); !r) {
            return table_error("unwind_chain_invalid", r.error().message);
        }
        f = chained;
    }
}

}  // namespace

Result<std::optional<ExceptionTable>> validate_exception_table(ByteView file, const PeImage& image) {
    const DataDirectory& dir = image.directory(DirectoryIndex::exception_table);
    if (!dir.present()) return std::optional<ExceptionTable>{};
    if (dir.size % kRuntimeFunctionSize != 0 || dir.size == 0) {
        return table_error("runtime_function_invalid",
                           std::format("exception directory size {} is not a nonzero multiple of 12", dir.size));
    }
    const std::uint32_t count = dir.size / kRuntimeFunctionSize;
    if (count > kMaxRuntimeFunctions) {
        return table_error("limit_exceeded", std::format("exception directory has {} entries (limit {})", count,
                                                         kMaxRuntimeFunctions));
    }
    if (dir.rva % 4 != 0) {
        return table_error("runtime_function_invalid",
                           std::format("exception directory RVA 0x{:x} is not 4-byte aligned", dir.rva));
    }
    const auto table = file_range(file, image, dir.rva, dir.size);
    if (!table) {
        return table_error("runtime_function_invalid",
                           std::format("exception directory [0x{:x}, +0x{:x}) is not backed by file data of one "
                                       "section",
                                       dir.rva, dir.size));
    }

    ExceptionTable result{dir.rva, count, 0, 0};
    std::uint32_t previous_end = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const RuntimeFunction f = read_function(*table, static_cast<std::size_t>(i) * kRuntimeFunctionSize);
        if (auto r = check_range(image, f, std::format("function {}", i)); !r) return std::unexpected(r.error());
        if (i != 0 && f.begin < previous_end) {
            return table_error("runtime_function_unsorted",
                               std::format("function {} at 0x{:x} starts before the end 0x{:x} of the previous entry",
                                           i, f.begin, previous_end));
        }
        previous_end = f.end;
        const auto unwind = check_unwind(file, image, f);
        if (!unwind) return std::unexpected(unwind.error());
        result.handler_count += unwind->handler ? 1 : 0;
        result.chained_count += unwind->chained ? 1 : 0;
    }
    return std::optional<ExceptionTable>{result};
}

}  // namespace scale::loader
