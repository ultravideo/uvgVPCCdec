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
size_t num_threads_ = 30;

void readFile(const std::string filename, std::vector<uint8_t> &data);

void API::initializeDecoder(const Parameters& param)
{
    Logger::log(LogLevel::INFO, "API", "Initialize decoder " + std::to_string(param.hello) + "\n");
    dec_context_.queue = std::make_shared<ThreadQueue>(num_threads_);
    Decompression::initializeStaticParameters(param, &dec_context_);
    Reconstruction::initializeStaticParameters(param, &dec_context_);

}

void API::decodeV3CChunk(v3c_chunk &chunk, uvgvpcc_dec::API::decoded_output* out) 
{

    std::vector<std::shared_ptr<uvgvpcc_dec::Job>> out_jobs;
    std::vector<std::shared_ptr<gof_info>> raw_gofs;
    std::vector<std::shared_ptr<decompressed_gof>> decompressed_gofs;
    std::vector<std::shared_ptr<point_cloud_frame>> point_cloud_frames;

    std::shared_ptr<uint8_t*> buffer = std::make_shared<uint8_t*>(chunk.data.data());
    //const uint8_t* buf = chunk.data.data();

    std::vector<uvgvpcc_dec::gof_info> gofs_infos = {};
    Decompression::parse_gofs(chunk, gofs_infos);
    for (size_t gof_index = 0; gof_index < gofs_infos.size(); gof_index++) {
        auto current_raw_gof = std::make_shared<uvgvpcc_dec::gof_info>(gofs_infos.at(gof_index));
        raw_gofs.push_back(current_raw_gof);
        std::shared_ptr<decompressed_gof> current_gof = std::make_shared<decompressed_gof>();
        current_gof->gof_index = gof_index;
        decompressed_gofs.push_back(current_gof);

        size_t vps_payload_start = current_raw_gof->vps_start + 4;
        Decompression::decompress_vps(vps_payload_start, current_gof->gof_index);
        size_t atlas_payload_start = current_raw_gof->ad_start + 4;
        size_t atlas_payload_size = current_raw_gof->ad_size - 4;
        Decompression::decompress_atlas_sub_bitstream(atlas_payload_size, current_gof.get(), atlas_payload_start);

        auto decompression = std::make_shared<Job>("Decompression::decompress_videos ",
            4, Decompression::decompress_videos, buffer, current_raw_gof, current_gof);

        /* ------------------ FORMAT CONVERSION ------------------ */
        auto format_conversion = std::make_shared<Job>("FormatConversion::convertToNominalFormat ",
            4, FormatConversion::convertToNominalFormat, current_gof.get());

        format_conversion->addDependency(decompression);

        for (size_t frame_index = 0; frame_index < current_gof->frame_count; frame_index++) {
            // Point cloud reconstruction -> per frame
            std::shared_ptr<point_cloud_frame> reconstructed_point_cloud_frame = std::make_shared<point_cloud_frame>();
            point_cloud_frames.push_back(reconstructed_point_cloud_frame);
            auto pc_reconstruct = std::make_shared<Job>("Reconstruction::construct_point_cloud_frame",
                1, Reconstruction::construct_point_cloud_frame, current_gof.get(), reconstructed_point_cloud_frame.get(), frame_index);
            
            auto pc_color = std::make_shared<Job>("PostReconstruction::PostProcess",
                2, PostReconstruction::PostProcess, current_gof.get(), reconstructed_point_cloud_frame.get(), frame_index);
    
            auto pc_convert = std::make_shared<Job>("Adaptation::convertYUV8ToRGB8",
                3, Adaptation::convertYUV8ToRGB8, reconstructed_point_cloud_frame.get());
            
            auto pc_out = std::make_shared<Job>("Adaptation::output_decoded_frame",
                5, Adaptation::output_decoded_frame, reconstructed_point_cloud_frame, out);
            out_jobs.push_back(pc_out);
        
            pc_reconstruct->addDependency(format_conversion);
            pc_color->addDependency(pc_reconstruct);
            pc_convert->addDependency(pc_color);
            pc_out->addDependency(pc_convert);

            if(frame_index != 0) {
                pc_out->addDependency(out_jobs.at(frame_index - 1));
            }

            if (frame_index == 0) {
                dec_context_.queue->submitJob(decompression);
                dec_context_.queue->submitJob(format_conversion);
            }
            dec_context_.queue->submitJob(pc_reconstruct);
            dec_context_.queue->submitJob(pc_color);
            dec_context_.queue->submitJob(pc_convert);
            dec_context_.queue->submitJob(pc_out);
            //dec_context_.queue->waitForJob(pc_write);
        }
    }
    std::cout << "out jobs size " << out_jobs.size() << std::endl;
    std::shared_ptr<uvgvpcc_dec::Job> last_out = out_jobs.back();
    dec_context_.queue->waitForJob(last_out);
    std::cout << "wait done " << std::endl;
}

}

