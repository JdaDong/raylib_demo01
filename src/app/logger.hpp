// logger.hpp - SetTraceLogCallback sink: stderr + in-memory ring for the F1 console
#pragma once
#include <string>
#include <vector>

namespace rd {

struct LogLine {
    int level;
    std::string text;
};

void InstallLogger(int minLevel);
std::vector<LogLine> RecentLogs(size_t max = 12);
int LogCount(int level);  // number of lines logged at >= level
const char* LogLevelName(int level);

}  // namespace rd
