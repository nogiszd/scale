// Probe executable for the test harness self-tests in tests/unit/CMakeLists.txt.

#include "scale_test.h"

SCALE_TEST(passing_case) {
    SCALE_CHECK(1 + 1 == 2);
}

SCALE_TEST(skipped_case) {
    SCALE_SKIP("probe skip");
}

SCALE_TEST(failing_case) {
    SCALE_CHECK_EQ(1 + 1, 3);
}
