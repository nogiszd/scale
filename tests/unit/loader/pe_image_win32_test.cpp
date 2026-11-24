// Windows-only loader inspection tests (decision 0004: portable tests use no OS APIs).

#include "scale/loader/pe_image.h"

#include <gtest/gtest.h>

#include <windows.h>

#include <filesystem>

using namespace scale::loader;

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

