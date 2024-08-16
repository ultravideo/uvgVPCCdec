#include <fstream>

#include "uvgvpccdec/uvgvpccdec.hpp"
#include "Decompression/decompression.hpp"
#include "Reconstruction/reconstruction.hpp"
#include "PostReconstruction/postReconstruction.hpp"
#include "Adaptation/adaptation.hpp"

namespace uvgvpcc_dec
{

context dec_context_;
size_t num_threads_ = 30;
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
    /* ----------------- Parse GOF boundaries ----------------- */
    Decompression::parse_gofs(*chunk, dec_context_.raw_gofs);

    for (size_t gof_index = 0; gof_index < dec_context_.raw_gofs.size(); gof_index++) {

        /* ----------------- Create GOF structures and parse VPS ----------------- */
        dec_context_.latest_gof = std::make_shared<GOF>();
        dec_context_.gofs.push_back(dec_context_.latest_gof);
        dec_context_.latest_gof->raw_chunk = chunk;
        dec_context_.latest_gof->raw_gof = std::make_shared<uvgvpcc_dec::gof_info>(dec_context_.raw_gofs.at(gof_index));
        dec_context_.latest_gof->decoded_gof = std::make_shared<decompressed_gof>();
        dec_context_.latest_gof->decoded_gof->gof_index = gof_index;

        size_t frame_index_in_gof = 0;

        size_t vps_payload_start = dec_context_.latest_gof->raw_gof->vps_start + 4;
        Decompression::decompress_vps(vps_payload_start, gof_index);

        // composition_unit_index
        for (size_t cu_index = 0; cu_index < dec_context_.latest_gof->raw_gof->composition_units.size(); ++cu_index) {

            /* ----------------- Decompress composition unit ----------------- */
            // Raw composition unit (cu) contains the boundaries of different V3C units inside the composition unit
            auto raw_cu = std::make_shared<uvgvpcc_dec::composition_unit>(dec_context_.latest_gof->raw_gof->composition_units.at(cu_index));
            
            // Decoded composition unit is where the decompressed data of the V3C units is stored
            dec_context_.latest_gof->decoded_gof->composition_units.push_back(std::make_shared<decompressed_cu>());
            std::shared_ptr<decompressed_cu> dec_cu = dec_context_.latest_gof->decoded_gof->composition_units.at(cu_index);
            dec_cu->cu_index = cu_index;

            size_t atlas_payload_start = raw_cu->ad_start + 4;
            size_t atlas_payload_size = raw_cu->ad_size - 4;
            Decompression::decompress_atlas_sub_bitstream(atlas_payload_size, dec_cu.get(), atlas_payload_start);

            auto occ_dec = std::make_shared<Job>("Decompression::decompress_v3c_video_unit ",
                1, Decompression::decompress_v3c_video_unit, V3C_OVD, dec_context_.latest_gof->raw_chunk->data.data(), raw_cu, dec_cu);
            auto geo_dec = std::make_shared<Job>("Decompression::decompress_v3c_video_unit ",
                1, Decompression::decompress_v3c_video_unit, V3C_GVD, dec_context_.latest_gof->raw_chunk->data.data(), raw_cu, dec_cu);
            auto atr_dec = std::make_shared<Job>("Decompression::decompress_v3c_video_unit ",
                1, Decompression::decompress_v3c_video_unit, V3C_AVD, dec_context_.latest_gof->raw_chunk->data.data(), raw_cu, dec_cu);

            /* ----------------- Reconstruct frames contained in composition unit ----------------- */
            // Index runnning inside composition unit. Different from frame index running in GOF
            for (size_t frame_index_in_cu = 0; frame_index_in_cu < dec_cu->cu_frame_count; frame_index_in_cu++) {
                // Point cloud reconstruction -> per frame
                dec_context_.latest_gof->reconstructed_point_cloud = std::make_shared<point_cloud_frame>();
                dec_context_.latest_gof->reconstructed_point_cloud->frame_index_in_gof = frame_index_in_gof;

                auto pc_setup = std::make_shared<Job>("Reconstruction::setup_point_cloud_frame",
                    2, Reconstruction::setup_point_cloud_frame, dec_cu.get(), dec_context_.latest_gof->reconstructed_point_cloud.get(),
                    frame_index_in_cu, gof_index);

                // Notice post-process depedency on all patches
                auto pc_color = std::make_shared<Job>("PostReconstruction::PostProcess",
                    3, PostReconstruction::PostProcess, dec_cu.get(), dec_context_.latest_gof->reconstructed_point_cloud.get(),
                    frame_index_in_cu, gof_index);

                for ( std::size_t index = 0; index < dec_cu->atlas_map.at(frame_index_in_cu)->patches_map.size(); index++ ) {
                    auto pc_patch = std::make_shared<Job>("Reconstruction::process_patch",
                        2, Reconstruction::process_patch, index, dec_cu.get(), dec_context_.latest_gof->reconstructed_point_cloud.get(),
                        frame_index_in_cu, gof_index);
                    dec_context_.latest_gof->patch_jobs.push_back(pc_patch);
                    pc_patch->addDependency(pc_setup);
                    pc_color->addDependency(pc_patch);
                }

                auto pc_convert = std::make_shared<Job>("Adaptation::convertYUV8ToRGB8",
                    4, Adaptation::convertYUV8ToRGB8, dec_context_.latest_gof->reconstructed_point_cloud.get());
                
                auto pc_out = std::make_shared<Job>("Adaptation::output_decoded_frame",
                    5, Adaptation::output_decoded_frame, dec_context_.latest_gof->reconstructed_point_cloud, out);
                dec_context_.latest_gof->out_jobs.push_back(pc_out);
            
                pc_setup->addDependency(occ_dec);
                pc_setup->addDependency(geo_dec);
                pc_color->addDependency(atr_dec);
                pc_convert->addDependency(pc_color);
                pc_out->addDependency(pc_convert);

                if(!(frame_index_in_gof == 0 && gof_index == 0)) {
                    pc_out->addDependency(last_out_);
                }
                last_out_ = pc_out;
                frame_index_in_gof++;
                if (frame_index_in_cu == 0) {
                    dec_context_.queue->submitJob(occ_dec);
                    dec_context_.queue->submitJob(geo_dec);
                    dec_context_.queue->submitJob(atr_dec);
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

}

