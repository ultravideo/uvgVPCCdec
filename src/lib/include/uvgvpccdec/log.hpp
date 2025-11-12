#pragma once
#include <cstdarg>
#include <iostream>
#include <string>
#include <mutex>

#ifndef UVG_LOG_LEVEL
#define UVG_LOG_LEVEL LogLevel::DEBUG
#endif

namespace uvgvpcc_dec {

enum class LogLevel { FATAL, ERROR, WARNING, INFO, PROFILING, TRACE, DEBUG };
constexpr LogLevel COMPILETIME_LOG_LEVEL = UVG_LOG_LEVEL;

// stringify table for LogLevel
static const std::string LogLevelStr[] = {"FATAL", "ERROR", "WARNING", "INFO", "PROFILING", "TRACE", "DEBUG"};

constexpr bool errorsAreFatalDefaultValue = true;
constexpr LogLevel logLevelDefaultValue = LogLevel::INFO;
constexpr std::ostream* outputDefaultValue = &std::cerr;

class Logger {
   public:
    static void setLogLevel(const LogLevel& level);
    static void setErrorsAreFatal(const bool& isFatal);
    static void setOutputStream(std::ostream& out);
    static LogLevel getLogLevel();
    static void printLogMessage(const std::string& context, LogLevel level, const std::string& message);

    template <LogLevel LEVEL>
    static void log(const std::string& context, const std::string& message) {
        static std::mutex logMutex;
        const std::lock_guard<std::mutex> lock(logMutex);
        if constexpr (LEVEL <= COMPILETIME_LOG_LEVEL) {
            if (LEVEL <= Logger::getLogLevel()) {
                printLogMessage(context, LEVEL, message);
            }
        }
    }
    static std::string printfStrToStdStr(const char* fmt, ...);
    static std::string vprintfStrToStdStr(const char* fmt, va_list args);

   private:
    static LogLevel logLevel;
    static bool errorsAreFatal_;
    static std::ostream* outputStream_;


};

}  // namespace uvgvpcc_dec