/*****************************************************************************
 * This file is part of uvg266 VVC encoder.
 *
 * Copyright (c) 2021, Tampere University, ITU/ISO/IEC, project contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * * Neither the name of the Tampere University or ITU/ISO/IEC nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
 ****************************************************************************/

/*
 * \file
 *
 */
#include "cli.hpp"

#include <getopt.h>

#include <regex>
#include <string>

#include "uvgvpccdec/log.hpp"
#include "uvgvpccdec/version.hpp"

namespace cli {

static const char short_options[] = "i:g:l:n:o:s:t:b:";
static const struct option long_options[] = {{"input", required_argument, NULL, 'i'},   {"output", required_argument, NULL, 'o'},
                                             {"frames", required_argument, NULL, 'n'},  {"start-frame", required_argument, NULL, 's'},
                                             {"geo-precision", required_argument, NULL, 'g'},
                                             {"threads", required_argument, NULL, 't'}, {"uvgvpccdec", required_argument, NULL, 0},
                                             {"loop-input", required_argument, NULL, 'l'},      {"help", no_argument, NULL, 0},
                                             {"version", no_argument, NULL, 0},         {0, 0, 0, 0}};

/**
 * \brief Try to detect voxel size from file name automatically
 *
 * \param file_name    file name to get voxel size from
 * \return voxel_size on success, 0 on fail
 */
static uint8_t select_voxel_size_auto(std::string file_name) {
    std::regex pattern("vox([0-9]+)");
    std::smatch match;
    int number = 0;
    if (std::regex_search(file_name, match, pattern)) {
        // The first match captures the entire pattern, so we need to access the second capture group (index 1)
        std::string number_str = match[1].str();
        number = std::stoi(number_str);
    }
    return number;
}

/**
 * \brief Try to detect frame count from file name automatically
 *
 * \param file_name    file name to get frame count from
 * \return frame count on success, 0 on fail
 */
static uint16_t select_frame_count_auto(std::string file_name) {
    std::regex pattern("([0-9]+)_%");
    std::smatch match;
    int number = 0;
    if (std::regex_search(file_name, match, pattern)) {
        // The first match captures the entire pattern, so we need to access the second capture group (index 1)
        std::string number_str = match[1].str();
        number = std::stoi(number_str);
    }
    return number;
}

/**
 * \brief Try to detect start frame from file name automatically
 *
 * \param file_name    file name to get frame count from
 * \return start frame on success, 0 on fail
 */
static uint16_t select_start_frame_auto(std::string file_name) {
    std::regex pattern("([0-9]+)_[0-9]+_%");
    std::smatch match;
    int number = 0;
    if (std::regex_search(file_name, match, pattern)) {
        // The first match captures the entire pattern, so we need to access the second capture group (index 1)
        std::string number_str = match[1].str();
        number = std::stoi(number_str);
    }
    return number;
}

/**
 * \brief Parse command line arguments.
 * \param argc  Number of arguments
 * \param argv  Argument list
 * \return      Parsed options
 */
opts_t opts_parse(const int argc, const char* const argv[]) {
    opts_t opts;

    // Parse command line options
    for (optind = 0;;) {
        int long_options_index = -1;

        int c = getopt_long(argc, const_cast<char* const*>(argv), short_options, long_options, &long_options_index);
        if (c == -1) break;

        if (long_options_index < 0) {
            int i;
            for (i = 0; long_options[i].name; i++) {
                if (long_options[i].val == c) {
                    long_options_index = i;
                    break;
                }
            }
        }

        const std::string name = long_options[long_options_index].name;
        if (name == "input") {
            if (!opts.inputPath.empty()) {
                throw std::runtime_error("Input error: More than one input file given.");
            }
            opts.inputPath = optarg;
        } else if (name == "output") {
            if (!opts.outputPath.empty()) {
                throw std::runtime_error("Input error: More than one output file given.");
            }
            opts.outputPath = optarg;
        } else if (name == "geo-precision") {
            opts.inputGeoPrecision = std::stoi(optarg);
            if (opts.inputGeoPrecision == 0) {
                throw std::runtime_error("Input error: Geometry precision is set to zero");
            }
        } else if (name == "frames") {
            opts.frames = std::stoi(optarg);
            if (opts.frames == 0) {
                throw std::runtime_error("Input error: Frame count is zero");
            }
        } else if (name == "start-frame") {
            opts.startFrame = std::stoi(optarg);
        } else if (name == "loop-input") {
            opts.loop_input = std::stoi(optarg);
        } else if (name == "version") {
            opts.version = true;
        } else if (name == "help") {
            opts.help = true;
        }
    }
    // Check for extra arguments.
    if (argc - optind > 0) {
        throw std::runtime_error("Input error: Extra argument found: " + std::string(argv[optind]) + ".");
    }

    if (opts.help || opts.version) {
        return opts;
    }

    // Check that the required files were defined
    if (opts.inputPath.empty() || opts.outputPath.empty()) {
        throw std::runtime_error("Input error: Input or output path is empty\n");
    }

    if (opts.inputGeoPrecision == 0) {
        opts.inputGeoPrecision = select_voxel_size_auto(opts.inputPath);
        uvgvpccdec::Logger::log(uvgvpccdec::LogLevel::INFO, "APPLICATION", "Detected geometry precision from file name: " + std::to_string(opts.inputGeoPrecision) + ".\n");
        if (opts.inputGeoPrecision == 0) {
            throw std::runtime_error("Input error: Geometry precision is set to zero");
        }
    }

    if (opts.frames == 0) {
        opts.frames = select_frame_count_auto(opts.inputPath);
        uvgvpccdec::Logger::log(uvgvpccdec::LogLevel::INFO, "APPLICATION",
                             "Detected frame count from file name: " + std::to_string(opts.frames) + ".\n");
        if (opts.frames == 0) {
            throw std::runtime_error("Input error: Frame count is zero");
        }
    }

    if (opts.startFrame == std::numeric_limits<uint32_t>::max()) {
        opts.startFrame = select_start_frame_auto(opts.inputPath);
        uvgvpccdec::Logger::log(uvgvpccdec::LogLevel::INFO, "APPLICATION",
                             "Detected start frame from file name: " + std::to_string(opts.startFrame) + ".\n");
        if (opts.startFrame == std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Input error: Frame count is zero");
        }
    }

    return opts;
}

void print_usage(void) {
    std::cout << "usage: to do\n"
              << "       --help for more information" << std::endl;
}

void print_version(void) { std::cout << "uvgvpccdec " << uvgvpccdec::get_version() << std::endl; }

void print_help(void) {
    fprintf(stdout,
            "Usage:\n"
            "uvg266 -i <input> --input-res <width>x<height> -o <output>\n"
            "\n"
            /* Word wrap to this width to stay under 80 characters (including ") *************/
            "Required:\n"
            "  -i, --input <filename>     : Input file\n"
            "      --input-res <res>      : Input resolution [auto]\n"
            "                                   - auto: Detect from file name.\n"
            "                                   - <int>x<int>: width times height\n"
            "  -o, --output <filename>    : Output file\n"
            "\n"
            /* Word wrap to this width to stay under 80 characters (including ") *************/
            "Presets:\n"
            "      --preset <preset>      : Set options to a preset [medium]\n"
            "                                   - ultrafast, superfast, veryfast, faster,\n"
            "                                     fast, medium, slow, slower, veryslow\n"
            "                                     placebo\n"
            "\n"
            /* Word wrap to this width to stay under 80 characters (including ") *************/
            "Input:\n"
            "  -n, --frames <integer>     : Number of frames to code [all]\n"
            "      --seek <integer>       : First frame to code [0]\n"
            "      --input-fps <num>[/<denom>] : Frame rate of the input video [25]\n"
            "      --source-scan-type <string> : Source scan type [progressive]\n"
            "                                   - progressive: Progressive scan\n"
            "                                   - tff: Top field first\n"
            "                                   - bff: Bottom field first\n"
            "      --input-format <string> : P420 or P400 [P420]\n"
            "      --input-bitdepth <int> : 8-16 [8]\n"
            "      --loop-input           : Re-read input file forever.\n"
            "      --input-file-format <string> : Input file format [auto]\n"
            "                                    - auto: Check the file ending for format\n"
            "                                    - y4m (skips frame headers)\n"
            "                                    - yuv\n"
            "\n"
            /* Word wrap to this width to stay under 80 characters (including ") *************/
    );
}
}  // namespace cli
