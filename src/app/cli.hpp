#pragma once

/**
 * \file
 * Command line interface
 */
#include <limits>
#include <string>

namespace cli {

struct opts_t {
    /** \brief Input filename */
    std::string inputPath{};
    /** \brief Output filename */
    std::string outputPath{};
    /** \brief Number of frames to encode */
    uint32_t frames{};
    /** \brief Input geometry precision */
    uint8_t inputGeoPrecision{};
    /** \brief Frame number to start the encoding */
    uint32_t startFrame = std::numeric_limits<uint32_t>::max();
    /** \brief Maximum number of threads to be used */
    uint32_t threads{};
    /** \brief Encoder configuration */
    std::string uvgvpccParametersString{};
    /** \brief Print help */
    bool help = false;
    /** \brief Print version */
    bool version = false;
    /** \brief Whether to loop input */
    uint32_t loop_input = 1;
};

opts_t opts_parse(const int argc, const char* const argv[]);

void print_usage(void);
void print_version(void);
void print_help(void);

}  // namespace cli