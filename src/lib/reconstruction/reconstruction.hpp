#pragma once

/// \file Entry point for the point cloud reconstruction process.

#include "bitstreamParsing/gof.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

struct common_reconstruction_parameters {
  size_t occupancyPrecision = 0;

  size_t num_tiles_in_atlas_frame = 0;
  size_t tile_width  = 0;
  size_t tile_height = 0;

  size_t left_top_x_in_frame = 0;
  size_t left_top_y_in_frame = 0;
  
  size_t patch_packing_block_size = 0;
  size_t blockToPatchWidth  = 0;
  size_t blockToPatchHeight = 0;
  size_t blockCount = 0;

  size_t layer_count = 0;

  bool asps_patch_precedence_order_flag = false;
  bool multipleStreams_ = false;
  bool absoluteD1_ = false;
  
  size_t geo_map_width    = 0;
  size_t geo_bit_depth_3d = 0;

  size_t attribute_map_width = 0;
  size_t offsetU = 0;
  size_t offsetV = 0;
};

struct other_reconstruction_parameters {
  size_t surface_thickness           = 0;
  size_t threshold_lossy_om          = 0;
  bool enable_size_quantization      = false;
  bool remove_duplicate_points       = false;
  bool point_local_reconstruction    = false;
  bool single_map_pixel_interleaving = false;
  bool use_additional_points_patch   = false;
  bool use_aux_seperate_video        = false;
  bool enhanced_occupancy_map_code   = false;
  bool EOM_fix_bit_count             = false;
};

struct sei_reconstruction_info {
  bool    geometry_smoothing_flag = false;
  bool    grid_smoothing          = false;
  size_t  grid_size               = 0;
  double  threshold_smoothing     = 0;

  bool    pbf_enable_flag         = false;
  int16_t pbf_passes_count        = 0;
  int16_t pbf_filter_size         = 0;
  int16_t pbf_log2_threshold      = 0;

  size_t  c_grid_size = 0;
  bool    color_smoothing_flag = 0;
  double  threshold_color_smoothing = 0;
  double  threshold_color_difference = 0;
  double  threshold_color_variation = 0;
};

struct reconstruction_options {
  // reconstruction options
  int pixelDeinterleavingType_; 
  int pointLocalReconstructionType_;
  int reconstructEomType_;
  int duplicatedPointRemovalType_;
  int reconstructRawType_;
  int applyGeoSmoothingType_;
  int applyAttrSmoothingType_;
  int attrTransferFilterType_;
  int applyOccupanySynthesisType_;

  void setReconstructionParameters(size_t profileReconstructionIdc) {
    if ( profileReconstructionIdc == 0 ) {
      pixelDeinterleavingType_      = 0;
      pointLocalReconstructionType_ = 0;
      reconstructEomType_           = 0;
      duplicatedPointRemovalType_   = 0;
      reconstructRawType_           = 0;
      applyGeoSmoothingType_        = 0;
      applyAttrSmoothingType_       = 0;
      attrTransferFilterType_       = 0;
      applyOccupanySynthesisType_   = 0;
    } else if ( profileReconstructionIdc == 1 ) {
      pixelDeinterleavingType_      = 1;
      pointLocalReconstructionType_ = 1;
      reconstructEomType_           = 1;
      duplicatedPointRemovalType_   = 1;
      reconstructRawType_           = 1;
      applyGeoSmoothingType_        = 1;
      applyAttrSmoothingType_       = 1;
      attrTransferFilterType_       = 1;
      applyOccupanySynthesisType_   = 0;
    } else if ( profileReconstructionIdc == 2 ) {
      pixelDeinterleavingType_      = 1;
      pointLocalReconstructionType_ = 1;
      reconstructEomType_           = 1;
      duplicatedPointRemovalType_   = 1;
      reconstructRawType_           = 1;
      applyGeoSmoothingType_        = 0;
      applyAttrSmoothingType_       = 1;
      attrTransferFilterType_       = 0;
      applyOccupanySynthesisType_   = 1;
    }
    printf( "---profileReconstructionIdc(%zu)-------\n", profileReconstructionIdc );
    printf( "pixelDeinterleavingType      : %d\n", pixelDeinterleavingType_ );
    printf( "pointLocalReconstructionType : %d\n", pointLocalReconstructionType_ );
    printf( "reconstructEomType           : %d\n", reconstructEomType_ );
    printf( "duplicatedPointRemovalType   : %d\n", duplicatedPointRemovalType_ );
    printf( "reconstructRawType           : %d\n", reconstructRawType_ );
    printf( "applyGeoSmoothingType        : %d\n", applyGeoSmoothingType_ );
    printf( "applyAttrSmoothingType       : %d\n", applyAttrSmoothingType_ );
    printf( "applyAttrTransferFilterType  : %d\n", attrTransferFilterType_ );
    printf( "applyOccupanySynthesisType   : %d\n", applyOccupanySynthesisType_ );
    printf( "---------------------------------------\n" );
  } 
};

struct reconstruction_parameters {
  common_reconstruction_parameters common_rec_params;
  other_reconstruction_parameters other_rec_params;
  sei_reconstruction_info rec_sei_info;
  reconstruction_options rec_opts;
};

class Reconstruction {
  public:
  static void reconstructPointCloud_(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof>& gof_);
  static void reconstructPointCloud(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof>& gof_);
  static void reconstructPointCloudFrame(std::shared_ptr<uvgvpcc_dec::Frame>& frame, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, common_reconstruction_parameters& rec_params);
  static void setReconstructionParameters(
    std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof>& gof_, 
    std::shared_ptr<reconstruction_parameters>& rec_params
  );
};

/* ------------------------ ripped from tmc2------------------------ */
enum PCCPatchOrientation {
  PATCH_ORIENTATION_DEFAULT = 0,  // 0: default
  PATCH_ORIENTATION_SWAP    = 1,  // 1: swap
  PATCH_ORIENTATION_ROT90   = 2,  // 2: rotation 90
  PATCH_ORIENTATION_ROT180  = 3,  // 3: rotation 180
  PATCH_ORIENTATION_ROT270  = 4,  // 4: rotation 270
  PATCH_ORIENTATION_MIRROR  = 5,  // 5: mirror
  PATCH_ORIENTATION_MROT90  = 6,  // 6: mirror + rotation 90
  PATCH_ORIENTATION_MROT180 = 7,  // 7: mirror + rotation 180
  PATCH_ORIENTATION_MROT270 = 8   // 8: similar to SWAP, not used switched SWAP with ROT90 positions
};