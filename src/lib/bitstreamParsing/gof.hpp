#pragma once
#include <memory>
#include <vector>

#include "atlas_context.hpp"
#include "uvgvpcc/uvgvpcc.hpp"
#include "video_sub_bitstream.hpp"
#include "vps.hpp"

/* Statistics related to a V3C Group Of Frames (GOF) */
class v3c_gof {
   public:
    v3c_gof() {
        v3c_unit_precision_ = 0;
        gof_id_ = 0;
        n_frames_ = 0;
        v3c_vps_sub_ = std::make_unique<vps>();
        v3c_ad_unit_ = std::make_unique<atlas_context>();
        // v3c_ovd_sub_ = std::make_unique<std::vector<uint8_t>>();
        // v3c_gvd_sub_ = std::make_unique<std::vector<uint8_t>>();
        // v3c_avd_sub_ = std::make_unique<std::vector<uint8_t>>();
    };
    v3c_gof(size_t id) {
        v3c_unit_precision_ = 0;
        gof_id_ = id;
        n_frames_ = 0;
        v3c_vps_sub_ = std::make_unique<vps>();
        v3c_ad_unit_ = std::make_unique<atlas_context>();
        // v3c_ovd_sub_ = std::make_unique<std::vector<uint8_t>>();
        // v3c_gvd_sub_ = std::make_unique<std::vector<uint8_t>>();
        // v3c_avd_sub_ = std::make_unique<std::vector<uint8_t>>();
    };

    // Used for LD mode
    void set_n_frames(size_t value) { n_frames_ = value; };

    size_t get_n_frames() { return n_frames_; };

    // Set the V3C unit precision. If smaller than before, do nothing
    void set_v3c_unit_precision(uint32_t new_precision) {
        if (new_precision > v3c_unit_precision_) {
            v3c_unit_precision_ = new_precision;
        }
    };

    /* Add new data to the most recent GOF */
    void add_v3c_vps(std::unique_ptr<vps> data) { v3c_vps_sub_ = std::move(data); };
    void add_v3c_atlas_context(std::unique_ptr<atlas_context> data) { v3c_ad_unit_ = std::move(data); };
    void add_v3c_ovd_sub(std::unique_ptr<std::vector<uint8_t>> data) { v3c_ovd_sub_ = std::move(data); };
    void add_v3c_gvd_sub(std::unique_ptr<std::vector<uint8_t>> data) { v3c_gvd_sub_ = std::move(data); };
    void add_v3c_avd_sub(std::unique_ptr<std::vector<uint8_t>> data) { v3c_avd_sub_ = std::move(data); };

    /* Get data from the most recent GOF */
    vps* get_v3c_vps() { return v3c_vps_sub_.get(); };
    atlas_context* get_v3c_atlas_context() { return v3c_ad_unit_.get(); };
    std::vector<uint8_t>* get_v3c_ovd_sub() { return v3c_ovd_sub_.get(); };
    std::vector<uint8_t>* get_v3c_gvd_sub() { return v3c_gvd_sub_.get(); };
    std::vector<uint8_t>* get_v3c_avd_sub() { return v3c_avd_sub_.get(); };
    size_t get_gof_id() {return gof_id_;};

    /* Write the latest GOF to a single V3C unit stream buffer, with parsing information given separately */
    void read_v3c_chunk(uvgvpcc_dec::API::v3c_chunk& chunk, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG);
    void read_v3c_chunk_parallel(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG);
    void read_v3c_chunk_separate_vuh_units(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG);

    /* Write the latest GOF in Low Delay (LD) mode to a single V3C unit stream buffer, with parsing information given separately */
    void read_v3c_ld_chunk(const std::vector<nal_info> &ovd_nals, const std::vector<nal_info> &gvd_nals,
                            const std::vector<nal_info> &avd_nals, uvgvpcc_dec::API::v3c_unit_stream *out, bool double_layer);

    private:
    size_t gof_id_;
    size_t v3c_unit_precision_;
    size_t n_frames_;                               // N of frames in GOF, used for low-delay mode
    std::unique_ptr<vps> v3c_vps_sub_;                   // no V3C header
    std::unique_ptr<atlas_context> v3c_ad_unit_;         // incl. V3C header
    std::unique_ptr<std::vector<uint8_t>> v3c_ovd_sub_;  // no V3C header
    std::unique_ptr<std::vector<uint8_t>> v3c_gvd_sub_;  // no V3C header
    std::unique_ptr<std::vector<uint8_t>> v3c_avd_sub_;  // no V3C header
};