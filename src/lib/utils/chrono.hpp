#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

class Timer {
   public:
    Timer() : start(std::chrono::steady_clock::now()) {}

    // Function to get elapsed time since the start of the program
    double elapsed() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count() / 1000000.0;
    }
    std::string elapsed_str() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << elapsed();
        return oss.str();
    }

   private:
    std::chrono::time_point<std::chrono::steady_clock> start;
};

static Timer global_timer;
