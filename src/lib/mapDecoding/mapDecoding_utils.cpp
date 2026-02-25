#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// #include "utils/fileExport.hpp"
#include "utils/parameters.hpp"
#include "utils/utils.hpp"
#include "uvgvpcc/log.hpp"
#include "uvgvpcc/uvgvpcc.hpp"


void RGB444toYUV420(std::vector<uint8_t>& img, const size_t& width, const size_t& height) {
    Logger::log<LogLevel::TRACE>("MapGeneration", "RGB444toYUV420\n");

    const size_t imageSize = width * height;
    const size_t imageSizeUV = imageSize >> 2U;

    const uint8_t* rChannel = img.data();
    const uint8_t* gChannel = rChannel + imageSize;
    const uint8_t* bChannel = gChannel + imageSize;

    std::vector<uint8_t> yuv420(imageSize + imageSizeUV * 2);
    uint8_t* yChannel = yuv420.data();
    uint8_t* uChannel = yChannel + imageSize;
    uint8_t* vChannel = uChannel + imageSizeUV;

    constexpr float kYR = 0.2126F;
    constexpr float kYG = 0.7152F;
    constexpr float kYB = 0.0722F;

    constexpr float kUR = -0.114572F;
    constexpr float kUG = -0.385428F;
    constexpr float kUB = 0.5F;

    constexpr float kVR = 0.5F;
    constexpr float kVG = -0.454153F;
    constexpr float kVB = -0.045847F;

    size_t idxUV = 0;

    for (size_t y = 0; y < height; y += 2) {
        const size_t row1 = y * width;
        const size_t row2 = row1 + width;

        for (size_t x = 0; x < width; x += 2) {
            const size_t i00 = row1 + x;
            const size_t i01 = i00 + 1;
            const size_t i10 = row2 + x;
            const size_t i11 = i10 + 1;

            const float r00 = static_cast<float>(rChannel[i00]);
            const float g00 = static_cast<float>(gChannel[i00]);
            const float b00 = static_cast<float>(bChannel[i00]);

            const float r01 = static_cast<float>(rChannel[i01]);
            const float g01 = static_cast<float>(gChannel[i01]);
            const float b01 = static_cast<float>(bChannel[i01]);

            const float r10 = static_cast<float>(rChannel[i10]);
            const float g10 = static_cast<float>(gChannel[i10]);
            const float b10 = static_cast<float>(bChannel[i10]);

            const float r11 = static_cast<float>(rChannel[i11]);
            const float g11 = static_cast<float>(gChannel[i11]);
            const float b11 = static_cast<float>(bChannel[i11]);

            yChannel[i00] = static_cast<uint8_t>(kYR * r00 + kYG * g00 + kYB * b00);
            yChannel[i01] = static_cast<uint8_t>(kYR * r01 + kYG * g01 + kYB * b01);
            yChannel[i10] = static_cast<uint8_t>(kYR * r10 + kYG * g10 + kYB * b10);
            yChannel[i11] = static_cast<uint8_t>(kYR * r11 + kYG * g11 + kYB * b11);

            const float avgR = 0.25F * (r00 + r01 + r10 + r11);
            const float avgG = 0.25F * (g00 + g01 + g10 + g11);
            const float avgB = 0.25F * (b00 + b01 + b10 + b11);

            uChannel[idxUV] = static_cast<uint8_t>(kUR * avgR + kUG * avgG + kUB * avgB + 128.F);
            vChannel[idxUV] = static_cast<uint8_t>(kVR * avgR + kVG * avgG + kVB * avgB + 128.F);
            ++idxUV;
        }
    }

    img.swap(yuv420);
}

void YUV420toYUV444(std::vector<uint8_t>& img, const size_t& width, const size_t& height) {
    Logger::log<LogLevel::TRACE>("MapGeneration", "RGB444toYUV420\n");

    const size_t imageSize = width * height;
    const size_t imageSizeUV = imageSize >> 2U;

    const uint8_t* y4Channel = img.data();
    const uint8_t* u2Channel = y4Channel + imageSize;
    const uint8_t* v0Channel = u2Channel + imageSizeUV;

    std::vector<uint8_t> uv44(imageSize * 2);
    uint8_t* u4Channel = uv44.data();
    uint8_t* v4Channel = u4Channel + imageSize;
   
    size_t idxUV = 0;

    for (size_t y = 0; y < height; y += 2) {
        const size_t row1 = y * width;
        const size_t row2 = row1 + width;

        for (size_t x = 0; x < width; x += 2) {
            
            ++idxUV;
        }
    }
    
}