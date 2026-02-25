#pragma once

/// \file Handle atlas information based on TMC2 implementation.

#include "atlas_frame.hpp"
#include "bitstream_common.hpp"
#include "bitstream_util.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

/* Atlas context is used to hold the atlas data (inside V3C_AD unit) of a single GOF */
class atlas_context {
    public:
    atlas_context() : asps_(), afps_(), atlas_data_(), gof_id_(), atlas_sub_size_(0), ad_nal_sizes_({}), ad_nal_precision_(0) {};

    // Read atlas sub bitstream
    void read_atlas_sub_bitstream(const size_t& v3c_unit_payload_size, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, bitstream_t* stream);

    // -------------- Getters - data structures --------------
    std::vector<atlas_tile_layer_rbsp>& get_atlases() { return atlas_data_; };
    atlas_sequence_parameter_set& get_asps() { return asps_; };
    atlas_frame_parameter_set& get_afps() { return afps_; };

    // -------------- Getters - helper variables --------------
    size_t get_gof_id() const { return gof_id_; };
    size_t get_atlas_sub_size() { return atlas_sub_size_; };
    std::vector<size_t> get_ad_nal_sizes() { return ad_nal_sizes_; };
    size_t get_ad_nal_precision() { return ad_nal_precision_; };

    // -------------- Setters - helper variables --------------
    void set_gof_id(size_t gof_id) { gof_id_ = gof_id; };
    void set_atlas_sub_size(size_t atlas_sub_size) { atlas_sub_size_ = atlas_sub_size; };

    private:
    /* Fill data structures with encoded data to gofUVG */
    void write_atlas_tile_layer_rbsp_to_gof(const atlas_tile_layer_rbsp& rbsp, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG);

    // -------------- Functions to read data structures from bitstream --------------
    void read_nal_hdr(uint8_t& nal_type, uint8_t& nal_layer_id, uint8_t& nal_temporal_id_plus1, bitstream_t* stream);
    void read_atlas_seq_parameter_set(bitstream_t* stream);
    void read_atlas_frame_parameter_set(bitstream_t* stream);
    void read_atlas_tile_header(atlas_tile_header& ath, NAL_UNIT_TYPE nalu_t, bitstream_t* stream);
    void read_atlas_tile_data_unit(atlas_tile_data_unit& atdu, const atlas_tile_header& ath, bitstream_t* stream);
    void read_atlas_tile_layer_rbsp(atlas_tile_layer_rbsp& rbsp, NAL_UNIT_TYPE nalu_t, bitstream_t* stream);

    void read_patch_information_data(patch_information_data& pid, const atlas_tile_header& ath, bitstream_t* stream);
    void read_patch_data_unit(patch_data_unit &pdu, const atlas_tile_header& ath, bitstream_t* stream);


    /* -------------- Atlas data structures -------------- */
    atlas_sequence_parameter_set asps_;
    atlas_frame_parameter_set afps_;
    std::vector<atlas_tile_layer_rbsp> atlas_data_;

    /* -------------- Helper variables -------------- */
    size_t gof_id_;
    size_t atlas_sub_size_;
    std::vector<size_t> ad_nal_sizes_;
    size_t ad_nal_precision_;
};

