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

void atlas_context::write_atlas_tile_layer_rbsp_to_gof(const atlas_tile_layer_rbsp& rbsp, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG) {
    gofUVG->mapHeightGOF = asps_.asps_frame_height;
    gofUVG->gofId = gof_id_;
    std::shared_ptr<uvgvpcc_dec::Frame> frameUVG = std::make_shared<uvgvpcc_dec::Frame>();

    //printf("Writing atlas tile layer rbsp to gofUVG, gofId: %zu\n", gof_id_);

    const size_t pid_count = rbsp.atdu_.patch_information_data_.size();
    const size_t minLevel = static_cast<size_t>(pow(2., rbsp.ath_.ath_pos_min_d_quantizer));

    // printf("  Number of patches: %zu\n", pid_count);
    for (size_t patch_index = 0; patch_index < pid_count; ++patch_index) {
        uvgvpcc_dec::Patch patchUVG;
        // const patch_data_unit& pdu = rbsp.atdu_.patch_information_data_.at(patch_index).patch_data_unit_;
        const patch_data_unit& pdu = rbsp.atdu_.patch_information_data_.at(patch_index).patch_data_unit_;

        patchUVG.occupancy_resolution = size_t(1) << asps_.asps_log2_patch_packing_block_size;
        patchUVG.omDSPosX_ = pdu.pdu_2d_pos_x;
        patchUVG.omDSPosY_ = pdu.pdu_2d_pos_y;
        patchUVG.widthInOccBlk_ = pdu.pdu_2d_size_x_minus1 + 1;
        patchUVG.heightInOccBlk_ = pdu.pdu_2d_size_y_minus1 + 1;
        patchUVG.posU_ = pdu.pdu_3d_offset_u;
        patchUVG.posV_ = pdu.pdu_3d_offset_v;
        patchUVG.posD_ = pdu.pdu_3d_offset_d * minLevel;
        
        patchUVG.sizeD_ = pdu.pdu_3d_range_d == 0 ? 0 : (pdu.pdu_3d_range_d * minLevel - 1);
        
        patchUVG.orientationIndex = pdu.pdu_orientation_index;
        patchUVG.setPatchPpiAndAxis(pdu.pdu_projection_id);

        // if (pdu.pdu_lod_enabled_flag) {
        //     ...
        // }

        frameUVG->patchList.push_back(patchUVG);
    }
    frameUVG->mapHeight = gofUVG->mapHeightGOF;
    gofUVG->frames.push_back(frameUVG);
}

