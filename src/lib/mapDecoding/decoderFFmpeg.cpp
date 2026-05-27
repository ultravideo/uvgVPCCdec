#include "decoderFFmpeg.hpp"

#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "abstract2DMapDecoder.hpp"
#include "bitstreamParsing/bitstream_util.hpp"
//#include "catchLibLog.hpp"
#include "utils/fileExport.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

using namespace uvgvpcc_dec;

void DecoderFFmpeg::initializeLogCallback() {}

namespace {

void setMapList(const std::shared_ptr<uvgvpcc_dec::GOF>& gof, std::vector<std::reference_wrapper<std::vector<uint8_t>>>& mapList,
                const DECODER_TYPE& decoderType) {
    mapList.reserve(gof->nbFrames);
    if (decoderType == OCCUPANCY) {
        for (const std::shared_ptr<uvgvpcc_dec::Frame>& frame : gof->frames) {
            mapList.emplace_back(frame->occupancyMapDS);
        }
        // bitstream = &gof->bitstreamOccupancy;

    } else if (decoderType == GEOMETRY) {
        for (const std::shared_ptr<uvgvpcc_dec::Frame>& frame : gof->frames) {
            mapList.emplace_back(frame->geometryMapL1);
            if (gof->doubleLayer) {
                mapList.emplace_back(frame->geometryMapL2);
            }
        }
        // bitstream = &gof->bitstreamGeometry;

    } else if (decoderType == ATTRIBUTE) {
        for (const std::shared_ptr<uvgvpcc_dec::Frame>& frame : gof->frames) {
            mapList.emplace_back(frame->attributeMapL1);
            if (gof->doubleLayer) {
                mapList.emplace_back(frame->attributeMapL2);
            }
        }
        // bitstream = &gof->bitstreamAttribute;
    } else {
        assert(false);
    }
}

void setMapList_16bit(const std::shared_ptr<uvgvpcc_dec::GOF>& gof, std::vector<std::reference_wrapper<std::vector<uint16_t>>>& mapList) {
    mapList.reserve(gof->nbFrames);
    for (const std::shared_ptr<uvgvpcc_dec::Frame>& frame : gof->frames) {
        mapList.emplace_back(frame->attributeMapL1_16bits);
        if (gof->doubleLayer) {
            mapList.emplace_back(frame->attributeMapL2_16bits);
        }
    }
}

void setBitstream(const std::shared_ptr<uvgvpcc_dec::GOF>& gof, std::vector<uint8_t>& bitstream, const DECODER_TYPE& decoderType) {
    switch (decoderType) {
        case OCCUPANCY:
            bitstream = gof->bitstreamOccupancy;
            break;
        case GEOMETRY:
            bitstream = gof->bitstreamGeometry;
            break;
        case ATTRIBUTE:
            bitstream = gof->bitstreamAttribute;
            break;
        default:
            assert(false);
    }
}

std::vector<uint8_t>& getBitstream(const std::shared_ptr<uvgvpcc_dec::GOF>& gof, const DECODER_TYPE& decoderType) {
    switch (decoderType) {
        case OCCUPANCY:
            return gof->bitstreamOccupancy;
            break;
        case GEOMETRY:
            return gof->bitstreamGeometry;
            break;
        case ATTRIBUTE:
            return gof->bitstreamAttribute;
            break;
        default:
            assert(false);
            static std::vector<uint8_t> dummy;
            return dummy;
    }
}

void setFFmpegConfig(const AVCodec* codec, AVCodecContext* codec_ctx, const DECODER_TYPE& decoderType) {

}


void decodeVideoFFmpeg( std::vector<AVFrame*> &frames, AVCodecContext* codec_ctx, std::vector<uint8_t>& bitstream, const std::string& decoderName ) {

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        throw std::runtime_error(decoderName + ": Failed to allocate AVPacket.");
    }

    const uint8_t hevc_start_code[4] = {0x00, 0x00, 0x00, 0x01};
    size_t read_ptr = 0;
    size_t write_ptr = 0;
    size_t map_size = bitstream.size();

    std::vector<uint8_t> data_buffer = {};

