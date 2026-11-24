# Testing contract

CTest is the only runner. Helpers are in `cmake/ScaleTesting.cmake` and the
harness is in `tests/support/scale_test.h`.

## Registering tests

```cmake
scale_add_test(NAME unit_loader
    SOURCES loader/pe_headers_test.cpp
    LIBS scale::loader
    LABELS quick)

scale_add_command_test(NAME cli_inspect_ret42
    COMMAND $<TARGET_FILE:scale> inspect $<TARGET_FILE:fixture_ret42>
    EXPECT_EXIT 0
    EXPECT_OUTPUT "entry_point_rva"
    LABELS integration)
```

- Each test has exactly one primary label: `quick`, `integration` or `gpu`.
  Configuration fails otherwise. Tests on a software Vulkan device also carry
  the label `software_gpu` and must say so in their output.
- The default timeout is 30 s. Pass `TIMEOUT` for longer tests.
- `scale_add_command_test` checks the exact exit code and, optionally, that the
  combined stdout and stderr match a regular expression.

## Harness

```cpp
#include "scale_test.h"
SCALE_TEST(parses_minimal_header) {
    auto r = scale::loader::inspect_pe(view);
    SCALE_REQUIRE(r.has_value());          // stops the case on failure
    SCALE_CHECK_EQ(r->machine, 0x8664u);   // records and continues
}
SCALE_TEST(gpu_readback) {
    if (!device) SCALE_SKIP("no Vulkan device");  // never a pass
}
```

Executable exit codes: `0` all passed, `1` at least one failure, `77` no
failures but at least one skip (CTest shows SKIPPED). Pass case names as
arguments to run a subset.

## Rules

- Deterministic. Seeded generators must print their seed.
- Assert on `Error::code`, never on message text.
- Every feature needs a positive and a negative test.
- Fixtures are source built by CMake. Never commit binaries.
- Missing hardware or tools means SKIPPED. Milestone acceptance that needs
  them stays incomplete until they actually run.
- Tests write only under their build directory or a dedicated temp root.
- Preset runs: `quick`, `integration`, `gpu`, `full`, `release-full`.
  Run `full` before moving a task to REVIEW.