void atlas_context::read_atlas_seq_parameter_set(bitstream_t* stream) {
    asps_.asps_atlas_sequence_parameter_set_id = readUE(stream, "asps_atlas_sequence_parameter_set_id",get_gof_id());
    asps_.asps_frame_width = readUE(stream, "asps_frame_width",get_gof_id());
    asps_.asps_frame_height = readUE(stream, "asps_frame_height",get_gof_id());
    asps_.asps_geometry_3d_bit_depth_minus1 = readU(stream, 5, "asps_geometry_3d_bit_depth_minus1",get_gof_id());
    asps_.asps_geometry_2d_bit_depth_minus1 = readU(stream, 5, "asps_geometry_2d_bit_depth_minus1",get_gof_id());
    asps_.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 = readUE(stream, "asps_log2_max_atlas_frame_order_cnt_lsb_minus4",get_gof_id());
    asps_.asps_max_dec_atlas_frame_buffering_minus1 = readUE(stream, "asps_max_dec_atlas_frame_buffering_minus1",get_gof_id());

    asps_.asps_long_term_ref_atlas_frames_flag = readU(stream, 1, "asps_long_term_ref_atlas_frames_flag",get_gof_id());
    asps_.asps_num_ref_atlas_frame_lists_in_asps = readUE(stream, "asps_num_ref_atlas_frame_lists_in_asps",get_gof_id());
    asps_.ref_lists.resize(asps_.asps_num_ref_atlas_frame_lists_in_asps);

    for (size_t i = 0; i < asps_.asps_num_ref_atlas_frame_lists_in_asps; i++) {
        ref_list_struct& ref = asps_.ref_lists.at(i);
        const uint8_t num_ref_entries = readUE(stream, "num_ref_entries",get_gof_id());
        ref.num_ref_entries = num_ref_entries;
        
        ref.st_ref_atlas_frame_flag.resize(num_ref_entries);
        ref.abs_delta_afoc_st.resize(num_ref_entries);
        ref.straf_entry_sign_flag.resize(num_ref_entries);
        ref.afoc_lsb_lt.resize(num_ref_entries);

        for (size_t i = 0; i < ref.num_ref_entries; ++i) {
            if (asps_.asps_long_term_ref_atlas_frames_flag) {
                ref.st_ref_atlas_frame_flag.at(i) = readU(stream, 1, "st_ref_atlas_frame_flag",get_gof_id());
            }
            if (ref.st_ref_atlas_frame_flag.at(i)) {
                ref.abs_delta_afoc_st.at(i) = readUE(stream, "abs_delta_afoc_st",get_gof_id());
                if (ref.abs_delta_afoc_st.at(i) > 0) {
                    ref.straf_entry_sign_flag.at(i) = readU(stream, 1, "straf_entry_sign_flag",get_gof_id());
                }
            }
            else {
                ref.afoc_lsb_lt.at(i) = readU(stream, asps_.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 + 4, "afoc_lsb_lt",get_gof_id());
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
        asps_.asps_map_pixel_deinterleaving_flag.resize(asps_.asps_map_count_minus1 + 1);
        for (size_t j = 0; j < asps_.asps_map_count_minus1; ++j) {
            asps_.asps_map_pixel_deinterleaving_flag.at(j) = readU(stream, 1, "asps_map_pixel_deinterleaving_flag",get_gof_id());
        }
    }
    asps_.asps_raw_patch_enabled_flag = readU(stream, 1, "asps_raw_patch_enabled_flag",get_gof_id());
    asps_.asps_eom_patch_enabled_flag = readU(stream, 1, "asps_eom_patch_enabled_flag",get_gof_id());

    if (asps_.asps_eom_patch_enabled_flag && asps_.asps_map_count_minus1 == 0) {
        asps_.asps_eom_fix_bit_count_minus1 = readU(stream, 4, "asps_eom_fix_bit_count_minus1",get_gof_id());
    }
    if (asps_.asps_raw_patch_enabled_flag || asps_.asps_eom_patch_enabled_flag) {
        asps_.asps_auxiliary_video_enabled_flag = readU(stream, 4, "asps_auxiliary_video_enabled_flag",get_gof_id());
    }

    asps_.asps_plr_enabled_flag = readU(stream, 1, "asps_plr_enabled_flag",get_gof_id());
    asps_.asps_vui_parameters_present_flag = readU(stream, 1, "asps_vui_parameters_present_flag",get_gof_id());
    
    asps_.asps_extension_present_flag = readU(stream, 1, "asps_extension_present_flag",get_gof_id());
    if (asps_.asps_extension_present_flag) {
        asps_.asps_vpcc_extension_present_flag = readU(stream, 1, "asps_vpcc_extension_present_flag",get_gof_id());
        asps_.asps_miv_extension_present_flag = readU(stream, 1, "asps_miv_extension_present_flag",get_gof_id());
        asps_.asps_extension_6bits = readU(stream, 6, "asps_extension_6bits",get_gof_id());
    }
    if (asps_.asps_vpcc_extension_present_flag) {
        asps_.asps_vpcc_remove_duplicate_point_enabled_flag = readU(stream, 1, "asps_vpcc_remove_duplicate_point_enabled_flag",get_gof_id());
        if (asps_.asps_pixel_deinterleaving_enabled_flag || asps_.asps_plr_enabled_flag) {
            asps_.asps_vpcc_surface_thickness_minus1 = readUE(stream, "asps_vpcc_surface_thickness_minus1",get_gof_id());
        }
    }
    bitstream_align(stream);
}

void atlas_context::read_atlas_frame_parameter_set(bitstream_t* stream) {
    afps_.afps_atlas_frame_parameter_set_id = readUE(stream, "afps_atlas_frame_parameter_set_id",get_gof_id());
    afps_.afps_atlas_sequence_parameter_set_id = readUE(stream, "afps_atlas_sequence_parameter_set_id",get_gof_id());
    afps_.afti.afti_single_tile_in_atlas_frame_flag = readU(stream, 1, "afti_single_tile_in_atlas_frame_flag",get_gof_id());
    // parts of atlas frame tile information not read for now, as it it not used
    afps_.afti.afti_signalled_tile_id_flag = readU(stream, 1, "afti_signalled_tile_id_flag",get_gof_id());
    afps_.afps_output_flag_present_flag = readU(stream, 1, "afps_output_flag_present_flag",get_gof_id());
    afps_.afps_num_ref_idx_default_active_minus1 = readUE(stream, "afps_num_ref_idx_default_active_minus1",get_gof_id());
    afps_.afps_additional_lt_afoc_lsb_len = readUE(stream, "afps_additional_lt_afoc_lsb_len",get_gof_id());
    afps_.afps_lod_mode_enabled_flag = readU(stream, 1, "afps_lod_mode_enabled_flag",get_gof_id());
    afps_.afps_raw_3d_offset_bit_count_explicit_mode_flag = readU(stream, 1, "afps_raw_3d_offset_bit_count_explicit_mode_flag",get_gof_id());
    afps_.afps_extension_present_flag = readU(stream, 1, "afps_extension_present_flag",get_gof_id());
    afps_.afps_miv_extension_present_flag = readU(stream, 1, "afps_miv_extension_present_flag",get_gof_id());
    afps_.afps_extension_7bits = readU(stream, 7, "afps_extension_7bits",get_gof_id());
    bitstream_align(stream);
}

void atlas_context::read_atlas_tile_header(atlas_tile_header &ath, NAL_UNIT_TYPE nalu_t, bitstream_t* stream) {
    if (nalu_t >= NAL_GBLA_W_LP && nalu_t <= NAL_RSV_IRAP_ACL_29) {
        ath.ath_no_output_of_prior_atlas_frames_flag = readU(stream, 1, "ath_no_output_of_prior_atlas_frames_flag", get_gof_id());
    }
    ath.ath_atlas_frame_parameter_set_id = readUE(stream, "ath_atlas_frame_parameter_set_id", get_gof_id());
    ath.ath_atlas_adaptation_parameter_set_id = readUE(stream, "ath_atlas_adaptation_parameter_set_id", get_gof_id());
    //ath.ath_id = readU(stream, ?,"ath_id", get_gof_id()); // TODO(lf): Dynamic bit length
    // const uint16_t tileID = ath.ath_id; 
    ath.ath_type = static_cast<ATH_TYPE> (readUE(stream, "ath_type", get_gof_id()));
    if (afps_.afps_output_flag_present_flag) {
        ath.ath_atlas_output_flag = readU(stream, 1, "ath_atlas_output_flag", get_gof_id());
    }
    const uint8_t Log2MaxAtlasFrmOrderCntLsb = asps_.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 + 4;
    ath.ath_atlas_frm_order_cnt_lsb = readU(stream, Log2MaxAtlasFrmOrderCntLsb, "ath_atlas_frm_order_cnt_lsb", get_gof_id()); // u(v)

    if (asps_.asps_num_ref_atlas_frame_lists_in_asps > 0) {
        ath.ath_ref_atlas_frame_list_asps_flag = readU(stream, 1, "ath_ref_atlas_frame_list_asps_flag", get_gof_id());
    }
    if (ath.ath_ref_atlas_frame_list_asps_flag == 0) {
        // std::cout << "ERROR: NOT IMPLEMENTED" << std::endl;
        throw std::runtime_error("Using atlas ref frame lists not implemented");
        return;
    } else if (asps_.asps_num_ref_atlas_frame_lists_in_asps > 1) {
        const uint8_t bit_len = std::ceil(std::log2(asps_.asps_num_ref_atlas_frame_lists_in_asps));
        ath.ath_ref_atlas_frame_list_idx = readU(stream, bit_len, "ath_ref_atlas_frame_list_idx", get_gof_id());
    }
    const size_t NumLtrAtlasFrmEntries = 0;  // default value, ref list is from ASPS TODO(lf): dynamic
    ath.ath_additional_afoc_lsb_present_flag.resize(NumLtrAtlasFrmEntries);
    ath.ath_additional_afoc_lsb_val.resize(NumLtrAtlasFrmEntries);
    for (size_t j = 0; j < NumLtrAtlasFrmEntries; j++) {
        ath.ath_additional_afoc_lsb_present_flag.at(j) = readU(stream, 1, "ath_additional_afoc_lsb_present_flag", get_gof_id());
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
            const uint8_t bit_len = std::floor(std::log2(asps_.asps_geometry_3d_bit_depth_minus1 + 1));
            ath.ath_raw_3d_offset_axis_bit_count_minus1 = readU(stream, bit_len, "ath_raw_3d_offset_axis_bit_count_minus1",get_gof_id());
        }
        if (ath.ath_type == ATH_TYPE::P_TILE && NumLtrAtlasFrmEntries > 1) {
            ath.ath_num_ref_idx_active_override_flag = readU(stream, 1, "ath_num_ref_idx_active_override_flag",get_gof_id());
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
        while(true) {
            atdu.atdu_patch_mode = readUE(stream, "atdu_patch_mode", get_gof_id());
            if (atdu.atdu_patch_mode == ATDU_PATCH_MODE_I_TILE::I_END
                || atdu.atdu_patch_mode == ATDU_PATCH_MODE_P_TILE::P_END) {
                break;
            }
            patch_information_data pid;
            pid.patchMode = atdu.atdu_patch_mode;
            read_patch_information_data(pid, ath, stream);
            atdu.patch_information_data_.push_back(pid);
        }

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

    // currently only use I_TILE types
    assert(ath.ath_type == I_TILE);
    assert(pid.patchMode == I_INTRA);
    auto& pdu = pid.patch_data_unit_;
    read_patch_data_unit(pdu, ath, stream);
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
    pdu.pdu_projection_id = bitstream_read(stream, ceil(log2(6)));
    pdu.pdu_orientation_index = bitstream_read(stream, false ? 3 : 1);

    if (afps_.afps_lod_mode_enabled_flag) {
        pdu.pdu_lod_enabled_flag = bitstream_read(stream, 1);
        if (pdu.pdu_lod_enabled_flag) {
            pdu.pdu_lod_scale_x_minus1 = bitstream_read_ue(stream);
            pdu.pdu_lod_scale_y_idc = bitstream_read_ue(stream);
        }
    }
    // if( asps_plr_enabled_flag )               == false
    // if( asps_miv_extension_present_flag )     == false
}

void atlas_context::read_atlas_tile_layer_rbsp(atlas_tile_layer_rbsp& rbsp, NAL_UNIT_TYPE nalu_t, bitstream_t* stream) {
    read_atlas_tile_header(rbsp.ath_, nalu_t, stream);
    read_atlas_tile_data_unit(rbsp.atdu_, rbsp.ath_, stream);
    bitstream_align_rbsp_trailing_bits(stream);
}

void atlas_context::read_nal_hdr(uint8_t& nal_type, uint8_t& nal_layer_id, uint8_t& nal_temporal_id_plus1, bitstream_t* stream) {
    bitstream_read(stream, 1);
    nal_type = bitstream_read(stream, 6);
    nal_layer_id = bitstream_read(stream, 6);
    nal_temporal_id_plus1 = bitstream_read(stream, 3);
}

void atlas_context::read_atlas_sub_bitstream(const size_t& v3c_unit_payload_size, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, bitstream_t* stream) {
    // uint8_t nal_type;
    // uint8_t nal_layer_id;
    // uint8_t nal_temporal_id_plus1;

    // // Atlas NAL sample stream header
    // ad_nal_precision_ = bitstream_read(stream, 3) + 1;
    // bitstream_advance(stream, 5); 
    // const uint32_t nal_precision_in_bits = ad_nal_precision_ * 8;
    // //printf("NAL precision in bits: %u\n", nal_precision_in_bits);

    // // ASPS NALU
    // ad_nal_sizes_.push_back(bitstream_read(stream, nal_precision_in_bits));
    // //printf("ASPS NAL size: %d\n", (int)ad_nal_sizes_.back());
    // /*
    //     nal_type = NAL_ASPS
    //     nal_layer_id = 0
    //     nal_temporal_id_plus1 = 1
    // */
    // read_nal_hdr(nal_type, nal_layer_id, nal_temporal_id_plus1, stream); 
    // //printf("NAL type: %d\n", nal_type);
    // read_atlas_seq_parameter_set(stream);

    // // AFPS NAL unit
    // ad_nal_sizes_.push_back(bitstream_read(stream, nal_precision_in_bits));
    // //printf("AFPS NAL size: %d\n", (int)ad_nal_sizes_.back());
    // /*
    //     nal_type = NAL_AFPS
    //     nal_layer_id = 0
    //     nal_temporal_id_plus1 = 1
    // */
    // read_nal_hdr(nal_type, nal_layer_id, nal_temporal_id_plus1, stream); 
    // //printf("NAL type: %d\n", nal_type);
    // read_atlas_frame_parameter_set(stream);

    // size_t frameId = gofUVG->gofId * gofUVG->gofCount;
    // while (stream->len < v3c_unit_payload_size) {
    //     ad_nal_sizes_.push_back(bitstream_read(stream, nal_precision_in_bits));
    //     //printf("NAL_IDR_N_LP NAL size: %d\n", (int)ad_nal_sizes_.back());
    //     /*
    //         nal_type = NAL_IDR_N_LP
    //         nal_layer_id = 0
    //         nal_temporal_id_plus1 = 1
    //     */
    //     read_nal_hdr(nal_type, nal_layer_id, nal_temporal_id_plus1, stream);        // TODO(lf): Dynamic NALU type
    //     //printf("NAL type: %d\n", nal_type);
    //     atlas_tile_layer_rbsp rbsp;
    //     read_atlas_tile_layer_rbsp(rbsp, static_cast<NAL_UNIT_TYPE>(nal_type), stream);
    //     atlas_data_.push_back(rbsp);
    //     //printf("Done reading ATDL RBSP\n");
    //     write_atlas_tile_layer_rbsp_to_gof(rbsp, gofUVG);
    //     gofUVG->frames.back()->frameId = frameId++;
    //     //printf("stream len: %zu, v3c_unit_payload_size: %zu\n", stream->len, v3c_unit_payload_size);
    // }

    // bitstream_advance(stream, nal_precision_in_bits);
    // /*
    //     nal_type = NAL_EOB
    //     nal_layer_id = 0
    //     nal_temporal_id_plus1 = 1
    // */
    // read_nal_hdr(nal_type, nal_layer_id, nal_temporal_id_plus1, stream);
    // // No payload in end of bitstream NAL unit



    const size_t length = stream->len + v3c_unit_payload_size;
    ad_nal_precision_ = bitstream_read(stream, 3) + 1;
    const uint32_t nal_precision_in_bits = ad_nal_precision_ * 8;

    bitstream_advance(stream, 5); 

    size_t frameId = gofUVG->gofId * (gofUVG->gofCount + (gofUVG->gofCount & 1));
    while (true) {
        if (stream->len >= length) {
            break;
        }
        
        const size_t nal_unit_size = bitstream_read(stream, nal_precision_in_bits);

        bitstream_read(stream, 1);
        const NAL_UNIT_TYPE nal_unit_type = static_cast<NAL_UNIT_TYPE>(bitstream_read(stream, 6));
        const uint8_t nal_layer_id = bitstream_read(stream, 6);
        const uint8_t nal_temporal_id_plus1 = bitstream_read(stream, 3);

        if (nal_unit_type == NAL_ASPS) {
            read_atlas_seq_parameter_set(stream);
        } else if (nal_unit_type == NAL_AFPS) {
            read_atlas_frame_parameter_set(stream);
        } else if (nal_unit_type == NAL_IDR_N_LP) {
            atlas_tile_layer_rbsp rbsp;
            read_atlas_tile_layer_rbsp(rbsp, nal_unit_type, stream);
            atlas_data_.push_back(rbsp);
            write_atlas_tile_layer_rbsp_to_gof(rbsp, gofUVG);
            gofUVG->frames.back()->frameId = frameId++;
        } 
        // else if (nal_unit_type == NAL_EOB) {

        // } else if (nal_unit_type == NAL_TRAIL_N) {

        // } else if (nal_unit_type == NAL_TRAIL_R) {

        // } else if (nal_unit_type == NAL_PREFIX_NSEI) {
        //     //seiRbsp( ptr, nal_unit_type, prefixSEITemp );  .
        // } else if (nal_unit_type == NAL_PREFIX_ESEI) {

        // } else if (nal_unit_type == NAL_RSV_ACL_32) {

        // } else {

        // }
    }
    
}