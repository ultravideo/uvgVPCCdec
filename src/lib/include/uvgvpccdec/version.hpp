#pragma once

#include <cstdint>
#include <string>

namespace uvgvpcc_dec {
std::string get_version();
uint16_t get_version_major();
uint16_t get_version_minor();
uint16_t get_version_patch();
// std::string get_git_hash();
}  // namespace uvgvpcc_dec