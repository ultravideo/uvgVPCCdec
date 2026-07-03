#include "atlas_context.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "atlas_frame.hpp"
#include "bitstream_common.hpp"
#include "bitstream_util.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

// PCCDecoder::createPatchFrameDataStructure
void atlas_context::write_atlas_tile_layer_rbsp_to_gof(const atlas_tile_layer_rbsp& rbsp, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG) {
    gofUVG->mapHeightGOF = asps_.asps_frame_height;
    gofUVG->gofId = gof_id_;
    std::shared_ptr<uvgvpcc_dec::Frame> frameUVG = std::make_shared<uvgvpcc_dec::Frame>();
    frameUVG->gofId = gof_id_;

    const atlas_tile_header& ath = rbsp.ath_;

    const int32_t quantizer_sizeX = 1 << ath.ath_patch_size_x_info_quantizer;
    const int32_t quantizer_sizeY = 1 << ath.ath_patch_size_y_info_quantizer;
    const double packing_block_sizeD = static_cast<double>(1 << asps_.asps_log2_patch_packing_block_size);
    const size_t occupancy_resolution = size_t(1) << asps_.asps_log2_patch_packing_block_size;

    //printf("Writing atlas tile layer rbsp to gofUVG, gofId: %zu\n", gof_id_);

    const size_t pid_count = rbsp.atdu_.patch_information_data_.size();
    const size_t minLevel = static_cast<size_t>(pow(2., rbsp.ath_.ath_pos_min_d_quantizer));

    std::vector<int32_t> ref_AFOC_list;
    // Set reference frames
    if (gofUVG->frames.size() > 0 && ath.ath_type != I_TILE) {
        const ref_list_struct& ref_list = ath.ath_ref_atlas_frame_list_asps_flag == 0 ? 
            ath.ref_list_struct_ : asps_.ref_lists.at(ath.ath_ref_atlas_frame_list_idx);
        
        for (size_t i = 0; i < ref_list.num_ref_entries; i++) {
            int deltaAfocSt = 0;
            if (ref_list.st_ref_atlas_frame_flags.at(i)) {
                deltaAfocSt = (2 * ref_list.strpf_entry_sign_flags.at(i) - 1) * ref_list.abs_delta_afoc_st.at(i);
            }
            int refPOC = i == 0 ? (int(gofUVG->frames.size()) - deltaAfocSt) : (ref_AFOC_list.at(i - 1) - deltaAfocSt);
            if (refPOC >= 0) {
                ref_AFOC_list.push_back(refPOC);
            }
        }
    }

    int64_t pred_index = 0;
    // printf("  Number of patches: %zu\n", pid_count);
    // printf("atlas_context::write_atlas_tile_layer_rbsp_to_gof ----------> start\n");
    for (size_t patch_index = 0; patch_index < pid_count; ++patch_index) {
        const patch_information_data& pid = rbsp.atdu_.patch_information_data_.at(patch_index);
        auto patch_mode = pid.patchMode_;

        if (patch_mode == P_INTRA || patch_mode == I_INTRA) { // Intra
            // printf("INTRA MODE\n");
            uvgvpcc_dec::Patch patchUVG;
            const patch_data_unit& pdu = pid.patch_data_unit_;
            patchUVG.occupancy_resolution = occupancy_resolution;
            patchUVG.patchPosXCanvasBlock = pdu.pdu_2d_pos_x; // u0
            patchUVG.patchPosYCanvasBlock = pdu.pdu_2d_pos_y; // v0
            patchUVG.posU_ = pdu.pdu_3d_offset_u;  // u1
            patchUVG.posV_ = pdu.pdu_3d_offset_v;  // v1
            if (pdu.pdu_lod_enabled_flag) {
                patchUVG.levelOfDetailX_ = pdu.pdu_lod_scale_x_minus1 + 1;
                patchUVG.levelOfDetailY_ = pdu.pdu_lod_scale_y_idc + (patchUVG.levelOfDetailX_ > 1 ? 1 : 2);
            } // else -> patchUVG.levelOfDetailX_ = 1, patchUVG.levelOfDetailY_ = 1 -> Default
            patchUVG.sizeD_ = pdu.pdu_3d_range_d == 0 ? 0 : (pdu.pdu_3d_range_d * minLevel - 1);
            if (asps_.asps_patch_size_quantizer_present_flag) {
                patchUVG.widthInPixel_   = (pdu.pdu_2d_size_x_minus1 + 1) * quantizer_sizeX; // size2DXInPixel_
                patchUVG.heightInPixel_  = (pdu.pdu_2d_size_y_minus1 + 1) * quantizer_sizeY; // size2DYInPixel_
                patchUVG.patchWidthCanvasBlock  = ceil(static_cast<double>(patchUVG.widthInPixel_) / packing_block_sizeD);
                patchUVG.patchHeightCanvasBlock = ceil(static_cast<double>(patchUVG.heightInPixel_) / packing_block_sizeD);
            } else {
                patchUVG.patchWidthCanvasBlock  = pdu.pdu_2d_size_x_minus1 + 1;
                patchUVG.patchHeightCanvasBlock = pdu.pdu_2d_size_y_minus1 + 1;
            }
            patchUVG.orientationIndex = pdu.pdu_orientation_index;
            patchUVG.setPatchPpiAndAxis(pdu.pdu_projection_id);
            patchUVG.posD_ = static_cast<int32_t>(pdu.pdu_3d_offset_d * minLevel);
            // if (patchUVG.normalAxis_ == 0) {
            //     patchUVG.tangentAxis_ = 2;
            //     patchUVG.bitangentAxis_ = 1;
            // } else if (patchUVG.normalAxis_ == 1) {
            //     patchUVG.tangentAxis_ = 2;
            //     patchUVG.bitangentAxis_ = 0;
            // } else {
            //     patchUVG.tangentAxis_ = 0;
            //     patchUVG.bitangentAxis_ = 1;
            // }
            
            // patchUVG.allocOneLayerData();
            if (asps_.asps_plr_enabled_flag) {
                // setPLRData( tile, patchUVG, pdu.getPLRData(), size_t( 1 ) << asps.getLog2PatchPackingBlockSize() );
            }
            
            patchUVG.patchPosXCanvas   = patchUVG.patchPosXCanvasBlock * occupancy_resolution;
            patchUVG.patchPosYCanvas   = patchUVG.patchPosYCanvasBlock * occupancy_resolution;
            patchUVG.patchWidthCanvas  = patchUVG.patchWidthCanvasBlock * occupancy_resolution;
            patchUVG.patchHeightCanvas = patchUVG.patchHeightCanvasBlock * occupancy_resolution;

            frameUVG->patchList.push_back(patchUVG);
        } else if (patch_mode == P_INTER) { // Inter
            uvgvpcc_dec::Patch patchUVG;
            const inter_patch_data_unit& ipdu = pid.inter_patch_data_unit_;
            
            patchUVG.occupancy_resolution = occupancy_resolution;
            patchUVG.bestMatchIdx_ = static_cast<int32_t>(ipdu.ref_patch_index + pred_index);
            pred_index += ipdu.ref_patch_index + 1;
            patchUVG.refAtlasFrameIdx_ = ipdu.ref_index;

            std::shared_ptr<uvgvpcc_dec::Frame>& ref_frame = gofUVG->frames.at(size_t(ref_AFOC_list.at(patchUVG.refAtlasFrameIdx_)));
            const uvgvpcc_dec::Patch& ref_patch = ref_frame->patchList.at(patchUVG.bestMatchIdx_);
            patchUVG.projectionMode_ = ref_patch.projectionMode_;
            patchUVG.setPatchPpiAndAxis(ref_patch.patchPpi_);
            patchUVG.patchPosXCanvasBlock = ipdu.pdu_2d_pos_x + ref_patch.patchPosXCanvasBlock;
            patchUVG.patchPosYCanvasBlock = ipdu.pdu_2d_pos_y + ref_patch.patchPosYCanvasBlock;
            patchUVG.orientationIndex = ref_patch.orientationIndex;
            patchUVG.posU_ = ipdu.pdu_3d_offset_u + ref_patch.posU_;
            patchUVG.posV_ = ipdu.pdu_3d_offset_v + ref_patch.posV_;
            if (asps_.asps_patch_size_quantizer_present_flag) {
                patchUVG.widthInPixel_   = (ref_patch.widthInPixel_ + ipdu.delta_2d_size_x) * quantizer_sizeX; // size2DXInPixel_
                patchUVG.heightInPixel_  = (ref_patch.heightInPixel_ + ipdu.delta_2d_size_y) * quantizer_sizeY; // size2DYInPixel_
                patchUVG.patchWidthCanvasBlock  = ceil(static_cast<double>(patchUVG.widthInPixel_) / packing_block_sizeD);
                patchUVG.patchHeightCanvasBlock = ceil(static_cast<double>(patchUVG.heightInPixel_) / packing_block_sizeD);
            } else {
                patchUVG.patchWidthCanvasBlock  = ref_patch.patchWidthCanvasBlock + ipdu.delta_2d_size_x;
                patchUVG.patchHeightCanvasBlock = ref_patch.patchHeightCanvasBlock + ipdu.delta_2d_size_y;
            }
            patchUVG.normalAxis_     = ref_patch.normalAxis_;
            patchUVG.tangentAxis_    = ref_patch.tangentAxis_;
            patchUVG.bitangentAxis_  = ref_patch.bitangentAxis_;
            patchUVG.axisOfAdditionalPlane_ = ref_patch.axisOfAdditionalPlane_;
            patchUVG.posD_           = (ipdu.pdu_3d_offset_d * minLevel) + ref_patch.posD_; // patchUVG.posD_ = (ipdu.pdu_3d_offset_d + (ref_patch.posD_ / minLevel)) * minLevel;
            const int64_t delta_DD   = ipdu.pdu_3d_range_d == 0 ? 0 : (ipdu.pdu_3d_range_d * minLevel - 1);
            patchUVG.sizeD_          = patchUVG.sizeD_ + delta_DD;
            patchUVG.levelOfDetailX_ = ref_patch.levelOfDetailX_;
            patchUVG.levelOfDetailY_ = ref_patch.levelOfDetailY_;

            // patchUVG.allocOneLayerData();
            if (asps_.asps_plr_enabled_flag) {
                // setPLRData( tile, patchUVG, pdu.getPLRData(), size_t( 1 ) << asps.getLog2PatchPackingBlockSize() );
            }
            
            patchUVG.patchPosXCanvas   = patchUVG.patchPosXCanvasBlock * occupancy_resolution;
            patchUVG.patchPosYCanvas   = patchUVG.patchPosYCanvasBlock * occupancy_resolution;
            patchUVG.patchWidthCanvas  = patchUVG.patchWidthCanvasBlock * occupancy_resolution;
            patchUVG.patchHeightCanvas = patchUVG.patchHeightCanvasBlock * occupancy_resolution;

            frameUVG->patchList.push_back(patchUVG);
        } else if (patch_mode == P_MERGE) {

        } else if (patch_mode == P_SKIP || rbsp.ath_.ath_type == SKIP_TILE) {

        } else if (patch_mode == P_RAW || patch_mode == I_RAW) {

        } else if (patch_mode == P_EOM || patch_mode == I_EOM) {

        } else {
            std::printf( "Error: unknow frame/patch type \n" );
        }
    }
    frameUVG->mapHeight = gofUVG->mapHeightGOF;
    gofUVG->frames.push_back(frameUVG);
    // printf("atlas_context::write_atlas_tile_layer_rbsp_to_gof ----------> Done, patch size = %zu\n", gofUVG->frames.back()->patchList.size());
}

