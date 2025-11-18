#include <cstddef>
#include <fstream>

#include "uvgvpccdec/uvgvpccdec.hpp"
#include "Decompression/decompression.hpp"
#include "Reconstruction/reconstruction.hpp"
#include "PostReconstruction/postReconstruction.hpp"
#include "Adaptation/adaptation.hpp"

namespace uvgvpcc_dec
{

context dec_context_;
bool fast_color_conversion_ = false;
std::shared_ptr<uvgvpcc_dec::Job> last_out_ = nullptr;

void readFile(const std::string filename, std::vector<uint8_t> &data);

void API::initializeDecoder(const Parameters& param)
{
    Logger::log<LogLevel::INFO>("API", "Initialize decoder " + std::to_string(param.hello) + "\n");
    dec_context_.queue = std::make_shared<ThreadQueue>(param.nbThread);
    dec_context_.p_ = &param;
    Decompression::initializeStaticParameters(param, &dec_context_);
    Reconstruction::initializeStaticParameters(param, &dec_context_);
    PostReconstruction::initializeStaticParameters(&dec_context_);
    fast_color_conversion_ = param.fast_color_conversion;

}

void API::emptyFrameQueue() 
{
    if (last_out_ != nullptr) {
        dec_context_.queue->waitForJob(last_out_);
    }
    Logger::log<LogLevel::INFO>("API", "Frame queue empty \n");
}

void API::decodeV3CChunk_serial(std::shared_ptr<v3c_chunk> chunk, uvgvpcc_dec::API::decoded_output* out) 
{
    /* ----------------- Parse GOF boundaries ----------------- */
    Decompression::parse_gofs(*chunk, dec_context_.raw_gofs);
    printf("Parsing GOFs ---------------> DONE\n");

    std::size_t frameId = 0;

    size_t gofs_size = dec_context_.raw_gofs.size();
    //size_t gofs_size = 4;
    for (size_t gof_index = 0; gof_index < gofs_size; gof_index++) {
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
        printf("Decompressing VPS for GOF %d -----------------> DONE\n", (int) gof_index);

        // composition_unit_index
        for (size_t cu_index = 0; cu_index < dec_context_.latest_gof->raw_gof->composition_unit_boundaries.size(); ++cu_index) {
            // lf : Seems that there is only one CU in each GOF

            /* ----------------- Decompress composition unit ----------------- */
            // Composition time (ct) contains the boundaries of different V3C units inside the composition unit
            auto raw_cu = std::make_shared<uvgvpcc_dec::composition_unit_boundary>(
                dec_context_.latest_gof->raw_gof->composition_unit_boundaries.at(cu_index));
            
            // Decoded composition unit is where the decompressed data of the V3C units is stored
            dec_context_.latest_gof->decoded_gof->composition_units.push_back(std::make_shared<composition_unit>());
            std::shared_ptr<composition_unit> dec_cu = dec_context_.latest_gof->decoded_gof->composition_units.at(cu_index);
            dec_cu->cu_index = cu_index;

            size_t atlas_payload_start = raw_cu->ad_start + 4;
            size_t atlas_payload_size = raw_cu->ad_size - 4;
            Decompression::decompress_atlas_sub_bitstream(atlas_payload_size, dec_cu.get(), atlas_payload_start);
            printf("Decompressing Atlas sub bitsream at CU_INDEX: %d -----------------------> DONE\n", (int) cu_index);

            // Decompression::decompress_v3c_video_unit: OCCUPANCY
            Decompression::decompress_v3c_video_unit(
                V3C_OVD, 
                dec_context_.latest_gof->raw_chunk->data.data(),
                raw_cu,
                dec_cu
            );
            //printf("Decompressing V3C_OVD - OCCUPANCY VPCC DATA ---------------> DONE\n");

            // Decompression::decompress_v3c_video_unit: GEOMETRY
            Decompression::decompress_v3c_video_unit(
                V3C_GVD, 
                dec_context_.latest_gof->raw_chunk->data.data(),
                raw_cu,
                dec_cu
            );

            // Decompression::decompress_v3c_video_unit: ATTRIBUTE
            Decompression::decompress_v3c_video_unit(
                V3C_AVD, 
                dec_context_.latest_gof->raw_chunk->data.data(),
                raw_cu,
                dec_cu
            );

            /* ----------------- Reconstruct frames contained in composition unit ----------------- */
            // Index runnning inside composition unit. Different from frame index running in GOF. 
            for (size_t frame_index_in_cu = 0; frame_index_in_cu < dec_cu->cu_frame_count; frame_index_in_cu++) {
                printf("Reconstructing frame %d of GOF %d\n", (int) frame_index_in_cu, (int) gof_index);

                /*--------------------- Setup point cloud frame ---------------------*/
                // Point cloud reconstruction -> per frame
                dec_context_.latest_gof->reconstructed_point_cloud = std::make_shared<point_cloud_frame>();
                dec_context_.latest_gof->reconstructed_point_cloud->gof_index = gof_index;
                dec_context_.latest_gof->reconstructed_point_cloud->frame_index_in_gof = frame_index_in_gof++;
                dec_context_.latest_gof->reconstructed_point_cloud->frameId = frameId++;
                Reconstruction::setup_point_cloud_frame(
                    dec_cu.get(),
                    dec_context_.latest_gof->reconstructed_point_cloud.get(),
                    frame_index_in_cu
                );
                /*##################################################################*/

                /*----------------- Reconstruct the point-cloud frame from the Atlas frame of the GOF -----------------*/
                for ( std::size_t index = 0; index < dec_cu->atlas_map.at(frame_index_in_cu)->patches_map.size(); index++ ) {
                    //printf("  Processing patch %d / %d\n", (int) index, (int) dec_cu->atlas_map.at(frame_index_in_cu)->patches_map.size());
                    Reconstruction::process_patch(
                    index,
                    dec_cu.get(),
                    dec_context_.latest_gof->reconstructed_point_cloud.get(),
                    frame_index_in_cu,
                    gof_index
                    );
                }
                /*####################################################################################################*/

                /*---------------------------- Post-reconstruction ----------------------------*/
                // Post-reconstruction : PostProcess
                PostReconstruction::PostProcess(
                    dec_cu.get(),
                    dec_context_.latest_gof->reconstructed_point_cloud.get(),
                    frame_index_in_cu,
                    gof_index
                );
                /*############################################################################*/

                /*-------------------------------- Adaptation --------------------------------*/
                if(dec_context_.p_->useTMC2AttributeYUVConversion) {
                    Adaptation::convertYUV16ToRGB8(dec_context_.latest_gof->reconstructed_point_cloud.get());
                } else {
                    if (fast_color_conversion_) {
                        Adaptation::convert_colors_fast(dec_context_.latest_gof->reconstructed_point_cloud.get());
                    } else {
                        Adaptation::convert_colors_slow(dec_context_.latest_gof->reconstructed_point_cloud.get());        
                    }
                }
                Adaptation::output_decoded_frame(dec_context_.latest_gof->reconstructed_point_cloud, out);
                /*############################################################################*/
            }
        }
    }
}


void API::decodeV3CChunk(std::shared_ptr<v3c_chunk> chunk, uvgvpcc_dec::API::decoded_output* out) 
{
    /* ----------------- Parse GOF boundaries ----------------- */
    Decompression::parse_gofs(*chunk, dec_context_.raw_gofs);

    std::size_t frameId = 0;

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
        for (size_t cu_index = 0; cu_index < dec_context_.latest_gof->raw_gof->composition_unit_boundaries.size(); ++cu_index) {
            // lf : Seems that there is only one CU in each GOF

            /* ----------------- Decompress composition unit ----------------- */
            // Raw composition unit (cu) contains the boundaries of different V3C units inside the composition unit
            auto raw_cu = std::make_shared<uvgvpcc_dec::composition_unit_boundary>(
                dec_context_.latest_gof->raw_gof->composition_unit_boundaries.at(cu_index));
            
            // Decoded composition unit is where the decompressed data of the V3C units is stored
            dec_context_.latest_gof->decoded_gof->composition_units.push_back(std::make_shared<composition_unit>());
            std::shared_ptr<composition_unit> dec_cu = dec_context_.latest_gof->decoded_gof->composition_units.at(cu_index);
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
                dec_context_.latest_gof->reconstructed_point_cloud->frameId = frameId++;

                auto pc_setup = std::make_shared<Job>("Reconstruction::setup_point_cloud_frame",
                    2, Reconstruction::setup_point_cloud_frame, dec_cu.get(), dec_context_.latest_gof->reconstructed_point_cloud.get(),
                    frame_index_in_cu);

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

                std::shared_ptr<Job> pc_convert;
                if(dec_context_.p_->useTMC2AttributeYUVConversion) {
                    pc_convert = std::make_shared<Job>("Adaptation::convertYUV16ToRGB8",
                        4, Adaptation::convertYUV16ToRGB8, dec_context_.latest_gof->reconstructed_point_cloud.get());
                } else {
                    if (fast_color_conversion_) {
                        pc_convert = std::make_shared<Job>("Adaptation::convertYUV8ToRGB8",
                            4, Adaptation::convert_colors_fast, dec_context_.latest_gof->reconstructed_point_cloud.get());
                    } else {
                        pc_convert = std::make_shared<Job>("Adaptation::convertYUV8ToRGB8",
                            4, Adaptation::convert_colors_slow, dec_context_.latest_gof->reconstructed_point_cloud.get());
                    }
                }
                
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

