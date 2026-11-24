#pragma once

// Import resolution policy (AGENTS.md, M3), first slice (T-204).
//
// Every import of an image is offered, in order, to:
//   1. an explicit, documented adapter   (none registered yet)
//   2. an approved guest module          (none approved yet)
//   3. an explicitly permitted Windows export (none permitted yet)
// and is otherwise UNRESOLVED. There is no DLL search path and no
// name-based fallback, and no callable stub is ever generated. An image with
// any unresolved required import must not be executed.
//
// While all three sources are empty, every import is unresolved. That is
// the correct, explicit result until adapters with verified signatures exist.

#include "scale/diagnostics/diagnostic_log.h"
#include "scale/loader/pe_image.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scale::runtime {

enum class ImportKind { by_name, by_ordinal };

[[nodiscard]] std::string_view to_string(ImportKind kind) noexcept;  // "by_name" / "by_ordinal"

struct ImportRequest {
    std::string module;               // as written in the import table
    ImportKind kind = ImportKind::by_name;
    std::string symbol;               // by_name
    std::uint16_t ordinal = 0;        // by_ordinal
};

struct UnresolvedImport {
    ImportRequest request;
    std::string importing_module;
    std::string reason;
};

struct ImportResolution {
    std::size_t total = 0;  // imports examined
    std::vector<UnresolvedImport> unresolved;

    [[nodiscard]] bool complete() const noexcept { return unresolved.empty(); }
};

// Applies the policy to every import of `image`. `importing_module` names the
// image in diagnostics (usually its file name).
[[nodiscard]] ImportResolution resolve_imports(const loader::PeImage& image, std::string_view importing_module);

// Fatal `runtime/import_resolution/unresolved_import` event with attributes
// module, symbol or ordinal, importing_module, import_kind.
[[nodiscard]] diagnostics::DiagnosticEvent to_event(const UnresolvedImport& unresolved);

}  // namespace scale::runtime