void atlas_context::read_plr_data(plr_data& plrd, bitstream_t* stream) {
    for (size_t i = 0; i < asps_.asps_map_count_minus1 + 1; i++) {
        plr_information& plri = asps_.plr_information_list.at(i);
        if (plri.map_enabled_flag) {
            const size_t block_count = plrd.block_to_patch_map_width_ * plrd.block_to_patch_map_height_;
            const uint8_t bit_count_mode = static_cast<uint8_t>(ceilLog2(static_cast<uint32_t>(plri.num_modes_minus1)));
            if (block_count > plri.block_threshold_per_patch_minus1 + 1) {
                plrd.level_flag = bitstream_read(stream, 1) != 0U; 
            } else {
                plrd.level_flag = true;
            }

            if (!plrd.level_flag) {
                for (size_t block_index = 0; block_index < block_count; block_index++) {
                    plrd.block_present_flags.at(i)    = bitstream_read(stream, 1) != 0U;
                    if (plrd.block_present_flags.at(i)) {
                        plrd.block_mode_minus1s.at(i) = bitstream_read(stream, bit_count_mode);
                    }
                }
            } else {
                plrd.present_flag    = bitstream_read(stream, 1) != 0U;
                if (plrd.present_flag) {
                    plrd.mode_minus1 = bitstream_read(stream, bit_count_mode);
                }
            }
        }
    }
}

void atlas_context::read_plr_information(bitstream_t* stream) {
    asps_.plr_information_list.resize(asps_.asps_map_count_minus1 + 1);
    for (size_t j = 0; j < asps_.asps_map_count_minus1 + 1; j++) {
        plr_information& plri = asps_.plr_information_list.at(j);
        plri.map_enabled_flag                = bitstream_read(stream, 1) != 0U;
        if (plri.map_enabled_flag) {
            plri.num_modes_minus1            = bitstream_read(stream, 4);
            plri.allocate();
            for (size_t i = 0; i < plri.num_modes_minus1; i++) {
                plri.interpolate_flags.at(i) = bitstream_read(stream, 1) != 0U;
                plri.filling_flags.at(i)     = bitstream_read(stream, 1) != 0U;
                plri.minimum_depths.at(i)    = bitstream_read(stream, 2);
                plri.neighbour_minus1s.at(i) = bitstream_read(stream, 2);
            }
            plri.block_threshold_per_patch_minus1 = bitstream_read(stream, 6);
        }
    }
}