    // printf("%s\n", decoderName.c_str());
    while (write_ptr < map_size) {
        size_t nalu_size = bitstream_read_size_from_poiter(&bitstream[write_ptr], 4);

        data_buffer.insert(data_buffer.end(), hevc_start_code, hevc_start_code + 4);
        write_ptr += 4;

        size_t hevc_nal_type = (bitstream[write_ptr] >> 1) & 0x3F;
        // printf("nalu_size: %d, hevc_nal_type: %d\n", (int)nalu_size, (int)hevc_nal_type);

        data_buffer.insert(data_buffer.end(), bitstream.begin()+write_ptr, bitstream.begin()+write_ptr+nalu_size);
        write_ptr += nalu_size;

        if (hevc_nal_type == 19 || hevc_nal_type == 1) {
            pkt->data = data_buffer.data();
            pkt->size = data_buffer.size();
            // av_new_packet(pkt, data_buffer.size());
            // memcpy(pkt->data, data_buffer.data(), data_buffer.size());
            // printf("Packet_size: %d, data size: %d\n", (int)pkt->size, (int)data_buffer.size());

            // Send AVPacket
            int ret = avcodec_send_packet(codec_ctx, pkt);
            if (ret < 0) {
                av_packet_free(&pkt);
                throw std::runtime_error(decoderName + ": Failed to send AVPacket.");
            }

            // Create AVFrame
            AVFrame* frame = av_frame_alloc();
            if (!frame) {
                throw std::runtime_error(decoderName + ": Failed to allocate AVFrame.");
            }

            while (true) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0)
                    throw std::runtime_error(decoderName + ": Failed to receive AVFrame.");
                frames.push_back(frame);
                frame = av_frame_alloc();
            }
            av_frame_free(&frame);
            data_buffer.clear();
        }
    }
    
    // Flush decoder
    avcodec_send_packet(codec_ctx, nullptr);
    AVFrame* frame = av_frame_alloc();
    while (avcodec_receive_frame(codec_ctx, frame) == 0)
    {
        frames.push_back(frame);
        frame = av_frame_alloc();
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);

    bitstream.clear();
    bitstream.shrink_to_fit();
}


void writeDecodedFramesToMapList_(
    const std::shared_ptr<uvgvpcc_dec::GOF>& gof, 
    std::vector<std::reference_wrapper<std::vector<uint8_t>>> &mapList, 
    std::vector<AVFrame*> &frames,
    const DECODER_TYPE& decoderType
) {
    const int height = frames.front()->height;
    const int width  = frames.front()->width;

    // Store the map size
    if (decoderType == OCCUPANCY) {
        gof->occupancy_map_width  = width;
        gof->occupancy_map_height = height;
    } else if (decoderType == GEOMETRY) {
        gof->geometry_map_width  = width;
        gof->geometry_map_height = height;
    } else {
        gof->attribute_map_width  = width;
        gof->attribute_map_height = height;
    }

    const size_t sizeY  = size_t(width) * size_t(height);
    const size_t sizeUV = sizeY >> 2U;
    const size_t total_size = sizeY << 1;
    const size_t UV_width = width >> 1; // width / 2
    const size_t UV_height = height >> 1;

    for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
        AVFrame* frame = frames[frame_index];
        std::vector<uint8_t>& map = mapList[frame_index].get();

        map.resize(total_size); // Y + U + V
        uint8_t* dstY = map.data();
        uint8_t* dstU = dstY + sizeY;
        uint8_t* dstV = dstU + sizeUV;

        // map.resize(sizeMap);
        // // Y
        // memcpy(map.data(), frame->data[0], sizeY); 
        // // U
        // memcpy(map.data() + sizeY, frame->data[1], sizeUV); 
        // // V
        // memcpy(map.data() + sizeY + sizeUV, frame->data[2], sizeUV);

        // Copy Y plane
        for (int y = 0; y < height; ++y) {
            memcpy(dstY + y*width, frame->data[0] + y*frame->linesize[0], width);
        }

        // Copy U plane
        for (int y = 0; y < UV_height; ++y) {
            memcpy(dstU + y*UV_width, frame->data[1] + y*frame->linesize[1], UV_width);
        }

        // Copy V plane
        for (int y = 0; y < UV_height; ++y) {
            memcpy(dstV + y*UV_width, frame->data[2] + y*frame->linesize[2], UV_width);
        }

        av_frame_free(&frame);

    }

}

