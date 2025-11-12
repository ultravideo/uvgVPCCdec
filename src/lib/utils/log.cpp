#include "uvgvpccdec/log.hpp"

#include <cstdarg>
#include <ctime>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>

// NOLINTNEXTLINE(hicpp-deprecated-headers)
#include <stdio.h>  // Needed for vasprintf

#include "chrono.hpp"

namespace uvgvpcc_dec {


namespace {

// Define color codes using const char*
constexpr const char* RED = "\x1B[31m";
constexpr const char* YEL = "\x1B[33m";
constexpr const char* BLU = "\x1B[34m";
constexpr const char* CYN = "\x1B[36m";
constexpr const char* GRN = "\x1B[32m";
constexpr const char* MAG = "\x1B[35m";
constexpr const char* BLD = "\x1B[1m";
constexpr const char* RST = "\x1B[0m";
constexpr const char* REDANDBLD = "\x1B[31m\x1B[1m";

constexpr const char* colorForLevel(LogLevel level) {
    switch (level) {
        case LogLevel::FATAL:
            return REDANDBLD;
        case LogLevel::ERROR:
            return RED;
        case LogLevel::WARNING:
            return YEL;
        case LogLevel::INFO:
            return BLU;
        case LogLevel::PROFILING:
            return CYN;
        case LogLevel::TRACE:
            return GRN;
        case LogLevel::DEBUG:
            return MAG;
        default:
            return RST;
    }
}

inline std::string getLogPrefix(const std::string& context, LogLevel level) {
    const std::string elapsedStr = global_timer.elapsed_str();
    return "[" + elapsedStr + "][" + LogLevelStr[static_cast<int>(level)] + "]\t[" + context + "] ";
}
}  // anonymous namespace

LogLevel Logger::logLevel = logLevelDefaultValue;
bool Logger::errorsAreFatal_ = errorsAreFatalDefaultValue;
std::ostream* Logger::outputStream_ = outputDefaultValue;

void Logger::setLogLevel(const LogLevel& level) { logLevel = level; }
void Logger::setErrorsAreFatal(const bool& isFatal) { errorsAreFatal_ = isFatal; }
void Logger::setOutputStream(std::ostream& out) { outputStream_ = &out; }
LogLevel Logger::getLogLevel() { return logLevel; }

void Logger::printLogMessage(const std::string& context, LogLevel level, const std::string& message) {
    static bool is_newline = true;
    std::ostringstream oss;
    if (is_newline) {
        oss << getLogPrefix(context, level);
        is_newline = false;
    }
    const bool last_is_newline = !message.empty() ? message.back() == '\n' : false;
    if (message.find('\n') != std::string::npos && !last_is_newline) {
        oss << std::regex_replace(message, std::regex(R"(\n(?!$))"), "\n" + getLogPrefix(context, level));
    } else {
        oss << message;
    }

    is_newline = last_is_newline;

    *Logger::outputStream_ << colorForLevel(level) << oss.str() << RST;
}
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cert-dcl50-cpp,cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay,cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc,hicpp-no-malloc)
std::string Logger::printfStrToStdStr(const char* fmt, ...) {
    char* str = nullptr;
    va_list args;
    va_start(args, fmt);
    if (vasprintf(&str, fmt, args) == -1 || str == nullptr) {
        uvgvpcc_dec::Logger::log<LogLevel::ERROR>("LOGGER", "vasprintf error in printfStrToStdStr function.\n");
        if (errorsAreFatal_) throw std::runtime_error("");
    }
    va_end(args);
    std::string result(str);
    free(str);  // Free the allocated memory
    return result;
}

std::string Logger::vprintfStrToStdStr(const char* fmt, va_list args) {
    char* str = nullptr;
    if (vasprintf(&str, fmt, args) == -1 || str == nullptr) {
        uvgvpcc_dec::Logger::log<LogLevel::ERROR>("LOGGER", "vasprintf error in vprintfStrToStdStr function.\n");
        if (errorsAreFatal_) throw std::runtime_error("");
    }
    std::string result(str);
    free(str);  // Free the allocated memory
    return result;
}
// NOLINTEND(cppcoreguidelines-pro-type-vararg,hicpp-vararg,cert-dcl50-cpp,cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay,cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc,hicpp-no-malloc)

}  // namespace uvgvpcc_dec