void atlas_context::read_atlas_seq_parameter_set(bitstream_t* stream) {
    // printf("Intial bytes size: %d, current bits: %d\n", stream->len, stream->cur_bit);

    asps_.asps_atlas_sequence_parameter_set_id = readUE(stream, "asps_atlas_sequence_parameter_set_id",get_gof_id());
    asps_.asps_frame_width = readUE(stream, "asps_frame_width",get_gof_id());
    asps_.asps_frame_height = readUE(stream, "asps_frame_height",get_gof_id());
    asps_.asps_geometry_3d_bit_depth_minus1 = readU(stream, 5, "asps_geometry_3d_bit_depth_minus1",get_gof_id());
    asps_.asps_geometry_2d_bit_depth_minus1 = readU(stream, 5, "asps_geometry_2d_bit_depth_minus1",get_gof_id());
    asps_.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 = readUE(stream, "asps_log2_max_atlas_frame_order_cnt_lsb_minus4",get_gof_id());
    asps_.asps_max_dec_atlas_frame_buffering_minus1 = readUE(stream, "asps_max_dec_atlas_frame_buffering_minus1",get_gof_id());
    asps_.asps_long_term_ref_atlas_frames_flag = readU(stream, 1, "asps_long_term_ref_atlas_frames_flag",get_gof_id()) != 0U;
    asps_.asps_num_ref_atlas_frame_lists_in_asps = readUE(stream, "asps_num_ref_atlas_frame_lists_in_asps",get_gof_id());
    asps_.ref_lists.resize(asps_.asps_num_ref_atlas_frame_lists_in_asps);
    printf("asps_num_ref_atlas_frame_lists_in_asps = %d\n", asps_.asps_num_ref_atlas_frame_lists_in_asps);
    for (size_t i = 0; i < asps_.asps_num_ref_atlas_frame_lists_in_asps; i++) {
        ref_list_struct& ref = asps_.ref_lists.at(i);
        const uint8_t num_ref_entries = readUE(stream, "num_ref_entries",get_gof_id());
        ref.num_ref_entries = num_ref_entries;
        
        ref.st_ref_atlas_frame_flags.resize(num_ref_entries);
        ref.abs_delta_afoc_st.resize(num_ref_entries);
        ref.strpf_entry_sign_flags.resize(num_ref_entries);
        ref.afoc_lsb_lt.resize(num_ref_entries);

        for (size_t j = 0; j < ref.num_ref_entries; ++j) {
            if (asps_.asps_long_term_ref_atlas_frames_flag) {
                ref.st_ref_atlas_frame_flags.at(j) = readU(stream, 1, "st_ref_atlas_frame_flags",get_gof_id());
            } else {
                ref.st_ref_atlas_frame_flags.at(j) = true; 
            }
            if (ref.st_ref_atlas_frame_flags.at(j)) {
                ref.abs_delta_afoc_st.at(j) = readUE(stream, "abs_delta_afoc_st",get_gof_id());
                if (ref.abs_delta_afoc_st.at(j) > 0) {
                    ref.strpf_entry_sign_flags.at(j) = readU(stream, 1, "straf_entry_sign_flag",get_gof_id());
                } else {
                    ref.strpf_entry_sign_flags.at(j) = true;
                }
            } else {
                ref.afoc_lsb_lt.at(j) = readU(stream, asps_.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 + 4, "afoc_lsb_lt",get_gof_id());
            }
        }
    }
    asps_.asps_use_eight_orientations_flag = readU(stream, 1, "asps_use_eight_orientations_flag",get_gof_id());
    asps_.asps_extended_projection_enabled_flag = readU(stream, 1, "asps_extended_projection_enabled_flag",get_gof_id());
    if (asps_.asps_extended_projection_enabled_flag) { 
        asps_.asps_max_number_projections_minus1 = readUE(stream, "asps_max_number_projections_minus1",get_gof_id());
    }
    asps_.asps_normal_axis_limits_quantization_enabled_flag = readU(stream, 1, "asps_normal_axis_limits_quantization_enabled_flag",get_gof_id());
    asps_.asps_normal_axis_max_delta_value_enabled_flag = readU(stream, 1, "asps_normal_axis_max_delta_value_enabled_flag",get_gof_id());
    asps_.asps_patch_precedence_order_flag = readU(stream, 1, "asps_patch_precedence_order_flag",get_gof_id());
    asps_.asps_log2_patch_packing_block_size = readU(stream, 3, "asps_log2_patch_packing_block_size",get_gof_id());
    asps_.asps_patch_size_quantizer_present_flag = readU(stream, 1, "asps_patch_size_quantizer_present_flag",get_gof_id());
    asps_.asps_map_count_minus1 = readU(stream, 4, "asps_map_count_minus1",get_gof_id());
    asps_.asps_pixel_deinterleaving_enabled_flag = readU(stream, 1, "asps_pixel_deinterleaving_enabled_flag",get_gof_id());

    if (asps_.asps_pixel_deinterleaving_enabled_flag) { 
        asps_.asps_map_pixel_deinterleaving_flag.resize(asps_.asps_map_count_minus1 + 1, false);
        for (size_t j = 0; j < asps_.asps_map_count_minus1 + 1; ++j) {
            asps_.asps_map_pixel_deinterleaving_flag.at(j) = readU(stream, 1, "asps_map_pixel_deinterleaving_flag",get_gof_id());
        }
    }
    asps_.asps_raw_patch_enabled_flag = readU(stream, 1, "asps_raw_patch_enabled_flag",get_gof_id());
    asps_.asps_eom_patch_enabled_flag = readU(stream, 1, "asps_eom_patch_enabled_flag",get_gof_id());

    if (asps_.asps_eom_patch_enabled_flag && asps_.asps_map_count_minus1 == 0) {
        asps_.asps_eom_fix_bit_count_minus1 = readU(stream, 4, "asps_eom_fix_bit_count_minus1",get_gof_id());
    }
    if (asps_.asps_raw_patch_enabled_flag || asps_.asps_eom_patch_enabled_flag) {
        asps_.asps_auxiliary_video_enabled_flag = readU(stream, 1, "asps_auxiliary_video_enabled_flag",get_gof_id());
    }

    asps_.asps_plr_enabled_flag = readU(stream, 1, "asps_plr_enabled_flag",get_gof_id());
    if (asps_.asps_plr_enabled_flag) {
        read_plr_information(stream);
    }
    asps_.asps_vui_parameters_present_flag = readU(stream, 1, "asps_vui_parameters_present_flag",get_gof_id());
    if (asps_.asps_vui_parameters_present_flag) {
        throw std::runtime_error("Handling ASPS VUI parameters not implemented");
    }
    
    asps_.asps_extension_flag = readU(stream, 1, "asps_extension_present_flag",get_gof_id());
    if (asps_.asps_extension_flag) {
        printf("ASPS extension present flag is set. Reading ASPS extension data...\n");
        asps_.asps_vpcc_extension_flag = readU(stream, 1, "asps_vpcc_extension_present_flag",get_gof_id());
        // asps_.asps_miv_extension_present_flag = readU(stream, 1, "asps_miv_extension_present_flag",get_gof_id());
        asps_.asps_extension_7bits = readU(stream, 7, "asps_extension_7bits",get_gof_id());
    }
    if (asps_.asps_vpcc_extension_flag) {
        asps_.asps_vpcc_remove_duplicate_point_enabled_flag = readU(stream, 1, "asps_vpcc_remove_duplicate_point_enabled_flag",get_gof_id());
        if (asps_.asps_pixel_deinterleaving_enabled_flag || asps_.asps_plr_enabled_flag) {
            asps_.asps_vpcc_surface_thickness_minus1 = readUE(stream, "asps_vpcc_surface_thickness_minus1",get_gof_id());
        }
    }
    if (asps_.asps_extension_7bits) {
        while (moreRbspData(stream)) {
            bitstream_advance(stream, 1);
        }
    }
    // printf("Before bitstream_align, current bytes: %d, current bits: %d\n", stream->len, stream->cur_bit);
    bitstream_align_rbsp_trailing_bits(stream);
    // printf("After bitstream_align, current bytes: %d, current bits: %d\n", stream->len, stream->cur_bit);
}

