// T-204: import resolution policy (all provider sources empty in this slice).

#include "scale/runtime/import_resolver.h"

#include "../loader/pe_builder.h"

#include <gtest/gtest.h>

using namespace scale::runtime;
using scale::test::ImportSpec;
using scale::test::PeBuilder;

namespace {

scale::loader::PeImage image_with_imports(const std::vector<ImportSpec>& modules) {
    PeBuilder pe;
    pe.entry_rva = pe.add_section(".text", scale::test::Bytes(16, std::byte{0xC3}), PeBuilder::kText);
    if (!modules.empty()) {
        const std::uint32_t rva = pe.next_section_rva();
        const auto idata = scale::test::build_import_section(rva, modules);
        pe.add_section(".idata", idata.data, PeBuilder::kRdata);
        pe.set_directory(scale::loader::DirectoryIndex::import_table, rva, idata.directory_size);
    }
    const auto bytes = pe.build();
    auto r = scale::loader::inspect_pe(scale::ByteView(bytes));
    EXPECT_TRUE(r.has_value()) << r.error().to_string();
    return r ? *r : scale::loader::PeImage{};
}

}  // namespace

TEST(ImportResolver, NoImportsIsComplete) {
    const auto res = resolve_imports(image_with_imports({}), "a.exe");
    EXPECT_TRUE(res.complete());
    EXPECT_EQ(res.total, 0u);
}

TEST(ImportResolver, EveryImportIsUnresolvedWithoutProviders) {
    const auto res = resolve_imports(
        image_with_imports({ImportSpec{"test_platform.dll", {std::string("TestAllocate"), std::uint16_t{2}}},
                            ImportSpec{"KERNEL32.dll", {std::string("ExitProcess")}}}),
        "game.exe");
    EXPECT_FALSE(res.complete());
    EXPECT_EQ(res.total, 3u);
    ASSERT_EQ(res.unresolved.size(), 3u);

    // Windows exports are not permitted implicitly either: KERNEL32 is unresolved.
    EXPECT_EQ(res.unresolved[2].request.module, "KERNEL32.dll");
    EXPECT_EQ(res.unresolved[2].request.symbol, "ExitProcess");

    const auto by_name = to_event(res.unresolved[0]);
    EXPECT_EQ(by_name.component, "runtime");
    EXPECT_EQ(by_name.stage, "import_resolution");
    EXPECT_EQ(by_name.code, "unresolved_import");
    EXPECT_TRUE(by_name.fatal);
    EXPECT_EQ(by_name.severity, scale::diagnostics::Severity::error);
    const std::vector<scale::diagnostics::Attribute> name_attrs = {{"module", "test_platform.dll"},
                                                                   {"symbol", "TestAllocate"},
                                                                   {"importing_module", "game.exe"},
                                                                   {"import_kind", "by_name"}};
    EXPECT_EQ(by_name.attributes, name_attrs);

    const auto by_ordinal = to_event(res.unresolved[1]);
    const std::vector<scale::diagnostics::Attribute> ordinal_attrs = {{"module", "test_platform.dll"},
                                                                      {"ordinal", "2"},
                                                                      {"importing_module", "game.exe"},
                                                                      {"import_kind", "by_ordinal"}};
    EXPECT_EQ(by_ordinal.attributes, ordinal_attrs);
}
