// T-102: compares scale::loader::inspect_pe_file against dumpbin (an
// independent Microsoft tool) for the self-authored fixtures in
// tests/fixtures/pe, and checks each fixture's documented contract.
//
// The fixtures are only read, never executed. If dumpbin is unavailable the
// comparison cases are SKIPPED (GTEST_SKIP), never passed.

#include "scale/common/text.h"
#include "scale/loader/pe_image.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace scale::loader;

namespace {

std::filesystem::path utf8_path(const char* utf8) {
    auto wide = scale::to_wide(utf8);
    return wide ? std::filesystem::path(*wide) : std::filesystem::path();
}

// ---- dumpbin runner --------------------------------------------------------------

std::optional<std::string> run_dumpbin(const std::string& option, const std::filesystem::path& file) {
    const std::filesystem::path dumpbin = utf8_path(SCALE_DUMPBIN_PATH);
    // cmd.exe strips the outer quotes of the whole command line.
    const std::wstring command = L"\"\"" + dumpbin.wstring() + L"\" /nologo " +
                                 *scale::to_wide(option) + L" \"" + file.wstring() + L"\"\"";
    FILE* pipe = ::_wpopen(command.c_str(), L"rt");
    if (pipe == nullptr) {
        return std::nullopt;
    }
    std::string output;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    if (::_pclose(pipe) != 0) {
        return std::nullopt;
    }
    return output;
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

std::uint64_t hex(const std::string& s) {
    return std::stoull(s, nullptr, 16);
}

// ---- dumpbin output model (parsed independently of the inspector) ------------------

struct DumpSection {
    std::string name;
    std::uint64_t virtual_size = 0, virtual_address = 0, raw_size = 0, raw_offset = 0, flags = 0;
};

struct DumpImport {
    std::string module;
    std::uint64_t iat_va = 0;
    std::vector<std::string> entries;  // "name:<symbol>:<hint>" or "ordinal:<n>"
};

struct Dump {
    std::map<std::string, std::uint64_t> header;  // "entry point", "image base", ...
    std::vector<std::pair<std::uint64_t, std::uint64_t>> directories;
    std::vector<DumpSection> sections;
    std::vector<DumpImport> imports;
    std::uint64_t relocation_blocks = 0;
    std::map<std::string, std::uint64_t> relocation_types;  // "ABS", "DIR64", ...
};

void parse_headers(const std::string& text, Dump& d) {
    static const std::regex value_line(R"(^\s+([0-9A-F]+) (machine|number of sections|entry point|image base|section alignment|file alignment|size of image|size of headers|DLL characteristics|number of directories|characteristics)\b)");
    static const std::regex subsystem_line(R"(^\s+([0-9A-F]+) subsystem \()");
    static const std::regex directory_line(R"(^\s+([0-9A-F]+) \[\s*([0-9A-F]+)\] RVA \[size\] of )");
    static const std::regex section_start(R"(^SECTION HEADER #\d+$)");
    static const std::regex section_field(R"(^\s*([0-9A-F]+) (virtual size|virtual address|size of raw data|file pointer to raw data|flags)\b)");
    static const std::regex section_name(R"(^\s*(\S+) name$)");

    DumpSection* current = nullptr;
    for (const std::string& line : lines_of(text)) {
        std::smatch m;
        if (std::regex_search(line, m, section_start)) {
            d.sections.emplace_back();
            current = &d.sections.back();
        } else if (current != nullptr && std::regex_search(line, m, section_name)) {
            current->name = m[1];
        } else if (current != nullptr && std::regex_search(line, m, section_field)) {
            const std::uint64_t v = hex(m[1]);
            const std::string field = m[2];
            if (field == "virtual size") current->virtual_size = v;
            else if (field == "virtual address") current->virtual_address = v;
            else if (field == "size of raw data") current->raw_size = v;
            else if (field == "file pointer to raw data") current->raw_offset = v;
            else current->flags = v;
        } else if (current == nullptr && std::regex_search(line, m, directory_line)) {
            d.directories.emplace_back(hex(m[1]), hex(m[2]));
        } else if (current == nullptr && std::regex_search(line, m, subsystem_line)) {
            d.header["subsystem"] = hex(m[1]);
        } else if (current == nullptr && std::regex_search(line, m, value_line)) {
            d.header.try_emplace(m[2], hex(m[1]));
        }
    }
}

void parse_imports(const std::string& text, Dump& d) {
    static const std::regex module_line(R"(^    (\S+)$)");
    static const std::regex iat_line(R"(^\s+([0-9A-F]+) Import Address Table$)");
    static const std::regex name_entry(R"(^\s+([0-9A-F]+) (\S+)$)");
    static const std::regex ordinal_entry(R"(^\s+Ordinal\s+(\d+)$)");
    const auto lines = lines_of(text);
    DumpImport* current = nullptr;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        std::smatch m;
        if (line.find("Summary") != std::string::npos) {
            break;
        }
        if (i + 1 < lines.size() && std::regex_search(line, m, module_line) &&
            lines[i + 1].find("Import Address Table") != std::string::npos) {
            d.imports.push_back(DumpImport{m[1], 0, {}});
            current = &d.imports.back();
        } else if (current != nullptr && std::regex_search(line, m, iat_line)) {
            current->iat_va = hex(m[1]);
        } else if (current != nullptr && std::regex_search(line, m, ordinal_entry)) {
            current->entries.push_back("ordinal:" + std::string(m[1]));
        } else if (current != nullptr && std::regex_search(line, m, name_entry)) {
            current->entries.push_back("name:" + std::string(m[2]) + ":" + std::to_string(hex(m[1])));
        }
    }
}

void parse_relocations(const std::string& text, Dump& d) {
    static const std::regex block_line(R"(^\s+[0-9A-F]+ RVA,\s+[0-9A-F]+ SizeOfBlock$)");
    static const std::regex entry_line(R"(^\s+[0-9A-F]+\s+(ABS|HIGH|LOW|HIGHLOW|DIR64)\b)");
    for (const std::string& line : lines_of(text)) {
        std::smatch m;
        if (std::regex_search(line, m, block_line)) {
            ++d.relocation_blocks;
        } else if (std::regex_search(line, m, entry_line)) {
            ++d.relocation_types[m[1]];
        }
    }
}

std::optional<Dump> dump(const std::filesystem::path& file) {
    const auto headers = run_dumpbin("/headers", file);
    const auto imports = run_dumpbin("/imports", file);
    const auto relocs = run_dumpbin("/relocations", file);
    if (!headers || !imports || !relocs) {
        return std::nullopt;
    }
    Dump d;
    parse_headers(*headers, d);
    parse_imports(*imports, d);
    parse_relocations(*relocs, d);
    return d;
}

// ---- Comparison --------------------------------------------------------------------

std::vector<std::string> inspector_entries(const ImportModule& m) {
    std::vector<std::string> out;
    for (const ImportEntry& e : m.entries) {
        out.push_back(e.by_ordinal ? "ordinal:" + std::to_string(e.ordinal)
                                   : "name:" + e.name + ":" + std::to_string(e.hint));
    }
    return out;
}

void compare_with_dumpbin(const std::filesystem::path& file, const PeImage& img) {
    if (std::string_view(SCALE_DUMPBIN_PATH).empty()) {
        GTEST_SKIP() << "dumpbin was not found at configure time";
    }
    const auto d = dump(file);
    ASSERT_TRUE(d.has_value()) << "dumpbin failed for " << file.string();

    EXPECT_EQ(d->header.at("machine"), img.machine);
    EXPECT_EQ(d->header.at("number of sections"), img.sections.size());
    EXPECT_EQ(d->header.at("characteristics"), img.characteristics);
    EXPECT_EQ(d->header.at("entry point"), img.address_of_entry_point);
    EXPECT_EQ(d->header.at("image base"), img.image_base);
    EXPECT_EQ(d->header.at("section alignment"), img.section_alignment);
    EXPECT_EQ(d->header.at("file alignment"), img.file_alignment);
    EXPECT_EQ(d->header.at("size of image"), img.size_of_image);
    EXPECT_EQ(d->header.at("size of headers"), img.size_of_headers);
    EXPECT_EQ(d->header.at("subsystem"), img.subsystem);
    EXPECT_EQ(d->header.at("DLL characteristics"), img.dll_characteristics);
    EXPECT_EQ(d->header.at("number of directories"), img.number_of_rva_and_sizes);

    ASSERT_EQ(d->directories.size(), kDirectoryCount);
    for (std::size_t i = 0; i < kDirectoryCount; ++i) {
        EXPECT_EQ(d->directories[i].first, img.directories[i].rva) << directory_name(static_cast<DirectoryIndex>(i));
        EXPECT_EQ(d->directories[i].second, img.directories[i].size) << directory_name(static_cast<DirectoryIndex>(i));
    }

    ASSERT_EQ(d->sections.size(), img.sections.size());
    for (std::size_t i = 0; i < img.sections.size(); ++i) {
        const Section& s = img.sections[i];
        const DumpSection& ds = d->sections[i];
        EXPECT_EQ(ds.name, s.name);
        EXPECT_EQ(ds.virtual_size, s.virtual_size) << s.name;
        EXPECT_EQ(ds.virtual_address, s.virtual_address) << s.name;
        EXPECT_EQ(ds.raw_size, s.raw_size) << s.name;
        EXPECT_EQ(ds.raw_offset, s.raw_offset) << s.name;
        EXPECT_EQ(ds.flags, s.characteristics) << s.name;
    }

    ASSERT_EQ(d->imports.size(), img.imports.size());
    for (std::size_t i = 0; i < img.imports.size(); ++i) {
        EXPECT_EQ(d->imports[i].module, img.imports[i].dll_name);
        EXPECT_EQ(d->imports[i].iat_va, img.image_base + img.imports[i].import_address_table_rva);
        // dumpbin lists ordinal imports in a different order than the table
        // (observed); compare as sorted sets of entries.
        auto expected = d->imports[i].entries;
        auto actual = inspector_entries(img.imports[i]);
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(), actual.end());
        EXPECT_EQ(expected, actual) << img.imports[i].dll_name;
    }