void atlas_context::read_atlas_frame_parameter_set(bitstream_t* stream) {
    afps_.afps_atlas_frame_parameter_set_id               = readUE(stream, "afps_atlas_frame_parameter_set_id",get_gof_id());
    afps_.afps_atlas_sequence_parameter_set_id            = readUE(stream, "afps_atlas_sequence_parameter_set_id",get_gof_id());
    
    /* atlasFrameTileInformationRbsp */
    afps_.afti.afti_single_tile_in_atlas_frame_flag                        = readU(stream, 1, "afti_single_tile_in_atlas_frame_flag",get_gof_id()) != 0U;
    if (!afps_.afti.afti_single_tile_in_atlas_frame_flag) {
        afps_.afti.afti_uniform_partition_spacing_flag                     = readU(stream, 1, "afti_uniform_partition_spacing_flag",get_gof_id()) != 0U;
        if (afps_.afti.afti_uniform_partition_spacing_flag) {
            afps_.afti.afti_partition_column_width_minus1.at(0) 
                                                                           = readUE(stream, "afti_partition_column_width_minus1.at(0)",get_gof_id());
            afps_.afti.afti_partition_row_height_minus1.at(0) 
                                                                           = readUE(stream, "afti_partition_row_height_minus1.at(0)",get_gof_id());
            afps_.afti.afti_num_partition_columns_minus1  = ceil(asps_.asps_frame_width / ((afps_.afti.afti_partition_column_width_minus1.at(0)+1)*64.0)) - 1;
            afps_.afti.afti_num_partition_rows_minus1     = ceil(asps_.asps_frame_width / ((afps_.afti.afti_partition_row_height_minus1.at(0)+1)*64.0)) - 1;
        } else {
            afps_.afti.afti_num_partition_columns_minus1                   = readUE(stream, "afti_num_partition_columns_minus1",get_gof_id());
            afps_.afti.afti_num_partition_rows_minus1                      = readUE(stream, "afti_num_partition_rows_minus1",get_gof_id());
            afps_.afti.afti_partition_column_width_minus1.resize(afps_.afti.afti_num_partition_columns_minus1);
            afps_.afti.afti_partition_row_height_minus1.resize(afps_.afti.afti_num_partition_columns_minus1);
            for (size_t i = 0; i < afps_.afti.afti_num_partition_columns_minus1; i++) {
                afps_.afti.afti_partition_column_width_minus1.at(i)        = readUE(stream, "afti.afti_partition_column_width_minus1.at(i)",get_gof_id());
            }
            for (size_t i = 0; i < afps_.afti.afti_num_partition_rows_minus1; i++) {
                afps_.afti.afti_partition_row_height_minus1.at(i)          = readUE(stream, "afti.afti_partition_row_height_minus1.at(i)",get_gof_id());
            }
        }
        afps_.afti.afti_single_partition_per_tile_flag                     = readU(stream, 1, "afti_single_partition_per_tile_flag",get_gof_id());
        if (afps_.afti.afti_single_partition_per_tile_flag == 0U) {
            uint32_t numPartitionsInAtlasFrame = (afps_.afti.afti_num_partition_columns_minus1 + 1) * (afps_.afti.afti_num_partition_rows_minus1 + 1);
            afps_.afti.afti_num_tiles_in_atlas_frame_minus1                = readUE(stream, "afti.afti_num_tiles_in_atlas_frame_minus1",get_gof_id());
            afps_.afti.afti_top_left_partition_idx.resize(afps_.afti.afti_num_tiles_in_atlas_frame_minus1 == 0 ? 1 : afps_.afti.afti_num_tiles_in_atlas_frame_minus1);
            afps_.afti.afti_bottom_right_partition_column_offset.resize(afps_.afti.afti_num_tiles_in_atlas_frame_minus1 == 0 ? 1 : afps_.afti.afti_num_tiles_in_atlas_frame_minus1);
            afps_.afti.afti_bottom_right_partition_row_offset.resize(afps_.afti.afti_num_tiles_in_atlas_frame_minus1 == 0 ? 1 : afps_.afti.afti_num_tiles_in_atlas_frame_minus1);
            for (size_t i = 0; i <= afps_.afti.afti_num_tiles_in_atlas_frame_minus1; i++) {
                afps_.afti.afti_top_left_partition_idx.at(i)               = readU(stream, ceilLog2(numPartitionsInAtlasFrame), "afti_single_partition_per_tile_flag",get_gof_id());
                afps_.afti.afti_bottom_right_partition_column_offset.at(i) = readUE(stream, "afti_bottom_right_partition_column_offset.at(i)",get_gof_id());
                afps_.afti.afti_bottom_right_partition_row_offset.at(i)    = readUE(stream, "afti_bottom_right_partition_row_offset.at(i)",get_gof_id());
            }
        } else {
            afps_.afti.afti_num_tiles_in_atlas_frame_minus1 = (afps_.afti.afti_num_partition_columns_minus1 + 1) * (afps_.afti.afti_num_partition_rows_minus1 + 1) - 1;
            afps_.afti.afti_top_left_partition_idx.resize(afps_.afti.afti_num_tiles_in_atlas_frame_minus1 == 0 ? 1 : afps_.afti.afti_num_tiles_in_atlas_frame_minus1);
            afps_.afti.afti_bottom_right_partition_column_offset.resize(afps_.afti.afti_num_tiles_in_atlas_frame_minus1 == 0 ? 1 : afps_.afti.afti_num_tiles_in_atlas_frame_minus1);
            afps_.afti.afti_bottom_right_partition_row_offset.resize(afps_.afti.afti_num_tiles_in_atlas_frame_minus1 == 0 ? 1 : afps_.afti.afti_num_tiles_in_atlas_frame_minus1);
            for (size_t i = 0; i <= afps_.afti.afti_num_tiles_in_atlas_frame_minus1; i++) {
                afps_.afti.afti_top_left_partition_idx.at(i)               = static_cast<uint32_t>(i); // afti.setTopLeftPartitionIdx( i, i );
                afps_.afti.afti_bottom_right_partition_column_offset.at(i) = 0;
                afps_.afti.afti_bottom_right_partition_row_offset.at(i)    = 0;
            }
        }
    } // else afti_single_tile_in_atlas_frame_flag = false -> default
    if (asps_.asps_auxiliary_video_enabled_flag) {
        afps_.afti.afti_auxiliary_video_tile_row_width_minus1              = readUE(stream, "afti_auxiliary_video_tile_row_width_minus1",get_gof_id());
        afps_.afti.afti_auxiliary_video_tile_row_height.resize(afps_.afti.afti_num_tiles_in_atlas_frame_minus1);
        for (size_t i = 0; i <= afps_.afti.afti_num_tiles_in_atlas_frame_minus1; i++) {
            afps_.afti.afti_auxiliary_video_tile_row_height.at(i)          = readUE(stream, "afti_auxiliary_video_tile_row_height.at(i)",get_gof_id());
        }
    }
    afps_.afti.afti_signalled_tile_id_flag                                 = readU(stream, 1, "afti_signalled_tile_id_flag",get_gof_id()) != 0U;
    afps_.afti.afti_tile_id.resize(afps_.afti.afti_num_tiles_in_atlas_frame_minus1 == 0 ? 1 : afps_.afti.afti_num_tiles_in_atlas_frame_minus1);
    if (afps_.afti.afti_signalled_tile_id_flag) {
        afps_.afti.afti_signalled_tile_id_length_minus1                    = readUE(stream, "afti_signalled_tile_id_length_minus1",get_gof_id());
        for (size_t i = 0; i <= afps_.afti.afti_num_tiles_in_atlas_frame_minus1; i++) {
            afps_.afti.afti_tile_id.at(i)                                  = readU(stream, afps_.afti.afti_signalled_tile_id_length_minus1, "afti_tile_id.at(i)",get_gof_id());
        }
    } else {
        for (size_t i = 0; i <= afps_.afti.afti_num_tiles_in_atlas_frame_minus1; i++) {
            afps_.afti.afti_tile_id.at(i) = static_cast<uint32_t>(i);
        }
    }
    /*>>>>>>>>>>>>>>>>>>>>>>>> atlasFrameTileInformationRbsp <<<<<<<<<<<<<<<<<<<<<<<<<<<<*/

    afps_.afps_output_flag_present_flag                   = readU(stream, 1, "afps_output_flag_present_flag",get_gof_id());
    afps_.afps_num_ref_idx_default_active_minus1          = readUE(stream, "afps_num_ref_idx_default_active_minus1",get_gof_id());
    afps_.afps_additional_lt_afoc_lsb_len                 = readUE(stream, "afps_additional_lt_afoc_lsb_len",get_gof_id());
    afps_.afps_lod_mode_enabled_flag                      = readU(stream, 1, "afps_lod_mode_enabled_flag",get_gof_id());
    afps_.afps_raw_3d_offset_bit_count_explicit_mode_flag = readU(stream, 1, "afps_raw_3d_offset_bit_count_explicit_mode_flag",get_gof_id());
    afps_.afps_extension_flag                             = readU(stream, 1, "afps_extension_flag",get_gof_id());
    if (afps_.afps_extension_flag) {
        afps_.afps_extension_8bits                        = readU(stream, 8, "afps_extension_8bits",get_gof_id());
    }

    if (afps_.afps_extension_8bits) { // Don't know what is the meaning of this step
        while (moreRbspData(stream)) {};
    }

    bitstream_align_rbsp_trailing_bits(stream);
}

