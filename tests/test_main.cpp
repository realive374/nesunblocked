#include <cstring>

#include "test.h"

namespace rt90test {

namespace {
int g_failures = 0;
}

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

void report_failure(const char* file, int line, const std::string& msg) {
    ++g_failures;
    std::fprintf(stderr, "    FAIL %s:%d: %s\n", file, line, msg.c_str());
}

}  // namespace rt90test

// Usage: rt90_tests [substring-filter]
int main(int argc, char** argv) {
    using namespace rt90test;
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failed = 0;
    for (const TestCase& t : registry()) {
        if (filter && !std::strstr(t.name, filter)) continue;
        const int before = g_failures;
        t.fn();
        ++run;
        const bool ok = g_failures == before;
        if (!ok) ++failed;
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", t.name);
    }
    std::printf("\n%d test(s), %d failed\n", run, failed);
    return failed == 0 ? 0 : 1;
}
