# Testing contract

GoogleTest 1.18.0 (pinned, SHA-256 verified, fetched by CMake at configure
time) run through CTest. Helpers: `cmake/ScaleTesting.cmake`. Do not write
custom assertion macros or test mains.

## Registering tests

```cmake
# GoogleTest executable; every TEST() becomes its own CTest test named
# unit_loader.<Suite>.<Case>.
scale_add_test(NAME unit_loader
    SOURCES loader/pe_headers_test.cpp
    LIBS scale::loader
    LABELS quick)

# Process-level check (CLI exit code + output).
scale_add_command_test(NAME cli_inspect_ret42
    COMMAND $<TARGET_FILE:scale> inspect $<TARGET_FILE:fixture_ret42>
    EXPECT_EXIT 0
    EXPECT_OUTPUT "entry_point_rva"
    LABELS integration)
```

- Each test has exactly one primary label: `quick`, `integration` or `gpu`.
  Configuration fails otherwise. Tests on a software Vulkan device also carry
  the label `software_gpu` and must say so in their output.
- The default timeout is 30 s per case. Pass `TIMEOUT` for longer tests.
- `scale_add_test` links `GTest::gtest_main`. Provide your own `main` only if
  a component truly needs global setup, and link `GTest::gtest` instead.
- gMock is not built. Ask the coordinator if you need it.

## Writing tests

```cpp
#include <gtest/gtest.h>

TEST(PeHeaders, ParsesMinimalImage) {
    auto r = scale::loader::inspect_pe(view);
    ASSERT_TRUE(r.has_value()) << r.error().to_string();
    EXPECT_EQ(r->machine, 0x8664u);
}

TEST(VulkanCompute, Readback) {
    if (!device) GTEST_SKIP() << "no Vulkan device";   // CTest: Skipped
}
```

- `GTEST_SKIP()` is reported by CTest as **Skipped**, never Passed. This works
  because `scale_add_test` sets `SKIP_REGULAR_EXPRESSION` to GoogleTest's
  `[  SKIPPED ]` marker, and it is checked by the `selftest_gtest_*` tests.
  A skip inside a test that already failed still counts as a failure.
- Assert on `Error::code`, never on message text.
- Every feature needs a positive and a negative test.

## Rules

- Deterministic. Seeded generators must print their seed (e.g. with
  `SCOPED_TRACE` or test output).
- Fixtures are source built by CMake. Never commit binaries.
- Missing hardware or tools means SKIPPED. Milestone acceptance that needs
  them stays incomplete until they actually run.
- Tests write only under their build directory or a dedicated temp root.
- Preset runs: `quick`, `integration`, `gpu`, `full`, `release-full`.
  Run `full` before moving a task to REVIEW.
- Offline builds: configure with
  `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<extracted googletest-1.18.0>`.