inline double PCCClip( const double& n, const double& lower, const double& upper ) {
    return ( std::max )( lower, ( std::min )( n, upper ) );
}

inline float PCCClip( const float& n, const float& lower, const float& upper ) {
    return ( std::max )( lower, ( std::min )( n, upper ) );
}

void YUVtoFloatYUV(const std::vector<uint8_t>& src, std::vector<float>& dst, uint16_t offset, float minV, float maxV) {
    size_t count = src.size();
    dst.resize(count);
    
    double scale = 255.0;
    double weight = 1.0 / scale;


    for (size_t i = 0; i < count; i++) {
        dst[i] = std::clamp( (float)(weight * (double)(src[i] - offset)), minV, maxV);
    }
}

void floatYUVtoYUV(const std::vector<float>& src, std::vector<uint16_t>& dst, double offset, double scale) {
    size_t count = src.size();
    dst.resize(count);

    for (size_t i = 0; i < count; i++) {
        dst[i] = static_cast<uint16_t>( PCCClip(std::round((float)(scale * (double)src[i] + offset)), 0.f, (float)scale) );
    }
}

void upsampling(const std::vector<float>& chromaIn, std::vector<float>& chromaOut, const int widthIn, const int heightIn) {
    const int widthOut = widthIn * 2;
    const int heightOut = heightIn * 2;
    chromaOut.resize(widthOut * heightOut);
    std::vector<float> temp;
    temp.resize(widthIn * heightOut);

    constexpr float scale = 1.0f / 256.0f;

    // Vertical upsampling
    for (int i = 0; i < heightIn; i++) {
        // i0 - 0
        const int i0_clamp = std::clamp(i, 0, heightIn - 1) * widthIn;
        // i0 - 1
        const int i0_m1_clamp = std::clamp(i - 1, 0, heightIn - 1) * widthIn;
        // i0 + 1 
        const int i0_p1_clamp = std::clamp(i + 1, 0, heightIn - 1) * widthIn;
        // i0 - 2
        const int i0_m2_clamp = std::clamp(i - 2, 0, heightIn - 1) * widthIn;
        // i0 + 2
        const int i0_p2_clamp = std::clamp(i + 2, 0, heightIn - 1) * widthIn;

        for (int j = 0; j < widthIn; j++) {
            temp[(2 * i) * widthIn + j] = (
                -8.0*chromaIn[i0_m2_clamp+j] + 
                64.0*chromaIn[i0_m1_clamp+j] + 
                216.0*chromaIn[i0_clamp+j] - 
                16.0*chromaIn[i0_p1_clamp+j]
            ) * scale;
            temp[(2 * i + 1) * widthIn + j] = (
                -16.0*chromaIn[i0_m1_clamp+j] + 
                216.0*chromaIn[i0_clamp+j] + 
                64.0*chromaIn[i0_p1_clamp+j] - 
                8.0*chromaIn[i0_p2_clamp+j]
            ) * scale;
        }
    }

    // Horizontal upsampling
    for (int i = 0; i < heightOut; i++) {
        const int step = i * widthIn;
        for (int j = 0; j < widthIn; j++) {
            chromaOut[i * widthOut + j * 2] = temp[step + std::clamp(j, 0, widthIn - 1)]; // ( 256*temp[step + std::clamp(j, 0, widthIn - 1)] ) * scale
            // chromaOut[i * widthOut + j * 2 + 1][1] = temp[0][step + std::clamp(j+1, 0, widthIn - 1)]; // ( 256*temp[step + std::clamp(j+1, 0, widthIn - 1)] ) * scale
            chromaOut[i * widthOut + j * 2 + 1] = (
                -16.0*temp[step + std::clamp(j - 1, 0, widthIn - 1)] + 
                144.0*temp[step + std::clamp(j, 0, widthIn - 1)] + 
                144.0*temp[step + std::clamp(j + 1, 0, widthIn - 1)] - 
                16.0*temp[step + std::clamp(j + 2, 0, widthIn - 1)]  
            ) * scale;
        }
    }
}

