// Probe for the test-infrastructure self-tests in tests/unit/CMakeLists.txt.
// Not registered with CTest directly: Skipped and Failing are intentional.

#include <gtest/gtest.h>

TEST(Probe, Passing) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(Probe, Skipped) {
    GTEST_SKIP() << "probe skip";
}

TEST(Probe, Failing) {
    EXPECT_EQ(1 + 1, 3);
}
