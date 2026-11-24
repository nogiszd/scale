#include "scale_test.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

namespace scale::test {
namespace {

struct Case {
    std::string name;
    TestFn fn;
};

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

int g_case_failures = 0;

void print(const std::string& text) {
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
}

}  // namespace

void register_test(std::string_view name, TestFn fn) {
    registry().push_back(Case{std::string(name), fn});
}

void record_failure(std::string_view expression, std::string_view detail, std::source_location where) {
    ++g_case_failures;
    std::string text = std::format("  {}({}): check failed: {}", where.file_name(), where.line(), expression);
    if (!detail.empty()) {
        text += std::format(" [{}]", detail);
    }
    text += '\n';
    print(text);
}

}  // namespace scale::test

int main(int argc, char** argv) {
    using namespace scale::test;

    std::vector<std::string> filter(argv + 1, argv + argc);
    int passed = 0;
    int failed = 0;
    int skipped = 0;

    for (const Case& c : registry()) {
        if (!filter.empty() && std::find(filter.begin(), filter.end(), c.name) == filter.end()) {
            continue;
        }
        g_case_failures = 0;
        std::string status;
        try {
            c.fn();
            status = g_case_failures == 0 ? "PASS" : "FAIL";
        } catch (const SkipCase& s) {
            status = g_case_failures == 0 ? "SKIP (" + s.reason + ")" : "FAIL";
        } catch (const AbortCase&) {
            status = "FAIL";
        } catch (const std::exception& e) {
            print(std::format("  unexpected exception: {}\n", e.what()));
            status = "FAIL";
        } catch (...) {
            print("  unexpected non-standard exception\n");
            status = "FAIL";
        }
        print(std::format("[{}] {}\n", status, c.name));
        if (status == "PASS") {
            ++passed;
        } else if (status == "FAIL") {
            ++failed;
        } else {
            ++skipped;
        }
    }

    const int ran = passed + failed + skipped;
    print(std::format("{} passed, {} failed, {} skipped\n", passed, failed, skipped));
    if (ran == 0) {
        print("error: no test cases matched\n");
        return 1;
    }
    if (failed != 0) {
        return 1;
    }
    return skipped != 0 ? 77 : 0;
}
