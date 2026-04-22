#pragma once

/**
 * \file
 * Command line interface
 */
#include <limits>
#include <string>
#include <span>
#include <cstdint>
#include <vector>

namespace cli {

struct opts_t {
    /** \brief Input filename */
    std::string inputPath{};
    /** \brief Output filename */
    std::string outputPath{};
    /** \brief TMC2Upscaling */
    bool tmc2Upscaling{false};
    /** \brief Fast color conversion */
    bool fastColorConversion{false};
    /** \brief Keep intermediate files */
    bool keepIntermediateFiles{false};
    /** \brief Maximum number of threads to be used */
    uint32_t threads{20};
    /** \brief Encoder configuration */
    std::string uvgvpccParametersString{};
    /** \brief Print help */
    bool help = false;
    /** \brief Print version */
    bool version = false;
    /** \brief If dummyRun is true, config is verified but no encoding is done */
    bool dummyRun = false;    
    /** \brief Remote input data */
    bool remoteInputData = false;
    /** \brief Source address for rtp streams */
    std::string srcAddress{};
    /** \brief Source port for rtp streams */
    std::vector<uint16_t> srcPort{};
    /** \brief Output directory for SDP files */
    std::string sdpOutdir{};
    /** \brief FPS limit for reading input frames */
    size_t inputFramePerSecondLimiter = 0; // 0 means no input frame limiter (maxConcurrentFrames can still be a limiter)

    /** \brief Remote output data */
    bool remoteOutputData = false;
    /** \brief Position address for visualizer */
    std::string positionAddress{};
    /** \brief Color port for rtp streams */
    std::string colorAddress{};

    /** \brief in order output data */
    bool in_order_output = false;
};

bool opts_parse(cli::opts_t& opts, const int argc, const char* const argv[]);

void print_usage(void);
void print_version(void);
void print_help(void);

}  // namespace cli