void writeDecodedFramesToMapList_color_inversion(
    const std::shared_ptr<uvgvpcc_dec::GOF>& gof, 
    std::vector<std::reference_wrapper<std::vector<uint16_t>>> &mapList, 
    std::vector<AVFrame*> &frames,
    const DECODER_TYPE& decoderType
) {

    if (frames.empty()) {
        throw std::runtime_error("FFmpeg decoder produced no frames.");
    }

    const int height = frames.front()->height;
    const int width  = frames.front()->width;

    // Store the map size
    gof->attribute_map_width  = (size_t)width;
    gof->attribute_map_height = (size_t)height;
    
    const size_t sizeY  = size_t(width) * size_t(height);
    const size_t sizeUV = sizeY >> 2U;
    const size_t total_size = sizeY << 1;
    const size_t UV_width = width >> 1; // width / 2
    const size_t UV_height = height >> 1;

    float minV_uv = -0.5f; // Chroma
    float maxV_uv = 0.5f; // Chroma
    float minV_y =  0.f;
    float maxV_y = 1.f;

    for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
        AVFrame* frame = frames[frame_index];
        std::vector<uint16_t>& map = mapList[frame_index].get();

        std::vector<uint8_t> yuv420[3];
        std::vector<float> yuv420_f[3];
        std::vector<float> yuv444_f[3];
        std::vector<uint16_t> yuv444[3];

        for (int y = 0; y < height; ++y) {
            yuv420[0].insert(yuv420[0].end(), frame->data[0] + y*frame->linesize[0], frame->data[0] + y*frame->linesize[0] + width);
        }
        for (int y = 0; y < UV_height; ++y) {
            yuv420[1].insert(yuv420[1].end(), frame->data[1] + y*frame->linesize[1], frame->data[1] + y*frame->linesize[1] + UV_width);
            yuv420[2].insert(yuv420[2].end(), frame->data[2] + y*frame->linesize[2], frame->data[2] + y*frame->linesize[2] + UV_width);
        }

        av_frame_free(&frame);

        // Apply tmc2 color inversion
        YUVtoFloatYUV(yuv420[0], yuv420_f[0], 0, minV_y, maxV_y);
        YUVtoFloatYUV(yuv420[1], yuv420_f[1], 128, minV_uv, maxV_uv);
        YUVtoFloatYUV(yuv420[2], yuv420_f[2], 128, minV_uv, maxV_uv);
        upsampling(yuv420_f[1], yuv444_f[1], (int)UV_width, (int)UV_height);
        upsampling(yuv420_f[2], yuv444_f[2], (int)UV_width, (int)UV_height);
        floatYUVtoYUV(yuv420_f[0], yuv444[0], 0.0, 65535.);
        floatYUVtoYUV(yuv444_f[1], yuv444[1], 32768., 65535.);
        floatYUVtoYUV(yuv444_f[2], yuv444[2], 32768., 65535.);

        // Copy data to mapList
        map.insert(map.end(), yuv444[0].data(), yuv444[0].data() + yuv444[0].size());
        map.insert(map.end(), yuv444[1].data(), yuv444[1].data() + yuv444[1].size());
        map.insert(map.end(), yuv444[2].data(), yuv444[2].data() + yuv444[2].size());
    }

}


