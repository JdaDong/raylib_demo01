#include "app/logger.hpp"

#include "raylib.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>

namespace rd {

namespace {
std::mutex g_mu;  // raylib may log from the audio thread
std::deque<LogLine> g_lines;
int g_counts[8] = {0};

const auto g_start = std::chrono::steady_clock::now();

void Callback(int level, const char* fmt, va_list args) {
    char buf[1024];
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    // NEVER call raylib (e.g. GetTime) in here: after CloseWindow() glfwGetTime()
    // raises a GLFW error -> TraceLog -> this callback -> infinite recursion (SIGSEGV).
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count();
    std::fprintf(stderr, "[%7.3f] %-5s %s\n", t, LogLevelName(level), buf);
    std::lock_guard<std::mutex> lock(g_mu);
    g_lines.push_back({level, buf});
    while (g_lines.size() > 200) g_lines.pop_front();
    for (int l = LOG_TRACE; l <= level && l < 8; ++l) ++g_counts[l];
}
}  // namespace

const char* LogLevelName(int level) {
    switch (level) {
        case LOG_TRACE: return "TRACE";
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_FATAL: return "FATAL";
        default: return "?";
    }
}

void InstallLogger(int minLevel) {
    SetTraceLogLevel(minLevel);
    SetTraceLogCallback(Callback);
}

std::vector<LogLine> RecentLogs(size_t max) {
    std::lock_guard<std::mutex> lock(g_mu);
    const size_t start = g_lines.size() > max ? g_lines.size() - max : 0;
    return {g_lines.begin() + static_cast<long>(start), g_lines.end()};
}

int LogCount(int level) {
    std::lock_guard<std::mutex> lock(g_mu);
    return (level >= 0 && level < 8) ? g_counts[level] : 0;
}

}  // namespace rd
