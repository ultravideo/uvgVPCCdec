#include "uvgvpccdec/log.hpp"

#include <cstdarg>
#include <ctime>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>

#include "chrono.hpp"

namespace uvgvpcc_dec {

#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define BLD "\x1b[1m"
#define RST "\x1B[0m"

LogLevel Logger::logLevel = LogLevel::INFO;

static std::string log_color[7] = {BLD RED, RED, YEL, BLU, CYN, GRN, MAG};

#define GET_LOG_PREFIX(level) \
    std::string("[" + global_timer.elapsed_str() + "][" + LogLevelStr[static_cast<int>(level)] + "] [" + context + "] ")

void Logger::setLogLevel(LogLevel level) { logLevel = level; }
LogLevel Logger::getLogLevel() { return logLevel; }

void Logger::log(LogLevel level, const std::string context, const std::string& message) {
    static std::mutex logMutex;
    std::lock_guard<std::mutex> lock(logMutex);

    static bool is_newline = true;
    if (level <= Logger::getLogLevel()) {
        std::ostringstream oss;
        if (is_newline) {
            oss << GET_LOG_PREFIX(level);
            is_newline = false;
        }
        bool last_is_newline = !message.empty() ? message.back() == '\n' : false;
        if (message.find('\n') != std::string::npos && !last_is_newline) {
            oss << std::regex_replace(message, std::regex(R"(\n(?!$))"), "\n" + GET_LOG_PREFIX(level));
        } else {
            oss << message;
        }

        if (last_is_newline) {
            is_newline = true;
        }

        std::cerr << log_color[static_cast<int>(level)] << oss.str() << RST;
    }
}

std::string Logger::printfStrToStdStr(const char* fmt, ...) {
    char* str;
    va_list args;
    va_start(args, fmt);
    if(vasprintf(&str, fmt, args) == -1) {
        std::cout << "Erreur vasprintf" << std::endl;
    }
    va_end(args);
    return (std::string)str;
}

std::string Logger::vprintfStrToStdStr(const char* fmt, va_list args) {
    char* str;
    if(vasprintf(&str, fmt, args) == -1) {
        std::cout << "Erreur vasprintf" << std::endl;
    }
    return (std::string)str;
}

}  // namespace uvgvpcc_dec