void writeDecodedFramesToMapList(
    const std::shared_ptr<uvgvpcc_dec::GOF>& gof, 
    std::vector<std::reference_wrapper<std::vector<uint8_t>>> &mapList, 
    std::vector<AVFrame*> &frames,
    const DECODER_TYPE& decoderType
) {

    if (frames.empty()) {
        throw std::runtime_error("FFmpeg decoder produced no frames.");
    }

    const int height = frames.front()->height;
    const int width  = frames.front()->width;

    // Store the map size
    if (decoderType == OCCUPANCY) {
        gof->occupancy_map_width  = width;
        gof->occupancy_map_height = height;
    } else if (decoderType == GEOMETRY) {
        gof->geometry_map_width  = width;
        gof->geometry_map_height = height;
    } else {
        gof->attribute_map_width  = width;
        gof->attribute_map_height = height;
    }

    const size_t sizeY  = size_t(width) * size_t(height);
    const size_t sizeUV = sizeY >> 2U;
    const size_t total_size = sizeY << 1;
    const size_t UV_width = width >> 1; // width / 2
    const size_t UV_height = height >> 1;

    int Y_widths[height];
    int Y_linesizes[height];
    for (int y = 0; y < height; y++) {
        Y_widths[y] = y*width;
        Y_linesizes[y] = y*frames.front()->linesize[0];
    }

    int UV_widths[UV_height];
    int UV_linesizes[UV_height];
    for (int y = 0; y < UV_height; y++) {
        UV_widths[y] = y*UV_width;
        UV_linesizes[y] = y*frames.front()->linesize[1];
    }

    for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
        AVFrame* frame = frames[frame_index];
        std::vector<uint8_t>& map = mapList[frame_index].get();

        map.resize(total_size); // Y + U + V
        uint8_t* dstY = map.data();
        uint8_t* dstU = dstY + sizeY;
        uint8_t* dstV = dstU + sizeUV;

        // Copy Y plane
        for (int y = 0; y < height; ++y) {
            memcpy(dstY + Y_widths[y], frame->data[0] + Y_linesizes[y], width);
        }

        // Copy U and V planes
        for (int y = 0; y < UV_height; ++y) {
            memcpy(dstU + UV_widths[y], frame->data[1] + UV_linesizes[y], UV_width);
            memcpy(dstV + UV_widths[y], frame->data[2] + UV_linesizes[y], UV_width);
        }

        av_frame_free(&frame);

    }

}


} // anonymous namespace

void DecoderFFmpeg::decodeGOFMaps(const std::shared_ptr<uvgvpcc_dec::GOF>& gof) {
    std::string decoderName;  // For log and debug
    switch (decoderType_) {
        case OCCUPANCY:
            decoderName = "FFmpeg occupancy map decoder";
            break;
        case GEOMETRY:
            decoderName = "FFmpeg geometry map decoder";
            break;
        case ATTRIBUTE:
            decoderName = "FFmpeg attribute map decoder";
            break;
        default:
            assert(false);
    }

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H265);
    if (!codec) {
        throw std::runtime_error(decoderName + ": Failed to find codec.");
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);

    if ((avcodec_open2(codec_ctx, codec, nullptr)) < 0) {
        throw std::runtime_error(decoderName + ": Failed to open codec.");
    }

    std::vector<std::reference_wrapper<std::vector<uint8_t>>> mapList;
    setMapList(gof, mapList, decoderType_);

    std::vector<uint8_t>& bitstream = getBitstream(gof, decoderType_);

    std::vector<AVFrame*> frames = {};
    decodeVideoFFmpeg(frames, codec_ctx, bitstream, decoderName);
    avcodec_free_context(&codec_ctx);

    if (decoderType_ == ATTRIBUTE && p_->useTMC2AttributeYUVConversion) {
        std::vector<std::reference_wrapper<std::vector<uint16_t>>> mapList_16bit;
        setMapList_16bit(gof, mapList_16bit);
        writeDecodedFramesToMapList_color_inversion(gof, mapList_16bit, frames, decoderType_);
    } else {
        writeDecodedFramesToMapList(gof, mapList, frames, decoderType_);
    }

    if (p_->exportIntermediateFiles) {
        for(const auto& frame : gof->frames) {
            switch (decoderType_) {
                case OCCUPANCY: {}
                    FileExport::exportImageOccupancy(frame, gof->occupancy_map_width);
                    break;
                case GEOMETRY:
                    FileExport::exportImageGeometryBgFill(frame, gof->geometry_map_width, gof->geometry_map_height, gof->doubleLayer);
                    break;
                case ATTRIBUTE:
                    FileExport::exportImageAttributeYUV(frame, gof->attribute_map_width, gof->attribute_map_height, gof->doubleLayer);
                    break;
                default:
                    assert(false);
            }
        }
    }
}