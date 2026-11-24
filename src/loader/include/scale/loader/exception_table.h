#pragma once

// Validation of the x64 exception directory (.pdata) and the unwind
// information it references (.xdata), for registration with the OS
// dispatcher (T-301). Portable: reads file bytes only (decision 0004).
//
// Sources: PE format, "The .pdata Section"
// (https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) and
// "x64 exception handling" (RUNTIME_FUNCTION, UNWIND_INFO, chained unwind
// info; https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64).
//
// Checked, for every RUNTIME_FUNCTION in the directory and every chained
// entry it reaches:
//   - BeginAddress < EndAddress, the whole range inside one executable section
//   - directory entries sorted by address and not overlapping
//   - UnwindInfoAddress 4-byte aligned; the UNWIND_INFO header, its unwind
//     code slots (CountOfCodes rounded up to even) and the trailing handler
//     RVA or chained RUNTIME_FUNCTION all backed by file data of one section
//   - Version 1 or 2; only the EHANDLER, UHANDLER and CHAININFO flags;
//     CHAININFO not combined with a handler flag
//   - a primary entry's SizeOfProlog does not exceed its function length
//   - a handler RVA lies inside an executable section
//   - chains are at most kMaxUnwindChainDepth long
// Not checked: the meaning of individual unwind codes and the
// language-specific handler data (both are interpreted by the OS dispatcher
// and the handler, not by SCALE).

#include "scale/common/byte_reader.h"
#include "scale/common/error.h"
#include "scale/loader/pe_image.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace scale::loader {

inline constexpr std::size_t kMaxRuntimeFunctions = 1u << 20;
inline constexpr std::size_t kMaxUnwindChainDepth = 32;

struct ExceptionTable {
    std::uint32_t rva = 0;                 // of the RUNTIME_FUNCTION array
    std::uint32_t function_count = 0;      // directory entries
    std::uint32_t handler_count = 0;       // directory entries with EHANDLER/UHANDLER
    std::uint32_t chained_count = 0;       // directory entries with CHAININFO
};

// nullopt when the image has no exception directory. `file` must be the
// bytes `image` was inspected from.
// Error codes (component "loader"): runtime_function_invalid,
// runtime_function_unsorted, unwind_info_invalid, exception_handler_invalid,
// unwind_chain_invalid, limit_exceeded.
[[nodiscard]] Result<std::optional<ExceptionTable>> validate_exception_table(ByteView file, const PeImage& image);

}  // namespace scale::loader
