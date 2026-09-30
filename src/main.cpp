// raylib_demo01 - NEON HARVEST: a 2D arena shooter that tours the raylib 6.0 API.
#include "app/app.hpp"

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
            std::fputs(rd::UsageText(), stdout);
            return 0;
        }
        if (!std::strcmp(argv[i], "--version")) {
            std::printf("raylib_demo01 %s (raylib %s)\n", RD_VERSION, RAYLIB_VERSION);
            return 0;
        }
    }
    rd::AppOptions opts;
    std::string err;
    if (!rd::ParseArgs(argc, argv, opts, err)) {
        std::fprintf(stderr, "error: %s\n\n%s", err.c_str(), rd::UsageText());
        return 2;
    }
    rd::App app(opts);
    return app.run();
}
