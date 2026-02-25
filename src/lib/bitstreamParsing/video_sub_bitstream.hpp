#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct nal_info {
    size_t location = 0;
    size_t size = 0;
};

// not used currently, this process is now included in byteStreamToSampleStream()
void find_nals(std::vector<uint8_t> &input_data, std::vector<nal_info> &nals);