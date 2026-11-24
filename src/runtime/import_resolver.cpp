#include "scale/runtime/import_resolver.h"

#include <format>

namespace scale::runtime {
namespace {

constexpr std::string_view kNoProvider =
    "no documented adapter, approved guest module or permitted Windows export provides this import";

}  // namespace

std::string_view to_string(ImportKind kind) noexcept {
    return kind == ImportKind::by_name ? "by_name" : "by_ordinal";
}

ImportResolution resolve_imports(const loader::PeImage& image, std::string_view importing_module) {
    ImportResolution resolution;
    for (const loader::ImportModule& module : image.imports) {
        for (const loader::ImportEntry& entry : module.entries) {
            ++resolution.total;
            ImportRequest request{module.dll_name, entry.by_ordinal ? ImportKind::by_ordinal : ImportKind::by_name,
                                  entry.by_ordinal ? std::string() : entry.name, entry.ordinal};
            // Policy sources (adapters, approved guest modules, permitted
            // Windows exports) are all empty in this slice; see the header.
            resolution.unresolved.push_back(
                UnresolvedImport{std::move(request), std::string(importing_module), std::string(kNoProvider)});
        }
    }
    return resolution;
}

diagnostics::DiagnosticEvent to_event(const UnresolvedImport& u) {
    std::vector<diagnostics::Attribute> attributes;
    attributes.push_back({"module", u.request.module});
    if (u.request.kind == ImportKind::by_name) {
        attributes.push_back({"symbol", u.request.symbol});
    } else {
        attributes.push_back({"ordinal", std::to_string(u.request.ordinal)});
    }
    attributes.push_back({"importing_module", u.importing_module});
    attributes.push_back({"import_kind", std::string(to_string(u.request.kind))});
    return diagnostics::DiagnosticEvent{"runtime",
                                        "import_resolution",
                                        "unresolved_import",
                                        diagnostics::Severity::error,
                                        true,
                                        u.reason,
                                        std::move(attributes)};
}

}  // namespace scale::runtime
