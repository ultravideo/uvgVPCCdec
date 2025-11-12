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

static const char short_options[] = "i:o:t:"; // Only input, output, threads
static const struct option long_options[] = {
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"threads", required_argument, NULL, 't'},
    {"TMC2Upscaling", required_argument, NULL, 0},
    {"fastColorConversion", required_argument, NULL, 0},
    {"keepIntermediateFiles", required_argument, NULL, 0},
    {"uvgvpccParametersString", required_argument, NULL, 0},
    {"help", no_argument, NULL, 0},
    {"version", no_argument, NULL, 0},
    {0, 0, 0, 0}};


/**
 * \brief Parse command line arguments.
 * \param opts  Options structure to fill
 * \param argc  Number of arguments
 * \param argv  Argument list
 * \return      True if execution execution should end (for exemple if the flag --help is used).
 */
bool opts_parse(cli::opts_t& opts, const int argc, const char* const argv[]) {
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
        } else if (name == "threads") {
            opts.threads = std::stoi(optarg);
        } else if (name == "TMC2Upscaling") {
            opts.tmc2Upscaling = std::string(optarg) == "true";
        } else if (name == "fastColorConversion") {
            opts.fastColorConversion = std::string(optarg) == "true";
        } else if (name == "keepIntermediateFiles") {
            opts.keepIntermediateFiles = std::string(optarg) == "true";
        } else if (name == "uvgvpccParametersString") {
            opts.uvgvpccParametersString = optarg;
        } else if (name == "version") {
            print_version();
            opts.version = true;
        } else if (name == "help") {
            print_help();
            opts.help = true;
        }
    }

    // Check for extra arguments.
    if (argc - optind > 0) {
        throw std::runtime_error("Input error: Extra argument found: " + std::string(argv[optind]) + ".");
    }

    if (opts.help || opts.version) {
        return true;
    }

    // Check that the required files were defined
    if (opts.inputPath.empty() || opts.outputPath.empty()) {
        throw std::runtime_error("Input error: Input or output path is empty\n");
    }

    return false;
}

void print_usage(void) {
    std::cout << "usage: to do\n"
              << "       --help for more information" << std::endl;
}

void print_version(void) { std::cout << "uvgvpccdec " << uvgvpcc_dec::get_version() << std::endl; }

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
