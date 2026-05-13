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

void decodeVideoFFmpeg_old(std::vector<AVFrame*>& frames,
                       AVCodecContext* codec_ctx,
                       std::vector<uint8_t>& bitstream,
                       const std::string& decoderName) {
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        throw std::runtime_error(decoderName + ": Failed to allocate AVPacket.");
    }

    const uint8_t hevc_start_code[4] = {0x00, 0x00, 0x00, 0x01};

    const size_t map_size = bitstream.size();
    size_t ptr = 0;

    std::vector<uint8_t> access_unit;
    std::vector<uint8_t> parameter_sets;

    auto append_annexb_nal = [&](std::vector<uint8_t>& dst,
                                 const uint8_t* nal_ptr,
                                 size_t nal_size) {
        dst.insert(dst.end(), hevc_start_code, hevc_start_code + 4);
        dst.insert(dst.end(), nal_ptr, nal_ptr + nal_size);
    };

    auto send_packet_and_receive_frames = [&](const std::vector<uint8_t>& packet_data) {
        if (packet_data.empty()) {
            return;
        }

        av_packet_unref(pkt);

        int ret = av_new_packet(pkt, static_cast<int>(packet_data.size()));
        if (ret < 0) {
            throw std::runtime_error(decoderName + ": Failed to allocate AVPacket payload.");
        }

        std::memcpy(pkt->data, packet_data.data(), packet_data.size());

        ret = avcodec_send_packet(codec_ctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0) {
            throw std::runtime_error(decoderName + ": Failed to send AVPacket.");
        }

        while (true) {
            AVFrame* frame = av_frame_alloc();
            if (!frame) {
                throw std::runtime_error(decoderName + ": Failed to allocate AVFrame.");
            }

            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&frame);
                break;
            }
            if (ret < 0) {
                av_frame_free(&frame);
                throw std::runtime_error(decoderName + ": Failed to receive AVFrame.");
            }

            frames.push_back(frame);
        }
    };

    auto flush_access_unit = [&]() {
        if (access_unit.empty()) {
            return;
        }

        std::vector<uint8_t> packet_data;
        packet_data.reserve(parameter_sets.size() + access_unit.size());

        if (!parameter_sets.empty()) {
            packet_data.insert(packet_data.end(), parameter_sets.begin(), parameter_sets.end());
        }
        packet_data.insert(packet_data.end(), access_unit.begin(), access_unit.end());

        send_packet_and_receive_frames(packet_data);
        access_unit.clear();
    };

    auto is_vcl = [](uint8_t nal_type) -> bool {
        return nal_type <= 31;
    };

    auto is_parameter_set = [](uint8_t nal_type) -> bool {
        return nal_type == 32 || nal_type == 33 || nal_type == 34;
    };

    auto first_slice_segment_in_pic_flag = [](const uint8_t* nal_ptr, size_t nal_size) -> bool {
        // HEVC NAL header is 2 bytes.
        // The first bit of the slice segment header follows immediately after.
        if (nal_size < 3) {
            return false;
        }
        return (nal_ptr[2] & 0x80) != 0;
    };

    bool have_started_picture = false;

    while (ptr + 4 <= map_size) {
        const size_t nal_size = bitstream_read_size_from_poiter(&bitstream[ptr], 4);
        ptr += 4;

        if (nal_size == 0) {
            continue;
        }
        if (ptr + nal_size > map_size) {
            av_packet_free(&pkt);
            throw std::runtime_error(decoderName + ": Corrupted HEVC bitstream (NAL exceeds buffer).");
        }

        const uint8_t* nal_ptr = &bitstream[ptr];
        const uint8_t nal_type = (nal_ptr[0] >> 1) & 0x3F;

        // Debug
        // printf("%s NAL type=%u size=%zu first_slice=%d\n",
        //        decoderName.c_str(), nal_type, nal_size,
        //        is_vcl(nal_type) ? (int)first_slice_segment_in_pic_flag(nal_ptr, nal_size) : -1);

        if (is_parameter_set(nal_type)) {
            append_annexb_nal(parameter_sets, nal_ptr, nal_size);
        }

        if (is_vcl(nal_type)) {
            const bool first_slice = first_slice_segment_in_pic_flag(nal_ptr, nal_size);

            // If this NAL starts a new picture and we already accumulated one,
            // flush the previous picture first.
            if (first_slice && have_started_picture && !access_unit.empty()) {
                flush_access_unit();
            }

            append_annexb_nal(access_unit, nal_ptr, nal_size);
            have_started_picture = true;
        } else {
            // Non-VCL NALs associated with the current AU
            append_annexb_nal(access_unit, nal_ptr, nal_size);
        }

        ptr += nal_size;
    }

    flush_access_unit();

    int ret = avcodec_send_packet(codec_ctx, nullptr);
    if (ret < 0) {
        av_packet_free(&pkt);
        throw std::runtime_error(decoderName + ": Failed to flush decoder.");
    }

    while (true) {
        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            av_packet_free(&pkt);
            throw std::runtime_error(decoderName + ": Failed to allocate AVFrame during flush.");
        }

        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            break;
        }
        if (ret < 0) {
            av_frame_free(&frame);
            av_packet_free(&pkt);
            throw std::runtime_error(decoderName + ": Failed to receive flushed AVFrame.");
        }

        frames.push_back(frame);
    }

    av_packet_free(&pkt);
    bitstream.clear();
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


void writeDecodedFramesToMapList_old(
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

    int Y_widths[height];
    int Y_linesizes[height];
    for (int y = 0; y < height; y++) {
        Y_widths[y] = y*width;
        Y_linesizes[y] = y*frames.front()->linesize[0];
    }

    int UV_widths[height];
    int UV_linesizes[height];
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

        // map.resize(sizeMap);
        // // Y
        // memcpy(map.data(), frame->data[0], sizeY); 
        // // U
        // memcpy(map.data() + sizeY, frame->data[1], sizeUV); 
        // // V
        // memcpy(map.data() + sizeY + sizeUV, frame->data[2], sizeUV);

        // // Copy Y plane
        // for (int y = 0; y < height; ++y) {
        //     memcpy(dstY + y*width, frame->data[0] + y*frame->linesize[0], width);
        // }

        // // Copy U plane
        // for (int y = 0; y < UV_height; ++y) {
        //     memcpy(dstU + y*UV_width, frame->data[1] + y*frame->linesize[1], UV_width);
        // }

        // // Copy V plane
        // for (int y = 0; y < UV_height; ++y) {
        //     memcpy(dstV + y*UV_width, frame->data[2] + y*frame->linesize[2], UV_width);
        // }

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

    int UV_widths[height];
    int UV_linesizes[height];
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

    writeDecodedFramesToMapList(gof, mapList, frames, decoderType_);

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