void atlas_context::read_atlas_tile_header(atlas_tile_header &ath, NAL_UNIT_TYPE nalu_t, bitstream_t* stream) {
    if (nalu_t >= NAL_BLA_W_LP && nalu_t <= NAL_RSV_IRAP_ACL_29) {
        ath.ath_no_output_of_prior_atlas_frames_flag = readU(stream, 1, "ath_no_output_of_prior_atlas_frames_flag", get_gof_id());
    }
    if (nalu_t == NAL_TRAIL_R) { ath.ath_tile_nalu_type_info = 1; };
    if (nalu_t == NAL_TRAIL_N) { ath.ath_tile_nalu_type_info = 2; };
    ath.ath_atlas_frame_parameter_set_id = readUE(stream, "ath_atlas_frame_parameter_set_id", get_gof_id());
    ath.ath_atlas_adaptation_parameter_set_id = readUE(stream, "ath_atlas_adaptation_parameter_set_id", get_gof_id());
    if (afps_.afti.afti_signalled_tile_id_flag) {
        ath.ath_id = readU(stream, afps_.afti.afti_signalled_tile_id_length_minus1 + 1, "ath_id", get_gof_id());
    } else {
        if (afps_.afti.afti_num_tiles_in_atlas_frame_minus1 != 0) {
            ath.ath_id = bitstream_read(stream, ceilLog2(afps_.afti.afti_num_tiles_in_atlas_frame_minus1 + 1));
        } // else ath.ath_id = 0 -> default
    }
    ath.ath_type = static_cast<ATH_TYPE> (readUE(stream, "ath_type", get_gof_id()));
    if (afps_.afps_output_flag_present_flag) {
        ath.ath_atlas_output_flag = readU(stream, 1, "ath_atlas_output_flag", get_gof_id()) != 0U;
    } // else ath.ath_atlas_output_flag = false -> default
    const uint8_t Log2MaxAtlasFrmOrderCntLsb = asps_.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 + 4;
    ath.ath_atlas_frm_order_cnt_lsb = readU(stream, Log2MaxAtlasFrmOrderCntLsb, "ath_atlas_frm_order_cnt_lsb", get_gof_id()); // u(v)

    if (asps_.asps_num_ref_atlas_frame_lists_in_asps > 0) {
        ath.ath_ref_atlas_frame_list_asps_flag = readU(stream, 1, "ath_ref_atlas_frame_list_asps_flag", get_gof_id()) != 0U;
    } // else ath.ath_ref_atlas_frame_list_asps_flag = false -> default
    ath.ath_ref_atlas_frame_list_idx = 0;
    if (ath.ath_ref_atlas_frame_list_asps_flag == 0) {
        ref_list_struct& ref = ath.ref_list_struct_;
        const uint8_t num_ref_entries = readUE(stream, "num_ref_entries",get_gof_id());
        ref.num_ref_entries = num_ref_entries;
        
        ref.st_ref_atlas_frame_flags.resize(num_ref_entries);
        ref.abs_delta_afoc_st.resize(num_ref_entries);
        ref.strpf_entry_sign_flags.resize(num_ref_entries);
        ref.afoc_lsb_lt.resize(num_ref_entries);

        for (size_t i = 0; i < ref.num_ref_entries; ++i) {
            if (asps_.asps_long_term_ref_atlas_frames_flag) {
                ref.st_ref_atlas_frame_flags.at(i) = readU(stream, 1, "st_ref_atlas_frame_flags",get_gof_id()) != 0U;
            } else {
                ref.st_ref_atlas_frame_flags.at(i) = true; 
            }
            if (ref.st_ref_atlas_frame_flags.at(i)) {
                ref.abs_delta_afoc_st.at(i) = readUE(stream, "abs_delta_afoc_st",get_gof_id());
                if (ref.abs_delta_afoc_st.at(i) > 0) {
                    ref.strpf_entry_sign_flags.at(i) = readU(stream, 1, "straf_entry_sign_flag",get_gof_id()) != 0U;
                } else {
                    ref.strpf_entry_sign_flags.at(i) = true;
                }
            } else {
                ref.afoc_lsb_lt.at(i) = readU(stream, asps_.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 + 4, "afoc_lsb_lt",get_gof_id());
            }
        }
    } else if (asps_.asps_num_ref_atlas_frame_lists_in_asps > 1) { 
        // const uint8_t bit_len = std::ceil(std::log2(asps_.asps_num_ref_atlas_frame_lists_in_asps));
        const uint8_t bit_len = ceilLog2(asps_.asps_num_ref_atlas_frame_lists_in_asps);
        ath.ath_ref_atlas_frame_list_idx = readU(stream, bit_len, "ath_ref_atlas_frame_list_idx", get_gof_id());
    }
    if (ath.ath_ref_atlas_frame_list_asps_flag) {
        ath.ref_list_struct_ = asps_.ref_lists.at(ath.ath_ref_atlas_frame_list_idx);
    }
    auto& ref_list = ath.ath_ref_atlas_frame_list_asps_flag ? asps_.ref_lists.at(ath.ath_ref_atlas_frame_list_idx) : ath.ref_list_struct_;
    size_t numLtrAtlasFrmEntries = 0;  
    for ( size_t i = 0; i < ref_list.num_ref_entries; i++ ) {
        if (!ref_list.st_ref_atlas_frame_flags.at(i)) {numLtrAtlasFrmEntries++;}
    }
    ath.ath_additional_afoc_lsb_present_flag.resize(numLtrAtlasFrmEntries, 0);
    ath.ath_additional_afoc_lsb_val.resize(numLtrAtlasFrmEntries, 0);
    for (size_t j = 0; j < numLtrAtlasFrmEntries; j++) {
        ath.ath_additional_afoc_lsb_present_flag.at(j) = readU(stream, 1, "ath_additional_afoc_lsb_present_flag", get_gof_id()) != 0U;
        if (ath.ath_additional_afoc_lsb_present_flag.at(j)) {
            ath.ath_additional_afoc_lsb_val.at(j) = readU(stream, afps_.afps_additional_lt_afoc_lsb_len, "ath_additional_afoc_lsb_val", get_gof_id());
        }
    }
    if (ath.ath_type != SKIP_TILE) {
        if (asps_.asps_normal_axis_limits_quantization_enabled_flag) {
            ath.ath_pos_min_d_quantizer = readU(stream, 5, "ath_pos_min_d_quantizer", get_gof_id());
            
            if (asps_.asps_normal_axis_max_delta_value_enabled_flag) {
                ath.ath_pos_delta_max_d_quantizer = readU(stream, 5, "ath_pos_delta_max_d_quantizer",get_gof_id());
            }
        }
        if (asps_.asps_patch_size_quantizer_present_flag) {
            ath.ath_patch_size_x_info_quantizer = readU(stream, 3, "ath_patch_size_x_info_quantizer",get_gof_id());
            ath.ath_patch_size_y_info_quantizer = readU(stream, 3, "ath_patch_size_y_info_quantizer",get_gof_id());
        }
        if (afps_.afps_raw_3d_offset_bit_count_explicit_mode_flag) {
            // const uint8_t bit_len = std::floor(std::log2(asps_.asps_geometry_3d_bit_depth_minus1 + 1));
            const uint8_t bit_len = floorLog2(asps_.asps_geometry_3d_bit_depth_minus1 + 1);
            ath.ath_raw_3d_offset_axis_bit_count_minus1 = readU(stream, bit_len, "ath_raw_3d_offset_axis_bit_count_minus1",get_gof_id());
        } else {
            ath.ath_raw_3d_offset_axis_bit_count_minus1 = std::max(0, asps_.asps_geometry_3d_bit_depth_minus1 - asps_.asps_geometry_2d_bit_depth_minus1) - 1;
        }
        if (ath.ath_type == ATH_TYPE::P_TILE && ref_list.num_ref_entries > 1) {
            ath.ath_num_ref_idx_active_override_flag = readU(stream, 1, "ath_num_ref_idx_active_override_flag",get_gof_id()) != 0U;
            if (ath.ath_num_ref_idx_active_override_flag) {
                ath.ath_num_ref_idx_active_minus1 = readUE(stream, "ath_num_ref_idx_active_minus1",get_gof_id());
            }
        }
    }
    bitstream_align(stream);
}

