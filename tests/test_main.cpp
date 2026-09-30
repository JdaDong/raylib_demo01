// test_main.cpp - runner: unit_tests [--suite NAME] [--filter SUBSTR] [--list] [-v]
#include "testing.hpp"

#include "raylib.h"

#include <chrono>
#include <cstring>
#include <set>

int main(int argc, char** argv) {
    const char* suite = nullptr;
    const char* filter = nullptr;
    bool list = false, verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--suite") && i + 1 < argc) suite = argv[++i];
        else if (!std::strcmp(argv[i], "--filter") && i + 1 < argc) filter = argv[++i];
        else if (!std::strcmp(argv[i], "--list")) list = true;
        else if (!std::strcmp(argv[i], "-v")) verbose = true;
    }
    // raylib logs INFO for every image/file op; keep test output readable
    SetTraceLogLevel(verbose ? LOG_INFO : LOG_ERROR);
    MakeDirectory(RD_TEST_TMP);

    if (list) {
        std::set<std::string> suites;
        for (auto& t : rt::Registry()) std::printf("%s.%s\n", t.suite, t.name);
        return 0;
    }

    int run = 0, failedTests = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (auto& t : rt::Registry()) {
        if (suite && std::strcmp(suite, t.suite) != 0) continue;
        const std::string full = std::string(t.suite) + "." + t.name;
        if (filter && full.find(filter) == std::string::npos) continue;
        ++run;
        const int before = rt::Ctx().failures;
        try {
            t.fn();
        } catch (const rt::RequireFailed&) {
        } catch (const std::exception& e) {
            rt::ReportFailure(t.file, t.line, std::string("exception: ") + e.what());
        }
        const bool ok = rt::Ctx().failures == before;
        if (!ok) ++failedTests;
        if (!ok || verbose) std::printf("[%s] %s\n", ok ? " OK " : "FAIL", full.c_str());
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("%s%d tests, %d checks, %d failed tests (%.1f ms)\n", failedTests ? "FAILED: " : "PASSED: ", run,
                rt::Ctx().checks, failedTests, ms);
    if (run == 0) {
        std::fprintf(stderr, "no tests matched\n");
        return 2;
    }
    return failedTests ? 1 : 0;
}
