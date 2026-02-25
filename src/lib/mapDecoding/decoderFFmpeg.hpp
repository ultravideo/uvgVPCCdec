#pragma once

/// \file Interface between uvgVPCCenc and the 2D encoder Kvazaar that implement the 'abstract2DMapEncoder'.

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}


#include "bitstreamParsing/gof.hpp"
#include "uvgvpcc/uvgvpcc.hpp"
#include "abstract2DMapDecoder.hpp"

using namespace uvgvpcc_dec;


class DecoderFFmpeg : public Abstract2DMapDecoder {
public:
    DecoderFFmpeg(const DECODER_TYPE& decoderType) : Abstract2DMapDecoder(decoderType) {};
    static void initializeLogCallback();
    void decodeGOFMaps(const std::shared_ptr<uvgvpcc_dec::GOF>& gof) override;
};