void atlas_context::read_atlas_tile_data_unit(atlas_tile_data_unit& atdu, const atlas_tile_header& ath, bitstream_t* stream) {
    // const uint16_t tileID = ath.ath_id;
    if (ath.ath_type == SKIP_TILE) {
        // skipPatchDataUnit(bitstream);
        //  This is just empty?
    } else {
        // while(true) {
        //     atdu.atdu_patch_mode = readUE(stream, "atdu_patch_mode", get_gof_id());
        //     if (atdu.atdu_patch_mode == ATDU_PATCH_MODE_I_TILE::I_END
        //         || atdu.atdu_patch_mode == ATDU_PATCH_MODE_P_TILE::P_END) {
        //         printf("atdu.atdu_patch_mode == ATDU_PATCH_MODE_I_TILE::I_END || atdu.atdu_patch_mode == ATDU_PATCH_MODE_P_TILE::P_END\n");
        //         break;
        //     }
        //     patch_information_data pid;
        //     pid.patchMode = atdu.atdu_patch_mode;
        //     read_patch_information_data(pid, ath, stream);
        //     atdu.patch_information_data_.push_back(pid);
        // }
        size_t patchIndex  = 0;
        prev_patch_size_u_ = 0;
        prev_patch_size_v_ = 0;
        pred_patch_index_  = 0;
        uint8_t patchMode  = readUE(stream, "atdu_tile_order", get_gof_id());  // ue(v)
        while (( patchMode != I_END ) && ( patchMode != P_END )) {
            patch_information_data pid;
            pid.patchMode_ = patchMode;
            pid.tileOrder_ = atdu.tile_order_;
            pid.patchIndex_ = patchIndex;
            patchIndex++;
            // printf("stream size = %zu, bytes = %d, bits = %d\n", stream->data.size(), stream->len, stream->cur_bit);
            read_patch_information_data(pid, ath, stream);
            atdu.patch_information_data_.push_back(pid);

            patchMode = readUE(stream, "atdu_tile_order", get_gof_id());  // ue(v)
        }
        prev_frame_index_ = atdu.tile_order_;
    }
}