    const std::uint64_t blocks = img.relocations ? img.relocations->block_count : 0;
    EXPECT_EQ(d->relocation_blocks, blocks);
    const auto type_count = [&](unsigned type) -> std::uint64_t {
        return img.relocations ? img.relocations->count_by_type[type] : 0;
    };
    const auto dump_count = [&](const char* name) -> std::uint64_t {
        const auto it = d->relocation_types.find(name);
        return it == d->relocation_types.end() ? 0 : it->second;
    };
    EXPECT_EQ(dump_count("ABS"), type_count(0));
    EXPECT_EQ(dump_count("HIGH"), type_count(1));
    EXPECT_EQ(dump_count("LOW"), type_count(2));
    EXPECT_EQ(dump_count("HIGHLOW"), type_count(3));
    EXPECT_EQ(dump_count("DIR64"), type_count(10));
}

PeImage inspect_fixture(const char* path_utf8) {
    const auto r = inspect_pe_file(utf8_path(path_utf8));
    EXPECT_TRUE(r.has_value()) << (r ? "" : r.error().to_string());
    return r ? *r : PeImage{};
}

}  // namespace

// ---- fixture_exit.exe ---------------------------------------------------------------

TEST(PeFixtureExit, MatchesContract) {
    const PeImage img = inspect_fixture(SCALE_FIXTURE_EXIT);
    EXPECT_EQ(img.machine, kMachineAmd64);
    EXPECT_FALSE(img.is_dll());
    EXPECT_NE(img.address_of_entry_point, 0u);
    ASSERT_EQ(img.imports.size(), 1u);
    EXPECT_EQ(img.imports[0].dll_name, "KERNEL32.dll");
    ASSERT_EQ(img.imports[0].entries.size(), 1u);
    EXPECT_EQ(img.imports[0].entries[0].name, "ExitProcess");
    ASSERT_TRUE(img.relocations.has_value());
    EXPECT_GE(img.relocations->count_by_type[10], 1u);  // pointer to fixture_value
    EXPECT_FALSE(img.tls.has_value());
}

