#pragma once

/// \file Entry point for the point cloud reconstruction process.

#include "bitstreamParsing/gof.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"
#include "reconstruction/reconstruction.hpp"

using namespace uvgvpcc_dec;

struct smoothedGeoInfo {
    uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput> point;
    uvgvpcc_dec::Vector3<uvgvpcc_dec::typeAttributeInput16bit> color;
    size_t pointIndex = 0;
};

template <typename T>
T PCCClip( const T& n, const T& lower, const T& upper ) {
  return ( std::max )( lower, ( std::min )( n, upper ) );
};

void identifyBoundaryPoints(
    std::vector<uint8_t>& occupancyMap, 
    size_t x, size_t y, 
    size_t tile_width, size_t tile_height, 
    size_t pointIndex, 
    std::vector<uint16_t>& boundaryPointTypes
);

void smoothReconstructedGeometry(
    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>>& reconstructed_geo, 
    const sei_reconstruction_info& sei_params, 
    std::vector<uint16_t>& boundaryPointTypes,
    const std::vector<uint32_t>& partition
);

void smoothReconstructedGeometry(
    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>>& reconstructed_geo,
    std::vector<smoothedGeoInfo>& smoothed_geo_points, 
    const sei_reconstruction_info& sei_params, 
    std::vector<uint16_t>& boundaryPointTypes,
    const std::vector<uint32_t>& partition
);

void smoothReconstructedGeometry(
    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>>& reconstructed_geo,
    std::vector<size_t>& smoothed_point_indices, 
    const sei_reconstruction_info& sei_params, 
    std::vector<uint16_t>& boundaryPointTypes,
    const std::vector<uint32_t>& partition
);

void transferColors16bitBP_fast(
    std::shared_ptr<uvgvpcc_dec::Frame> &frame,
    std::vector<Vector3<typeGeometryInput>>& sourceGeos,
    std::vector<Vector3<typeAttributeInput16bit>>& sourceAttrs,
    std::vector<size_t>& smoothed_point_indices
);

void transferColors16bitBP_fast(
    std::shared_ptr<uvgvpcc_dec::Frame> &frame,
    std::vector<smoothedGeoInfo>& smoothed_geo_points
);