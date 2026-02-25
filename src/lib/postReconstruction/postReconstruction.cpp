#include "postReconstruction.hpp"
#include "bitstreamParsing/vps.hpp"

namespace {



} // anonymous namespace

void PostReconstruction::applyPostProcess(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_) {
    const vps* vps = gof_->get_v3c_vps();

    bool multipleStreams_ = vps->vps_multiple_map_streams_present_flag_.at(0);
    const size_t layer_count = vps->vps_map_count_minus1_.at(0) + 1;

    for (const auto& frame : gofUVG->frames) {
        
        const size_t pointCount = frame->pointsPosList.size();
        const std::vector<uvgvpcc_dec::point3d>& pointsPixelsList = frame->pointsPixelsList;

        for (const auto& point : pointsPixelsList) {
            const size_t x = location.data_[0];
            const size_t y = location.data_[1];
            const size_t layer = location.data_[2];

            const std::vector<uint8_t>& attributeMap = (layer == 0) ? frame->attributeMapL1 : frame->attributeMapL2;
            /*
                if useTMC2AttributeYUVConversion {
                    color16bit
                } else {
                    color8bit
                }
            */
        }

    } // Frame
}