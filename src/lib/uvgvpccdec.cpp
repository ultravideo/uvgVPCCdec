#include <fstream>

#include "uvgvpccdec/uvgvpccdec.hpp"
#include "Decompression/decompression.hpp"
#include "FormatConversion/formatConversion.hpp"
#include "Reconstruction/reconstruction.hpp"
#include "PostReconstruction/postReconstruction.hpp"
#include "Adaptation/adaptation.hpp"

namespace uvgvpcc_dec
{

context dec_context_;
size_t num_threads_ = 16;
std::shared_ptr<uvgvpcc_dec::Job> last_out_ = nullptr;

void readFile(const std::string filename, std::vector<uint8_t> &data);

void API::initializeDecoder(const Parameters& param)
{
    Logger::log(LogLevel::INFO, "API", "Initialize decoder " + std::to_string(param.hello) + "\n");
    dec_context_.queue = std::make_shared<ThreadQueue>(num_threads_);
    Decompression::initializeStaticParameters(param, &dec_context_);
    Reconstruction::initializeStaticParameters(param, &dec_context_);

}

void API::emptyFrameQueue() 
{
    if (last_out_ != nullptr) {
        dec_context_.queue->waitForJob(last_out_);
    }
    Logger::log(LogLevel::INFO, "API", "Frame queue empty \n");
}

void API::decodeV3CChunk(std::shared_ptr<v3c_chunk> chunk, uvgvpcc_dec::API::decoded_output* out) 
{
    Decompression::parse_gofs(*chunk, dec_context_.raw_gofs);
    for (size_t gof_index = 0; gof_index < dec_context_.raw_gofs.size(); gof_index++) {
        dec_context_.latest_gof = std::make_shared<GOF>();
        dec_context_.gofs.push_back(dec_context_.latest_gof);
        dec_context_.latest_gof->raw_chunk = chunk;
        dec_context_.latest_gof->raw_gof = std::make_shared<uvgvpcc_dec::gof_info>(dec_context_.raw_gofs.at(gof_index));
        dec_context_.latest_gof->decoded_gof = std::make_shared<decompressed_gof>();
        dec_context_.latest_gof->decoded_gof->gof_index = gof_index;

        size_t vps_payload_start = dec_context_.latest_gof->raw_gof->vps_start + 4;
        Decompression::decompress_vps(vps_payload_start, gof_index);
        size_t atlas_payload_start = dec_context_.latest_gof->raw_gof->ad_start + 4;
        size_t atlas_payload_size = dec_context_.latest_gof->raw_gof->ad_size - 4;
        Decompression::decompress_atlas_sub_bitstream(atlas_payload_size, dec_context_.latest_gof->decoded_gof.get(), atlas_payload_start);

        auto occ_dec = std::make_shared<Job>("Decompression::decompress_v3c_video_unit ",
            4, Decompression::decompress_v3c_video_unit, V3C_OVD, dec_context_.latest_gof->raw_chunk->data.data(), dec_context_.latest_gof->raw_gof, dec_context_.latest_gof->decoded_gof);
        auto geo_dec = std::make_shared<Job>("Decompression::decompress_v3c_video_unit ",
            4, Decompression::decompress_v3c_video_unit, V3C_GVD, dec_context_.latest_gof->raw_chunk->data.data(), dec_context_.latest_gof->raw_gof, dec_context_.latest_gof->decoded_gof);
        auto atr_dec = std::make_shared<Job>("Decompression::decompress_v3c_video_unit ",
            4, Decompression::decompress_v3c_video_unit, V3C_AVD, dec_context_.latest_gof->raw_chunk->data.data(), dec_context_.latest_gof->raw_gof, dec_context_.latest_gof->decoded_gof);
       
        /* ------------------ FORMAT CONVERSION ------------------ */
        auto format_conversion = std::make_shared<Job>("FormatConversion::convertToNominalFormat ",
            4, FormatConversion::convertToNominalFormat, dec_context_.latest_gof->decoded_gof.get());

        format_conversion->addDependency(occ_dec);
        format_conversion->addDependency(geo_dec);
        format_conversion->addDependency(atr_dec);

        for (size_t frame_index = 0; frame_index < dec_context_.latest_gof->decoded_gof->frame_count; frame_index++) {
            // Point cloud reconstruction -> per frame
            dec_context_.latest_gof->reconstructed_point_cloud = std::make_shared<point_cloud_frame>();

            auto pc_setup = std::make_shared<Job>("Reconstruction::setup_point_cloud_frame",
                1, Reconstruction::setup_point_cloud_frame, dec_context_.latest_gof->decoded_gof.get(), dec_context_.latest_gof->reconstructed_point_cloud.get(), frame_index);

            auto pc_color = std::make_shared<Job>("PostReconstruction::PostProcess",
                3, PostReconstruction::PostProcess, dec_context_.latest_gof->decoded_gof.get(), dec_context_.latest_gof->reconstructed_point_cloud.get(), frame_index);

            for ( std::size_t index = 0; index < dec_context_.latest_gof->decoded_gof->atlas_map.at(frame_index)->patches_map.size(); index++ ) {
                auto pc_patch = std::make_shared<Job>("Reconstruction::process_patch",
                    2, Reconstruction::process_patch, index, dec_context_.latest_gof->decoded_gof.get(), dec_context_.latest_gof->reconstructed_point_cloud.get(), frame_index);
                dec_context_.latest_gof->patch_jobs.push_back(pc_patch);
                pc_patch->addDependency(pc_setup);
                pc_color->addDependency(pc_patch);
            }

            auto pc_convert = std::make_shared<Job>("Adaptation::convertYUV8ToRGB8",
                3, Adaptation::convertYUV8ToRGB8, dec_context_.latest_gof->reconstructed_point_cloud.get());
            
            auto pc_out = std::make_shared<Job>("Adaptation::output_decoded_frame",
                5, Adaptation::output_decoded_frame, dec_context_.latest_gof->reconstructed_point_cloud, out);
            dec_context_.latest_gof->out_jobs.push_back(pc_out);
        
            pc_setup->addDependency(format_conversion);
            //pc_patch->addDependency(pc_setup);
            pc_convert->addDependency(pc_color);
            pc_out->addDependency(pc_convert);

            if(!(frame_index == 0 && gof_index == 0)) {
                pc_out->addDependency(last_out_);
            }
            last_out_ = pc_out;
            if (frame_index == 0) {
                dec_context_.queue->submitJob(occ_dec);
                dec_context_.queue->submitJob(geo_dec);
                dec_context_.queue->submitJob(atr_dec);
                dec_context_.queue->submitJob(format_conversion);
            }
            dec_context_.queue->submitJob(pc_setup);
            for (size_t i = 0; i < dec_context_.latest_gof->patch_jobs.size(); i++) {
                dec_context_.queue->submitJob(dec_context_.latest_gof->patch_jobs.at(i));
            }
            dec_context_.queue->submitJob(pc_color);
            dec_context_.queue->submitJob(pc_convert);
            dec_context_.queue->submitJob(pc_out);
        }
    }
}

}

