#pragma once
#include <string>

// TODO(lf) : verify that all 'uvgvpcc_dec' follow the exact same writting
namespace uvgvpcc_dec {
std::string get_version();
size_t get_version_major();
size_t get_version_minor();
size_t get_version_patch();
}  // namespace uvgvpcc_dec