TEST(PeFixtureExit, MatchesDumpbin) {
    compare_with_dumpbin(utf8_path(SCALE_FIXTURE_EXIT), inspect_fixture(SCALE_FIXTURE_EXIT));
}

// ---- fixture_lib.dll ----------------------------------------------------------------

TEST(PeFixtureLib, MatchesContract) {
    const PeImage img = inspect_fixture(SCALE_FIXTURE_LIB);
    EXPECT_TRUE(img.is_dll());
    EXPECT_TRUE(img.directory(DirectoryIndex::export_table).present());
    EXPECT_TRUE(img.imports.empty());
}

TEST(PeFixtureLib, MatchesDumpbin) {
    compare_with_dumpbin(utf8_path(SCALE_FIXTURE_LIB), inspect_fixture(SCALE_FIXTURE_LIB));
}

// ---- fixture_ordinal.exe ------------------------------------------------------------

TEST(PeFixtureOrdinal, MatchesContract) {
    const PeImage img = inspect_fixture(SCALE_FIXTURE_ORDINAL);
    ASSERT_EQ(img.imports.size(), 2u);
    const ImportModule* lib = nullptr;
    for (const ImportModule& m : img.imports) {
        if (m.dll_name == "fixture_lib.dll") lib = &m;
    }
    ASSERT_NE(lib, nullptr);
    bool ordinal7 = false;
    bool mul_by_name = false;
    for (const ImportEntry& e : lib->entries) {
        ordinal7 = ordinal7 || (e.by_ordinal && e.ordinal == 7);
        mul_by_name = mul_by_name || (!e.by_ordinal && e.name == "FixtureMul");
    }
    EXPECT_TRUE(ordinal7) << "FixtureAdd must be imported by ordinal 7";
    EXPECT_TRUE(mul_by_name) << "FixtureMul must be imported by name";
}

TEST(PeFixtureOrdinal, MatchesDumpbin) {
    compare_with_dumpbin(utf8_path(SCALE_FIXTURE_ORDINAL), inspect_fixture(SCALE_FIXTURE_ORDINAL));
}
