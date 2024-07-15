#pragma once
#include <cstdarg>
#include <iostream>
#include <string>

namespace uvgvpcc_dec {
enum class LogLevel { FATAL, ERROR, WARNING, INFO, PROFILING, TRACE, DEBUG };

// stringify table for LogLevel
static const char* LogLevelStr[] = {"FATAL", "ERROR", "WARNING", "INFO", "PROFILING", "TRACE", "DEBUG"};

class Logger {
   public:
    static void setLogLevel(LogLevel level);
    static LogLevel getLogLevel();
    static void log(LogLevel level, const std::string context, const std::string& message);
    static std::string printfStrToStdStr(const char* fmt, ...);
    static std::string vprintfStrToStdStr(const char* fmt, va_list args);

   private:
    static LogLevel logLevel;
};
}  // namespace uvgvpcc_dec