void atlas_context::read_patch_information_data(patch_information_data& pid, const atlas_tile_header& ath, bitstream_t* stream) {
    // if (ath.ath_type == SKIP_TILE) {
    //     // skip mode: currently not supported but added it for convenience. Could
    //     // easily be removed
    // } else if (ath.ath_type == P_TILE) {
    //     if (pid.patchMode == P_SKIP) {
    //         // skip mode: currently not supported but added it for convenience. Could
    //         // easily be removed
    //         // skipPatchDataUnit(bitstream);
    //     } else if (pid.patchMode == P_MERGE) {
    //         const auto& mpdu = pid.merge_patch_data_unit_;
    //         // mergePatchDataUnit(mpdu, ath, syntax, bitstream);
    //     } else if (pid.patchMode == P_INTRA) {
    //         const auto& pdu = pid.patch_data_unit_;
    //         // patchDataUnit(pdu, ath, syntax, bitstream);
    //     } else if (pid.patchMode == P_INTER) {
    //         const auto& ipdu = pid.inter_patch_data_unit_;
    //         // interPatchDataUnit(ipdu, ath, syntax, bitstream);
    //     } else if (pid.patchMode == P_RAW) {
    //         const auto& rpdu = pid.raw_patch_data_unit_;
    //         // rawPatchDataUnit(rpdu, ath, syntax, bitstream);
    //     } else if (pid.patchMode == P_EOM) {
    //         const auto& epdu = pid.eom_patch_data_unit_;
    //         // eomPatchDataUnit(epdu, ath, syntax, bitstream);
    //     }
    // } else if (ath.ath_type == I_TILE) {  // currently only use I_TILE types
    //     if (pid.patchMode == I_INTRA) {
    //         auto& pdu = pid.patch_data_unit_;
    //         read_patch_data_unit(pdu, ath, stream);
    //         // patchDataUnit(pdu, ath, syntax, bitstream);
    //     } else if (pid.patchMode == I_RAW) {
    //         const auto& rpdu = pid.raw_patch_data_unit_;
    //         // rawPatchDataUnit(rpdu, ath, syntax, bitstream);
    //     } else if (pid.patchMode == I_EOM) {
    //         const auto& epdu = pid.eom_patch_data_unit_;
    //         // eomPatchDataUnit(epdu, ath, syntax, bitstream);
    //     }
    // }

    if (ath.ath_type == SKIP_TILE) {
        // skip mode: currently not supported but added it for convenience. Could
        // easily be removed
    } else if (ath.ath_type == P_TILE) {
        if (pid.patchMode_ == P_SKIP) {
            // skip mode: currently not supported but added it for convenience. Could
            // easily be removed
            // skipPatchDataUnit(bitstream);
        } else if (pid.patchMode_ == P_MERGE) {
            // const auto& mpdu = pid.merge_patch_data_unit_;
            // mergePatchDataUnit(mpdu, ath, syntax, bitstream);
        } else if (pid.patchMode_ == P_INTRA) {
            auto& pdu = pid.patch_data_unit_;
            read_patch_data_unit(pdu, ath, stream);
            // patchDataUnit(pdu, ath, syntax, bitstream);
        } else if (pid.patchMode_ == P_INTER) {
            auto& ipdu = pid.inter_patch_data_unit_;
            read_inter_patch_data_unit(ipdu, ath, stream);
            // const auto& ipdu = pid.inter_patch_data_unit_;
            // interPatchDataUnit(ipdu, ath, syntax, bitstream);
        } else if (pid.patchMode_ == P_RAW) {
            // const auto& rpdu = pid.raw_patch_data_unit_;
            // rawPatchDataUnit(rpdu, ath, syntax, bitstream);
        } else if (pid.patchMode_ == P_EOM) {
            // const auto& epdu = pid.eom_patch_data_unit_;
            // eomPatchDataUnit(epdu, ath, syntax, bitstream);
        }
    } else if (ath.ath_type == I_TILE) {  // currently only use I_TILE types
        if (pid.patchMode_ == I_INTRA) {
            auto& pdu = pid.patch_data_unit_;
            read_patch_data_unit(pdu, ath, stream);
            // patchDataUnit(pdu, ath, syntax, bitstream);
        } else if (pid.patchMode_ == I_RAW) {
            const auto& rpdu = pid.raw_patch_data_unit_;
            // rawPatchDataUnit(rpdu, ath, syntax, bitstream);
        } else if (pid.patchMode_ == I_EOM) {
            const auto& epdu = pid.eom_patch_data_unit_;
            // eomPatchDataUnit(epdu, ath, syntax, bitstream);
        }
    }

    // currently only use I_TILE types
    // assert(ath.ath_type == I_TILE);
    // assert(pid.patchMode_ == I_INTRA);
    // auto& pdu = pid.patch_data_unit_;
    // read_patch_data_unit(pdu, ath, stream);
}

// 8.3.7.6  Inter patch data unit syntax
void atlas_context::read_inter_patch_data_unit(inter_patch_data_unit &ipdu, const atlas_tile_header& ath, bitstream_t* stream) {
    size_t numRefIdxActive = 0;
    if (ath.ath_type == P_TILE || ath.ath_type == SKIP_TILE) {
        if (ath.ath_num_ref_idx_active_override_flag) {
            numRefIdxActive = ath.ath_num_ref_idx_active_minus1 + 1;
        } else {
            auto& ref_list = ath.ath_ref_atlas_frame_list_asps_flag ? 
                asps_.ref_lists.at(ath.ath_ref_atlas_frame_list_idx) : ath.ref_list_struct_;
            numRefIdxActive = static_cast<size_t>( ( std::min )( static_cast<int>( ref_list.num_ref_entries ),
                                             static_cast<int>( afps_.afps_num_ref_idx_default_active_minus1 ) + 1 ) );
        }
    }
    if (numRefIdxActive > 1) {
        ipdu.ref_index      = bitstream_read_ue(stream);
    } 
    ipdu.ref_patch_index    = bitstream_read_se(stream);
    ipdu.pdu_2d_pos_x       = bitstream_read_se(stream);
    ipdu.pdu_2d_pos_y       = bitstream_read_se(stream);
    ipdu.delta_2d_size_x    = bitstream_read_se(stream);
    ipdu.delta_2d_size_y    = bitstream_read_se(stream);
    ipdu.pdu_3d_offset_u    = bitstream_read_se(stream);
    ipdu.pdu_3d_offset_v    = bitstream_read_se(stream);
    ipdu.pdu_3d_offset_d    = bitstream_read_se(stream);
    if (asps_.asps_normal_axis_max_delta_value_enabled_flag) {
        ipdu.pdu_3d_range_d = bitstream_read_se(stream);
    }

    if (asps_.asps_plr_enabled_flag) {
        atlas_tile_layer_rbsp& ad_prev = atlas_data_.at(prev_frame_index_);
        patch_information_data& pid_prev = ad_prev.atdu_.patch_information_data_.at(ipdu.ref_patch_index + pred_patch_index_);
        int32_t size_u = ipdu.delta_2d_size_x;
        int32_t size_v = ipdu.delta_2d_size_y;
        if (ad_prev.ath_.ath_type == P_TILE) {
            if (pid_prev.patchMode_ == P_MERGE) {
                printf("P_MERGE -> Not yet implemented\n");
            } else if (pid_prev.patchMode_ == P_INTER) {
                plr_data& plr_data_prev = pid_prev.inter_patch_data_unit_.plr_data_;
                size_u += plr_data_prev.block_to_patch_map_width_;
                size_v += plr_data_prev.block_to_patch_map_height_;
            } else if (pid_prev.patchMode_ == P_INTRA) {
                plr_data& plr_data_prev = pid_prev.patch_data_unit_.plr_data_;
                size_u += plr_data_prev.block_to_patch_map_width_;
                size_v += plr_data_prev.block_to_patch_map_height_;
            }
        } else if (ad_prev.ath_.ath_type == I_TILE) {
            if (pid_prev.patchMode_ == I_INTRA) {
                plr_data& plr_data_prev = pid_prev.patch_data_unit_.plr_data_;
                size_u += plr_data_prev.block_to_patch_map_width_;
                size_v += plr_data_prev.block_to_patch_map_height_;
            }
        }
        plr_data& plrd = ipdu.plr_data_;
        plrd.allocate(size_u, size_v);
        read_plr_data(plrd, stream);
        //plrData( plrd, syntax, asps, bitstream );
        prev_patch_size_u_ = size_u;
        prev_patch_size_v_ = size_v;
        pred_patch_index_ += ipdu.ref_patch_index + 1;
    }
}

