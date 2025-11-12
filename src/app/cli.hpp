#pragma once

/**
 * \file
 * Command line interface
 */
#include <limits>
#include <string>
#include "uvgvpccdec/uvgvpccdec.hpp"

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
};

bool opts_parse(cli::opts_t& opts, const int argc, const char* const argv[]);

void print_usage(void);
void print_version(void);
void print_help(void);

}  // namespace cli