void atlas_context::read_patch_data_unit(patch_data_unit &pdu, const atlas_tile_header& ath, bitstream_t* stream) {
    pdu.pdu_2d_pos_x = bitstream_read_ue(stream);
    pdu.pdu_2d_pos_y = bitstream_read_ue(stream);
    pdu.pdu_2d_size_x_minus1 = bitstream_read_ue(stream);
    pdu.pdu_2d_size_y_minus1 = bitstream_read_ue(stream);
    pdu.pdu_3d_offset_u = bitstream_read(stream, asps_.asps_geometry_3d_bit_depth_minus1 + 1);
    pdu.pdu_3d_offset_v = bitstream_read(stream, asps_.asps_geometry_3d_bit_depth_minus1 + 1);
    pdu.pdu_3d_offset_d = bitstream_read(stream, asps_.asps_geometry_3d_bit_depth_minus1 - ath.ath_pos_min_d_quantizer + 1);

    if (asps_.asps_normal_axis_max_delta_value_enabled_flag) {
        const uint32_t rangeDBitDepth = std::min(asps_.asps_geometry_2d_bit_depth_minus1, asps_.asps_geometry_3d_bit_depth_minus1) + 1;
        pdu.pdu_3d_range_d = bitstream_read(stream, rangeDBitDepth - ath.ath_pos_delta_max_d_quantizer);
    } 
    // pdu.pdu_projection_id = bitstream_read(stream, ceil(log2(6))); asps_.asps_max_number_projections_minus1
    pdu.pdu_projection_id = bitstream_read(stream, ceilLog2(asps_.asps_max_number_projections_minus1 + 1));
    pdu.pdu_orientation_index = bitstream_read(stream, asps_.asps_use_eight_orientations_flag ? 3 : 1);

    if (afps_.afps_lod_mode_enabled_flag) {
        pdu.pdu_lod_enabled_flag = bitstream_read(stream, 1) != 0U;
        if (pdu.pdu_lod_enabled_flag) {
            pdu.pdu_lod_scale_x_minus1 = uint8_t(bitstream_read_ue(stream));
            pdu.pdu_lod_scale_y_idc = uint8_t(bitstream_read_ue(stream));
        }
    }
    if (asps_.asps_plr_enabled_flag) {
        plr_data& plrd = pdu.plr_data_;
        plrd.allocate(pdu.pdu_2d_size_x_minus1 + 1, pdu.pdu_2d_size_y_minus1 + 1);
        read_plr_data(plrd, stream);
    }
}

void atlas_context::read_atlas_tile_layer_rbsp(atlas_tile_layer_rbsp& rbsp, NAL_UNIT_TYPE nalu_t, bitstream_t* stream) {
    read_atlas_tile_header(rbsp.ath_, nalu_t, stream);
    read_atlas_tile_data_unit(rbsp.atdu_, rbsp.ath_, stream);
    bitstream_align_rbsp_trailing_bits(stream);
}

void atlas_context::read_nal_hdr(uint8_t& nal_type, uint8_t& nal_layer_id, uint8_t& nal_temporal_id_plus1, bitstream_t* stream) {
    bitstream_advance(stream, 1); // bitstream_read(stream, 1);
    nal_type = bitstream_read(stream, 6);
    nal_layer_id = bitstream_read(stream, 6);
    nal_temporal_id_plus1 = bitstream_read(stream, 3);
}

void atlas_context::read_atlas_sub_bitstream(const size_t& v3c_unit_payload_size, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, bitstream_t* stream) {
    const size_t length = stream->len + v3c_unit_payload_size;
    ad_nal_precision_ = bitstream_read(stream, 3) + 1;
    const uint32_t nal_precision_in_bits = ad_nal_precision_ * 8;

    bitstream_advance(stream, 5); 

    pccsei sei;

    size_t frameId = 0;
    while (stream->len < stream->data.size()) {
        // if (stream->len >= length) {
        //     break;
        // }
        
        const uint32_t nal_unit_size = bitstream_read(stream, nal_precision_in_bits);
        // printf("NAL unit size: %d, GOF ID: %d\n", nal_unit_size, (int)gofUVG->gofId);

        bitstream_t stream_tmp;
        stream_tmp.data.resize(stream_tmp.len + nal_unit_size);
        // stream_tmp->data.insert(stream_tmp->data.begin(), stream->data.data(), stream->data.data() + stream->len);
        memcpy( stream_tmp.data.data() + stream_tmp.len, stream->data.data() + stream->len, nal_unit_size );
        stream->len += nal_unit_size;

        // nalUnitHeader
        bitstream_advance(&stream_tmp, 1); // bitstream_read(&stream_tmp, 1);
        const NAL_UNIT_TYPE nal_unit_type = static_cast<NAL_UNIT_TYPE>(bitstream_read(&stream_tmp, 6));
        const uint8_t nal_layer_id = bitstream_read(&stream_tmp, 6);
        const uint8_t nal_temporal_id_plus1 = bitstream_read(&stream_tmp, 3);

        // printf("main bitstream bytes = %d, bits = %d\n", stream->len, stream->cur_bit);
        // printf("Tmp bitstream  bytes = %d, bits = %d\n", stream_tmp.len, stream_tmp.cur_bit);
        switch (nal_unit_type) {
            case NAL_ASPS: read_atlas_seq_parameter_set(&stream_tmp); break;
            case NAL_AFPS: read_atlas_frame_parameter_set(&stream_tmp); break;
            case NAL_TRAIL_N:
            case NAL_TRAIL_R:
            case NAL_TSA_N:
            case NAL_TSA_R:
            case NAL_STSA_N:
            case NAL_STSA_R:
            case NAL_RADL_N:
            case NAL_RADL_R:
            case NAL_RASL_N:
            case NAL_RASL_R:
            case NAL_SKIP_N:
            case NAL_SKIP_R:
            case NAL_IDR_N_LP: {
                atlas_tile_layer_rbsp rbsp;
                rbsp.sei_.sei_prefix = sei.sei_prefix;
                rbsp.atdu_.tile_order_ = atlas_data_.size();
                read_atlas_tile_layer_rbsp(rbsp, nal_unit_type, &stream_tmp);
                atlas_data_.push_back(rbsp);
                write_atlas_tile_layer_rbsp_to_gof(rbsp, gofUVG);
                gofUVG->frames.back()->frameId = frameId++;
                // printf("Atlas Tile Layer NAL units\n");
            } break;
            case NAL_PREFIX_ESEI:
            case NAL_PREFIX_NSEI: read_prefix_sei_rbsp(&stream_tmp, nal_unit_type, sei); break;
            case NAL_SUFFIX_ESEI:
            case NAL_SUFFIX_NSEI:
                read_suffix_sei_rbsp(&stream_tmp, nal_unit_type, atlas_data_.back().sei_);
                break;
            default:
                // printf("NAL not supported\n");
                break;
        }
        // printf("main bitstream bytes = %d, bits = %d\n", stream->len, stream->cur_bit);
        // printf("Tmp bitstream  bytes = %d, bits = %d\n", stream_tmp.len, stream_tmp.cur_bit);
    }
    
}