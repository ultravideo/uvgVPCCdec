#include "geometry_smoothing.hpp"
#include "bitstreamParsing/vps.hpp"
#include "pccKdTree.hpp"

using namespace uvgvpcc_dec;

namespace {
    struct validPointInfo {
        uvgvpcc_dec::Vector3<int> P;
        uvgvpcc_dec::Vector3<int> S;
        size_t pointIndex = 0;
        int64_t cellKeys[2][2][2];
        int64_t cellIndices[2][2][2];

        int cellKeys_int[2][2][2];
        int cellIndices_int[2][2][2];
    };

    struct nnPointInfo {
        Vector3<typeGeometryInput> point;
        Vector3<typeAttributeInput16bit> color;
        double dist = 0.0;
        size_t pointIndex = 0;
    };

    struct smoothedGeoPoint {
        Vector3<typeGeometryInput> point;
        size_t pointIndex = 0;
    };

    struct smoothedColorInfo {
        Vector3<typeAttributeInput16bit> firstColor;
        Vector3<double> centroid2 = {0.0, 0.0, 0.0};
        int count = 0;
        double sumWeights{0.0};
    };
    
    struct DistColor {
        double dist;
        Vector3<typeAttributeInput16bit> color;
        // Vector3<typeGeometryInput> point;
    };

    struct nnTargetInfo {
        double dist;
        size_t index;
    };

} // annonymous namespace



void identifyBoundaryPoints(
    std::vector<uint8_t>& occupancyMap, 
    size_t x, size_t y, 
    size_t tile_width, size_t tile_height, 
    size_t pointIndex, 
    std::vector<uint16_t>& boundaryPointTypes
) {
    bool occupied = occupancyMap[y*tile_width + x] != 0;
    if (!occupied) {
        return;
    }

    if (y > 0 && y < tile_height - 1) {
        if (occupancyMap[(y - 1)*tile_width + x] == 0 || occupancyMap[(y + 1) *tile_width + x] == 0) {
            boundaryPointTypes[pointIndex] = 1;
        }
    }
    if (x > 0 && x < tile_width - 1 && boundaryPointTypes[pointIndex] != 1) {
        if (occupancyMap[y*tile_width + (x + 1)] == 0 || occupancyMap[y*tile_width + (x - 1)] == 0) {
            boundaryPointTypes[pointIndex] = 1;
        }
    }
    if ( y > 0 && y < tile_height - 1 && x > 0 && boundaryPointTypes[pointIndex] != 1 ) {
        if ( occupancyMap[(y - 1)*tile_width + (x - 1)] == 0 || occupancyMap[(y + 1)*tile_width + (x - 1)] == 0 ) {
            boundaryPointTypes[pointIndex] = 1;
        }
    }
    if ( y > 0 && y < tile_height - 1 && x < tile_width - 1 && boundaryPointTypes[pointIndex] != 1 ) {
        if ( occupancyMap[( y - 1 ) * tile_width + ( x + 1 )] == 0 || occupancyMap[( y + 1 ) * tile_width + ( x + 1 )] == 0 ) {
            boundaryPointTypes[pointIndex] = 1;
        }
    }
    if ( y == 0 || y == tile_height - 1 || x == 0 || x == tile_width - 1 ) {
        boundaryPointTypes[pointIndex] = 1;
    }

    ////////////// second layer
    if (boundaryPointTypes[pointIndex] != 1) {
        for (int ix = -2; ix <= 2; ix++) {
            for (int iy = -2; iy <= 2; iy++) {
                if (ix == 0 && iy == 0) {
                    continue; // Skip the center point
                }
                size_t neighborX = x + ix;
                size_t neighborY = y + iy;
                if (neighborX < tile_width && neighborY < tile_height) {
                    if (occupancyMap[neighborY*tile_width + neighborX] == 0) {
                        boundaryPointTypes[pointIndex] = 1;
                        ix = 4; // Break the outer loop
                        iy = 4; // Break the inner loop
                    }
                }
            }
        }
        if (y == 1 || y == tile_height - 2 || x == 1 || x == tile_width - 2) {
            boundaryPointTypes[pointIndex] = 1;
        }
    }
}

bool gridFiltering(
    uvgvpcc_dec::Vector3<int> P,
    uvgvpcc_dec::Vector3<int> S,
    uvgvpcc_dec::Vector3<double>& centroid,
    int& count,
    std::vector<uint16_t>& gridCount,
    std::vector<uvgvpcc_dec::Vector3<float>>& center,
    std::vector<bool>& doSmooth,
    uint8_t gridSize,
    uint16_t gridSize_div2,
    uint16_t gridWidth,
    uint64_t gridWidth_pow3,
    int gridSize_mult2,
    int gridSize_mult2_pow3,
    std::unordered_map<size_t,int64_t>& cellIndex
) {
    bool otherClusterPointCount = false;
    int64_t idx[2][2][2];
    for (int dz = 0; dz < 2; dz++) {
        for (int dy = 0; dy < 2; dy++) {
            for (int dx = 0; dx < 2; dx++) {
                int64_t tmp = ((int64_t)S[0] + dx) + gridWidth*(((int64_t)S[1] + dy) + ((int64_t)S[2] + dz)*gridWidth);
                idx[dz][dy][dx] = tmp;
                if (doSmooth[cellIndex[tmp]] && (gridCount[cellIndex[tmp]] != 0U)) {
                    otherClusterPointCount = true; 
                }
            }
        }
    }
    if (!otherClusterPointCount) {
        return false;
    }
    uvgvpcc_dec::Vector3<double> centroid3[2][2][2] = {};
    uvgvpcc_dec::Vector3<double> curVector = P;
    // uvgvpcc_dec::Vector3<double> curVector = {(double)P[0], (double)P[1], (double)P[2]};
    uvgvpcc_dec::Vector3<int> S2 = S * gridSize;
    uvgvpcc_dec::Vector3<int> W = ( P - S2 - gridSize_div2 ) * 2 + 1;
    uvgvpcc_dec::Vector3<int> Q( gridSize_mult2 - W[0], gridSize_mult2 - W[1], gridSize_mult2 - W[2] );
    uvgvpcc_dec::Vector3<double> centroid4 = {0.0, 0.0, 0.0};
    count = 0;
    for (int dz = 0, c = Q[2]; dz < 2; dz++, c = W[2]) {
        for (int dy = 0, b = Q[1]; dy < 2; dy++, b = W[1]) {
            for (int dx = 0, a = Q[0]; dx < 2; dx++, a = W[0]) {
                if ((dx == 0 && dy == 0 && dz == 0) || (idx[dz][dy][dx] < gridWidth_pow3)) {
                    auto index = cellIndex[idx[dz][dy][dx]];
                    centroid3[dz][dy][dx] = gridCount[index] > 0 ? uvgvpcc_dec::Vector3<double>( center[index] ) : curVector;
                } else {
                    centroid3[dz][dy][dx] = curVector;
                }
                int product = a * b * c;
                centroid3[dz][dy][dx] *= product;
                centroid4 += centroid3[dz][dy][dx];
                count += product * gridCount[cellIndex[idx[dz][dy][dx]]];
            }
        }
    }
    centroid4 /= gridSize_mult2_pow3;
    count /= gridSize_mult2_pow3;
    centroid = centroid4 * count;
    return otherClusterPointCount;
}

void smoothReconstructedGeometry_(
    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>>& reconstructed_geo, 
    const sei_reconstruction_info& sei_params, 
    std::vector<uint16_t>& boundaryPointTypes,
    const std::vector<uint32_t>& partition
) {
    const size_t rec_points_count = reconstructed_geo.size();
    const size_t grid_size = sei_params.grid_size;
    const size_t grid_size_div2 = grid_size >> 1;

    // Get max point and compute w
    int16_t maxSize = 0;
    for (size_t i = 0; i < rec_points_count; i++) { // Get max point
        maxSize = std::max({reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2], maxSize});
    } // Get max point

    const size_t grid_width = (maxSize + static_cast<int16_t>(grid_size) - 1) / static_cast<int16_t>(grid_size);

    std::unordered_map<size_t,int64_t> cellIndex;
    size_t numBoundaryCells    = 0;
    const int disth            = ( std::max )( static_cast<int>( grid_size ) / 2, 1 );
    const int threshold        = grid_size * grid_width;
    const int upper            = threshold - disth;

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Index boundary cells >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    for (size_t i = 0; i < rec_points_count; i++) { // Index boundary cells
        if (boundaryPointTypes[i] != 1) {
            continue;
        }
        uvgvpcc_dec::Vector3<int> P(reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2]);
        if (P[0] < disth || P[0] >= upper ||
            P[1] < disth || P[1] >= upper ||
            P[2] < disth || P[2] >= upper) {
            continue;
        }

        uvgvpcc_dec::Vector3<int> P2(P[0] / grid_size, P[1] / grid_size, P[2] / grid_size);
        uvgvpcc_dec::Vector3<int> Q(
            P2[0] + ((P[0] % grid_size < grid_size_div2) ? - 1 : 0), 
            P2[1] + ((P[1] % grid_size < grid_size_div2) ? - 1 : 0), 
            P2[2] + ((P[2] % grid_size < grid_size_div2) ? - 1 : 0)
        );

        for (int ix = 0; ix < 2; ix++) {
            for (int iy = 0; iy < 2; iy++) {
                for (int iz = 0; iz < 2; iz++) {
                    int64_t cellId = ((int64_t)Q[0] + ix) + grid_width*(((int64_t)Q[1] + iy) + ((int64_t)Q[2] + iz)*grid_width);
                    if (cellIndex.find(cellId) == cellIndex.end()) {
                        cellIndex[cellId] = numBoundaryCells;
                        numBoundaryCells++;
                    }
                }
            }
        }

        // int64_t cellKey = (int64_t)P2[0] + grid_width*((int64_t)P2[1] + (int64_t)P2[2]*grid_width);
        // if (cellIndex.find(cellKey) != cellIndex.end()) {
        //     printf("Point at index: %zu ------> Found!\n", i);
        // }

    } // Index boundary cells
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Index boundary cells <<<<<<<<<<<<<<<<<<<<<<<<<<< */

    std::vector<uvgvpcc_dec::Vector3<float>> geoSmoothingCenter(numBoundaryCells, {0., 0., 0.});
    std::vector<uint16_t> geoSmoothingCount(numBoundaryCells, 0);
    std::vector<uint32_t> geoSmoothingPartition(numBoundaryCells);
    std::vector<bool> geoSmoothingDoSmooth(numBoundaryCells, false);

    int found_points = 0;
    int not_found_points = 0;

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Add grid centroids >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    for (size_t i = 0; i < rec_points_count; i++) { // Add grid centroids
        uvgvpcc_dec::Vector3<int> P(reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2]);
        if (P[0] < disth || P[0] >= upper ||
            P[1] < disth || P[1] >= upper ||
            P[2] < disth || P[2] >= upper) {
            continue;
            // printf("Point out of range ----------> Skipped!\n");
        }

        uvgvpcc_dec::Vector3<int> P2(P[0] / grid_size, P[1] / grid_size, P[2] / grid_size);
        int64_t cellKey = (int64_t)P2[0] + grid_width*((int64_t)P2[1] + (int64_t)P2[2]*grid_width);

        // if (cellIndex.find(cellKey) == cellIndex.end()) {
        //     continue;
        // }

        // // if ( count[cellId] == 0 ) {
        // //     gpartition[cellId] = patchIdx;
        // //     centerGrid[cellId] = {0., 0., 0.};
        // //     doSmooth[cellId]   = false;
        // // } else if ( !doSmooth[cellId] && gpartition[cellId] != patchIdx ) {
        // //     doSmooth[cellId] = true;
        // // }
        // // centerGrid[cellId] += PCCVector3<float>( point );
        // // count[cellId]++;
        
        // auto cellIdx = cellIndex[cellKey];
        // uint32_t patchIndexPlus1 = partition[i] + 1;
        // if (geoSmoothingCount[cellIdx] == 0) {
        //     geoSmoothingPartition[cellIdx] = patchIndexPlus1;
        // } else if (!geoSmoothingDoSmooth[cellIdx] && geoSmoothingPartition[cellIdx] != patchIndexPlus1) {
        //     geoSmoothingDoSmooth[cellIdx] = true;
        // }
        // geoSmoothingCenter[cellIdx] += uvgvpcc_dec::Vector3<float>(reconstructed_geo[i]);
        // geoSmoothingCount[cellIdx]++;

        if (cellIndex.find(cellKey) != cellIndex.end()) {
            auto cellIdx = cellIndex[cellKey];
            uint32_t patchIndexPlus1 = partition[i] + 1;
            if (geoSmoothingCount[cellIdx] == 0) {
                geoSmoothingPartition[cellIdx] = patchIndexPlus1;
            } else if (!geoSmoothingDoSmooth[cellIdx] && geoSmoothingPartition[cellIdx] != patchIndexPlus1) {
                geoSmoothingDoSmooth[cellIdx] = true;
                printf("Compute for Index, do smooth = True: %zu, key: %ld\n", i, cellKey);
            }
            geoSmoothingCenter[cellIdx] += uvgvpcc_dec::Vector3<float>(reconstructed_geo[i]);
            geoSmoothingCount[cellIdx]++;
            // printf("Point at index: %zu ------> Found!\n", i);
            found_points++;
        } else {
            // printf("Point at index: %zu ------> Not Found!\n", i);
            not_found_points++;
        }
        
    } // Add grid centroids
    printf("Found points: %d, Not found points: %d\n", found_points, not_found_points);
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Add grid centroids <<<<<<<<<<<<<<<<<<<<<<<<<<< */

    // Compute the average of the grid centroid
    for (size_t i = 0; i < geoSmoothingCount.size(); i++) { 
        // printf("i -> Count = %d\n", geoSmoothingCount[i]);
        if (geoSmoothingCount[i] != 0U) {
            geoSmoothingCenter[i] /= geoSmoothingCount[i];
        }
    }

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Smooth point cloud grids >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    const uint64_t grid_width_pow3 = (uint16_t)grid_width * (uint16_t)grid_width * (uint16_t)grid_width;
    const int grid_size_mult2 = static_cast<uint8_t>(grid_size) << 1;
    const int grid_size_mult2_pow3 = grid_size_mult2 * grid_size_mult2 * grid_size_mult2;

    for (size_t i = 0; i < rec_points_count; i++) {
        if (boundaryPointTypes[i] != 1) {
            continue;
        }
        uvgvpcc_dec::Vector3<int> P(reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2]);
        if (P[0] < disth || P[0] >= upper ||
            P[1] < disth || P[1] >= upper ||
            P[2] < disth || P[2] >= upper) {
            continue;
        }
        
        uvgvpcc_dec::Vector3<int> P2(P[0] / (uint8_t)grid_size, P[1] / (uint8_t)grid_size, P[2] / (uint8_t)grid_size);
        uvgvpcc_dec::Vector3<int> P3 = P - P2 * grid_size;
        uvgvpcc_dec::Vector3<int> S(
            P2[0] + ( ( P3[0] < (uint16_t)grid_size_div2 ) ? -1 : 0 ), 
            P2[1] + ( ( P3[1] < (uint16_t)grid_size_div2 ) ? -1 : 0 ), 
            P2[2] + ( ( P3[2] < (uint16_t)grid_size_div2 ) ? -1 : 0 ) 
        );

        uvgvpcc_dec::Vector3<double> centroid(0.0, 0.0, 0.0);
        int count = 0;

        bool otherClusterPointCount = gridFiltering(
            P, S, centroid, count, 
            geoSmoothingCount, geoSmoothingCenter, geoSmoothingDoSmooth, 
            (int)grid_size, grid_size_div2, 
            (uint16_t)grid_width, grid_width_pow3, grid_size_mult2, grid_size_mult2_pow3, 
            cellIndex
        );

        if (otherClusterPointCount) {
            uvgvpcc_dec::Vector3<double> curVector = P;
            double dist2 = ((curVector * count - centroid).norm2()) / static_cast<double>(count) + 0.5;
            if (dist2 >= std::max({static_cast<int>(sei_params.threshold_smoothing), count})*2) {
                centroid = centroid / static_cast<double>(count) + 0.5;
                for (size_t k = 0; k < 3; k++) {
                    centroid[k] = double( int64_t(centroid[k]) );
                }
                reconstructed_geo[i] = centroid;
                boundaryPointTypes[i] = static_cast<uint16_t>(3);
            }
        }
    }
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Smooth point cloud grids <<<<<<<<<<<<<<<<<<<<<<<<<<< */

}

bool gridFiltering_int(
    uvgvpcc_dec::Vector3<int> P,
    uvgvpcc_dec::Vector3<int> S,
    uvgvpcc_dec::Vector3<double>& centroid,
    int& count,
    std::vector<uint16_t>& gridCount,
    std::vector<uvgvpcc_dec::Vector3<float>>& center,
    std::vector<bool>& doSmooth,
    uint8_t gridSize,
    uint16_t gridSize_div2,
    uint16_t gridWidth,
    uint64_t gridWidth_pow3,
    int gridSize_mult2,
    int gridSize_mult2_pow3,
    std::unordered_map<size_t,int64_t>& cellIndex
) {
    bool otherClusterPointCount = false;
    int64_t cellKeys[2][2][2];
    int64_t cellIndices[2][2][2];

    for (int dz = 0; dz < 2; dz++) {
        for (int dy = 0; dy < 2; dy++) {
            for (int dx = 0; dx < 2; dx++) {
                int64_t tmp = ((int64_t)S[0] + dx) + gridWidth*(((int64_t)S[1] + dy) + ((int64_t)S[2] + dz)*gridWidth);
                cellKeys[dz][dy][dx] = tmp;
                const auto index = cellIndex[tmp];
                cellIndices[dz][dy][dx] = index;
                if (doSmooth[index] && (gridCount[index] != 0U)) {
                    otherClusterPointCount = true; 
                }
            }
        }
    }
    if (!otherClusterPointCount) {
        return false;
    }
    // uvgvpcc_dec::Vector3<double> centroid3[2][2][2] = {};
    // uvgvpcc_dec::Vector3<double> curVector = P;
    uvgvpcc_dec::Vector3<int> S2 = S * gridSize;
    uvgvpcc_dec::Vector3<int> W = ( P - S2 - gridSize_div2 ) * 2 + 1;
    uvgvpcc_dec::Vector3<int> Q( gridSize_mult2 - W[0], gridSize_mult2 - W[1], gridSize_mult2 - W[2] );
    uvgvpcc_dec::Vector3<double> centroid4 = {0.0, 0.0, 0.0};
    count = 0;
    for (int dz = 0, c = Q[2]; dz < 2; dz++, c = W[2]) {
        for (int dy = 0, b = Q[1]; dy < 2; dy++, b = W[1]) {
            for (int dx = 0, a = Q[0]; dx < 2; dx++, a = W[0]) {
                const auto index = cellIndices[dz][dy][dx];
                uvgvpcc_dec::Vector3<double> centroid3 = {};
                if ((dx == 0 && dy == 0 && dz == 0) || (cellKeys[dz][dy][dx] < gridWidth_pow3)) {
                    centroid3 = gridCount[index] > 0 ? uvgvpcc_dec::Vector3<double>( center[index] ) : uvgvpcc_dec::Vector3<double>(P);
                } else {
                    centroid3 = P;
                }
                int product = a * b * c;
                centroid3 *= product;
                centroid4 += centroid3;
                count += product * gridCount[index];
            }
        }
    }
    count /= gridSize_mult2_pow3;
    centroid = centroid4  * (static_cast<double>(count) / gridSize_mult2_pow3);
    return otherClusterPointCount;
}

bool gridFiltering(
    const validPointInfo& pointInfo,
    uvgvpcc_dec::Vector3<double>& centroid,
    int& count,
    std::vector<uint16_t>& gridCount,
    std::vector<uvgvpcc_dec::Vector3<float>>& center,
    std::vector<bool>& doSmooth,
    uint8_t gridSize,
    uint16_t gridSize_div2,
    uint64_t gridWidth_pow3,
    int gridSize_mult2,
    int gridSize_mult2_pow3
) {
    // bool otherClusterPointCount = false;

    // for (int dz = 0; dz < 2; dz++) {
    //     for (int dy = 0; dy < 2; dy++) {
    //         for (int dx = 0; dx < 2; dx++) {
    //             // const auto index = cellIndex[pointInfo.cellKeys[dz][dy][dx]];
    //             const auto index = pointInfo.cellIndices[dz][dy][dx];
    //             if (doSmooth[index] && (gridCount[index] != 0U)) {
    //                 otherClusterPointCount = true; 
    //                 dz = dy = 4;
    //                 break;
    //             }
    //         }
    //     }
    // }
    // if (!otherClusterPointCount) {
    //     return false;
    // }
    // uvgvpcc_dec::Vector3<int> S2 = pointInfo.S * gridSize;
    // uvgvpcc_dec::Vector3<int> W = ( pointInfo.P - S2 - gridSize_div2 ) * 2 + 1;
    // uvgvpcc_dec::Vector3<int> Q( gridSize_mult2 - W[0], gridSize_mult2 - W[1], gridSize_mult2 - W[2] );
    // uvgvpcc_dec::Vector3<double> centroid4 = {0.0, 0.0, 0.0};
    // uvgvpcc_dec::Vector3<double> P_double = pointInfo.P;
    // count = 0;
    // for (int dz = 0, c = Q[2]; dz < 2; dz++, c = W[2]) {
    //     for (int dy = 0, b = Q[1]; dy < 2; dy++, b = W[1]) {
    //         for (int dx = 0, a = Q[0]; dx < 2; dx++, a = W[0]) {
    //             const auto index = pointInfo.cellIndices[dz][dy][dx];
    //             uvgvpcc_dec::Vector3<double> centroid3 = gridCount[index] > 0 ? uvgvpcc_dec::Vector3<double>( center[index] ) : P_double;
    //             const int weight = a * b * c;
    //             centroid4 += centroid3 * weight;
    //             count += weight * gridCount[index];
    //         }
    //     }
    // }
    // count /= gridSize_mult2_pow3;
    // centroid = centroid4 / gridSize_mult2_pow3;
    // return otherClusterPointCount;

    bool otherClusterPointCount = false;

    for (int dz = 0; dz < 2; dz++) {
        for (int dy = 0; dy < 2; dy++) {
            for (int dx = 0; dx < 2; dx++) {
                const auto index = pointInfo.cellIndices_int[dz][dy][dx];
                if (doSmooth[index] && (gridCount[index] != 0U)) {
                    otherClusterPointCount = true; 
                    dz = dy = 4;
                    break;
                }
            }
        }
    }
    if (!otherClusterPointCount) {
        return false;
    }
    uvgvpcc_dec::Vector3<int> S2 = pointInfo.S * gridSize;
    uvgvpcc_dec::Vector3<int> W = ( pointInfo.P - S2 - gridSize_div2 ) * 2 + 1;
    uvgvpcc_dec::Vector3<int> Q( gridSize_mult2 - W[0], gridSize_mult2 - W[1], gridSize_mult2 - W[2] );
    uvgvpcc_dec::Vector3<double> centroid4 = {0.0, 0.0, 0.0};
    uvgvpcc_dec::Vector3<double> P_double = pointInfo.P;
    count = 0;
    for (int dz = 0, c = Q[2]; dz < 2; dz++, c = W[2]) {
        for (int dy = 0, b = Q[1]; dy < 2; dy++, b = W[1]) {
            for (int dx = 0, a = Q[0]; dx < 2; dx++, a = W[0]) {
                const auto index = pointInfo.cellIndices_int[dz][dy][dx];
                uvgvpcc_dec::Vector3<double> centroid3 = gridCount[index] > 0 ? uvgvpcc_dec::Vector3<double>( center[index] ) : P_double;
                const int weight = a * b * c;
                centroid4 += centroid3 * weight;
                count += weight * gridCount[index];
            }
        }
    }
    count /= gridSize_mult2_pow3;
    centroid = centroid4 / gridSize_mult2_pow3;
    return otherClusterPointCount;
}

// Before 4.79s
void smoothReconstructedGeometry(
    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>>& reconstructed_geo,
    // std::vector<smoothedGeoInfo>& smoothed_geo_points, 
    const sei_reconstruction_info& sei_params, 
    std::vector<uint16_t>& boundaryPointTypes,
    const std::vector<uint32_t>& partition
) {
    const size_t rec_points_count = reconstructed_geo.size();
    const size_t grid_size = sei_params.grid_size;
    const size_t grid_size_div2 = grid_size >> 1;

    std::vector<validPointInfo> candidate_points_info;
    candidate_points_info.reserve(rec_points_count/3);

    // Get max point and compute w
    int16_t maxSize = 0;
    for (size_t i = 0; i < rec_points_count; i++) { // Get max point
        maxSize = std::max({reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2], maxSize});
    } // Get max point

    const size_t grid_width = (maxSize + static_cast<int16_t>(grid_size) - 1) / static_cast<int16_t>(grid_size);

    //std::unordered_map<size_t,int64_t> cellIndex;
    std::vector<int> cellIndex;
    cellIndex.resize(grid_width * grid_width * grid_width);
    std::fill(cellIndex.begin(), cellIndex.end(), -1);

    size_t numBoundaryCells    = 0;
    const int disth            = ( std::max )( static_cast<int>( grid_size ) / 2, 1 );
    const int threshold        = grid_size * grid_width;
    const int upper            = threshold - disth;

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Index boundary cells >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    for (size_t i = 0; i < rec_points_count; i++) { // Index boundary cells
        if (boundaryPointTypes[i] != 1) {
            continue;
        }
        uvgvpcc_dec::Vector3<int> P(reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2]);
        if (P[0] < disth || P[0] >= upper ||
            P[1] < disth || P[1] >= upper ||
            P[2] < disth || P[2] >= upper) {
            continue;
        }

        uvgvpcc_dec::Vector3<int> P2(P[0] / grid_size, P[1] / grid_size, P[2] / grid_size);
        uvgvpcc_dec::Vector3<int> Q(
            P2[0] + ((P[0] % grid_size < grid_size_div2) ? - 1 : 0), 
            P2[1] + ((P[1] % grid_size < grid_size_div2) ? - 1 : 0), 
            P2[2] + ((P[2] % grid_size < grid_size_div2) ? - 1 : 0)
        );

        validPointInfo point;
        for (int ix = 0; ix < 2; ix++) {
            for (int iy = 0; iy < 2; iy++) {
                for (int iz = 0; iz < 2; iz++) {
                    // int64_t cellId = ((int64_t)Q[0] + ix) + grid_width*(((int64_t)Q[1] + iy) + ((int64_t)Q[2] + iz)*grid_width);
                    // point.cellKeys[iz][iy][ix] = cellId;
                    // const auto it =  cellIndex.emplace(cellId, numBoundaryCells);
                    // if (it.second) {
                    //     numBoundaryCells++;
                    // }
                    // point.cellIndices[iz][iy][ix] = it.first->second;

                    int cellId = (Q[0] + ix) + grid_width*((Q[1] + iy) + (Q[2] + iz)*grid_width);
                    if (cellIndex[cellId] == -1) {
                        cellIndex[cellId] = numBoundaryCells;
                        numBoundaryCells++;
                    } 
                    point.cellKeys_int[iz][iy][ix] = cellId;
                    point.cellIndices_int[iz][iy][ix] = cellIndex[cellId];
                }
            }
        }
        point.P = P;
        point.S = Q;
        point.pointIndex = i;

        candidate_points_info.push_back(point);

    } // Index boundary cells
    //printf("cellIndex size: %zu\n", cellIndex.size());
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Index boundary cells <<<<<<<<<<<<<<<<<<<<<<<<<<< */

    std::vector<uvgvpcc_dec::Vector3<float>> geoSmoothingCenter(numBoundaryCells, {0., 0., 0.});
    std::vector<uint16_t> geoSmoothingCount(numBoundaryCells, 0);
    std::vector<uint32_t> geoSmoothingPartition(numBoundaryCells);
    std::vector<bool> geoSmoothingDoSmooth(numBoundaryCells, false);

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Add grid centroids >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    for (size_t i = 0; i < rec_points_count; i++) { // Add grid centroids
        uvgvpcc_dec::Vector3<int> P(reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2]);
        if (P[0] < disth || P[0] >= upper ||
            P[1] < disth || P[1] >= upper ||
            P[2] < disth || P[2] >= upper) {
            continue;
        }

        // uvgvpcc_dec::Vector3<int> P2(P[0] / grid_size, P[1] / grid_size, P[2] / grid_size);
        // int64_t cellKey = (int64_t)P2[0] + grid_width*((int64_t)P2[1] + (int64_t)P2[2]*grid_width);

        // auto it = cellIndex.find(cellKey);
        // if (it != cellIndex.end()) {
        //     const auto cellIdx = it->second;
        //     uint32_t patchIndexPlus1 = partition[i] + 1;
        //     if (geoSmoothingCount[cellIdx] == 0) {
        //         geoSmoothingPartition[cellIdx] = patchIndexPlus1;
        //     } else if (!geoSmoothingDoSmooth[cellIdx] && geoSmoothingPartition[cellIdx] != patchIndexPlus1) {
        //         geoSmoothingDoSmooth[cellIdx] = true;
        //     }
        //     geoSmoothingCenter[cellIdx] += uvgvpcc_dec::Vector3<float>(reconstructed_geo[i]);
        //     geoSmoothingCount[cellIdx]++;
        // }

        uvgvpcc_dec::Vector3<int> P2(P[0] / grid_size, P[1] / grid_size, P[2] / grid_size);
        int cellKey = P2[0] + grid_width*(P2[1] + P2[2]*grid_width);

        if (cellIndex[cellKey] != -1) {
            const auto cellIdx = cellIndex[cellKey];
            uint32_t patchIndexPlus1 = partition[i] + 1;
            if (geoSmoothingCount[cellIdx] == 0) {
                geoSmoothingPartition[cellIdx] = patchIndexPlus1;
            } else if (!geoSmoothingDoSmooth[cellIdx] && geoSmoothingPartition[cellIdx] != patchIndexPlus1) {
                geoSmoothingDoSmooth[cellIdx] = true;
            }
            geoSmoothingCenter[cellIdx] += uvgvpcc_dec::Vector3<float>(reconstructed_geo[i]);
            geoSmoothingCount[cellIdx]++;
        }
        
    } // Add grid centroids
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Add grid centroids <<<<<<<<<<<<<<<<<<<<<<<<<<< */

    // Compute the average of the grid centroid
    for (size_t i = 0; i < geoSmoothingCount.size(); i++) { 
        if (geoSmoothingCount[i] != 0U) {
            geoSmoothingCenter[i] /= geoSmoothingCount[i];
        }
    }

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Smooth point cloud grids >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    const uint64_t grid_width_pow3 = (uint16_t)grid_width * (uint16_t)grid_width * (uint16_t)grid_width;
    const int grid_size_mult2 = static_cast<uint8_t>(grid_size) << 1;
    const int grid_size_mult2_pow3 = grid_size_mult2 * grid_size_mult2 * grid_size_mult2;
    for (const auto& rec_point_info : candidate_points_info) {
        uvgvpcc_dec::Vector3<double> centroid(0.0, 0.0, 0.0);
        int count = 0;

        bool otherClusterPointCount = gridFiltering(
            rec_point_info, centroid, count, 
            geoSmoothingCount, geoSmoothingCenter, geoSmoothingDoSmooth, 
            (int)grid_size, grid_size_div2, 
            grid_width_pow3, grid_size_mult2, grid_size_mult2_pow3
        );

        if (otherClusterPointCount) {
            uvgvpcc_dec::Vector3<double> curVector = rec_point_info.P;
            double dist2 = ((curVector - centroid).norm2())*count + 0.5;
            if (dist2 >= std::max({static_cast<int>(sei_params.threshold_smoothing), count})*2) {
                centroid = centroid + 0.5;
                reconstructed_geo[rec_point_info.pointIndex] = centroid;
                boundaryPointTypes[rec_point_info.pointIndex] = static_cast<uint16_t>(3);
            }
        }
    }
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Smooth point cloud grids <<<<<<<<<<<<<<<<<<<<<<<<<<< */

}

void smoothReconstructedGeometry(
    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>>& reconstructed_geo,
    std::vector<smoothedGeoInfo>& smoothed_geo_points, 
    const sei_reconstruction_info& sei_params, 
    std::vector<uint16_t>& boundaryPointTypes,
    const std::vector<uint32_t>& partition
) {
    const size_t rec_points_count = reconstructed_geo.size();
    const size_t grid_size = sei_params.grid_size;
    const size_t grid_size_div2 = grid_size >> 1;

    std::vector<validPointInfo> candidate_points_info;
    candidate_points_info.reserve(rec_points_count/3);

    // Get max point and compute w
    int16_t maxSize = 0;
    for (size_t i = 0; i < rec_points_count; i++) { // Get max point
        maxSize = std::max({reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2], maxSize});
    } // Get max point

    const size_t grid_width = (maxSize + static_cast<int16_t>(grid_size) - 1) / static_cast<int16_t>(grid_size);

    //std::unordered_map<size_t,int64_t> cellIndex;
    std::vector<int> cellIndex;
    cellIndex.resize(grid_width * grid_width * grid_width);
    std::fill(cellIndex.begin(), cellIndex.end(), -1);

    size_t numBoundaryCells    = 0;
    const int disth            = ( std::max )( static_cast<int>( grid_size ) / 2, 1 );
    const int threshold        = grid_size * grid_width;
    const int upper            = threshold - disth;

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Index boundary cells >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    for (size_t i = 0; i < rec_points_count; i++) { // Index boundary cells
        if (boundaryPointTypes[i] != 1) {
            continue;
        }
        uvgvpcc_dec::Vector3<int> P(reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2]);
        if (P[0] < disth || P[0] >= upper ||
            P[1] < disth || P[1] >= upper ||
            P[2] < disth || P[2] >= upper) {
            continue;
        }

        uvgvpcc_dec::Vector3<int> P2(P[0] / grid_size, P[1] / grid_size, P[2] / grid_size);
        uvgvpcc_dec::Vector3<int> Q(
            P2[0] + ((P[0] % grid_size < grid_size_div2) ? - 1 : 0), 
            P2[1] + ((P[1] % grid_size < grid_size_div2) ? - 1 : 0), 
            P2[2] + ((P[2] % grid_size < grid_size_div2) ? - 1 : 0)
        );

        validPointInfo point;
        for (int ix = 0; ix < 2; ix++) {
            for (int iy = 0; iy < 2; iy++) {
                for (int iz = 0; iz < 2; iz++) {
                    int cellId = (Q[0] + ix) + grid_width*((Q[1] + iy) + (Q[2] + iz)*grid_width);
                    if (cellIndex[cellId] == -1) {
                        cellIndex[cellId] = numBoundaryCells;
                        numBoundaryCells++;
                    } 
                    point.cellKeys_int[iz][iy][ix] = cellId;
                    point.cellIndices_int[iz][iy][ix] = cellIndex[cellId];
                }
            }
        }
        point.P = P;
        point.S = Q;
        point.pointIndex = i;

        candidate_points_info.push_back(point);

    } // Index boundary cells
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Index boundary cells <<<<<<<<<<<<<<<<<<<<<<<<<<< */

    std::vector<uvgvpcc_dec::Vector3<float>> geoSmoothingCenter(numBoundaryCells, {0., 0., 0.});
    std::vector<uint16_t> geoSmoothingCount(numBoundaryCells, 0);
    std::vector<uint32_t> geoSmoothingPartition(numBoundaryCells);
    std::vector<bool> geoSmoothingDoSmooth(numBoundaryCells, false);

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Add grid centroids >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    for (size_t i = 0; i < rec_points_count; i++) { // Add grid centroids
        uvgvpcc_dec::Vector3<int> P(reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2]);
        if (P[0] < disth || P[0] >= upper ||
            P[1] < disth || P[1] >= upper ||
            P[2] < disth || P[2] >= upper) {
            continue;
        }

        uvgvpcc_dec::Vector3<int> P2(P[0] / grid_size, P[1] / grid_size, P[2] / grid_size);
        int cellKey = P2[0] + grid_width*(P2[1] + P2[2]*grid_width);

        if (cellIndex[cellKey] != -1) {
            const auto cellIdx = cellIndex[cellKey];
            uint32_t patchIndexPlus1 = partition[i] + 1;
            if (geoSmoothingCount[cellIdx] == 0) {
                geoSmoothingPartition[cellIdx] = patchIndexPlus1;
            } else if (!geoSmoothingDoSmooth[cellIdx] && geoSmoothingPartition[cellIdx] != patchIndexPlus1) {
                geoSmoothingDoSmooth[cellIdx] = true;
            }
            geoSmoothingCenter[cellIdx] += uvgvpcc_dec::Vector3<float>(reconstructed_geo[i]);
            geoSmoothingCount[cellIdx]++;
        }
        
    } // Add grid centroids
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Add grid centroids <<<<<<<<<<<<<<<<<<<<<<<<<<< */

    // Compute the average of the grid centroid
    for (size_t i = 0; i < geoSmoothingCount.size(); i++) { 
        if (geoSmoothingCount[i] != 0U) {
            geoSmoothingCenter[i] /= geoSmoothingCount[i];
        }
    }

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Smooth point cloud grids >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    const uint64_t grid_width_pow3 = (uint16_t)grid_width * (uint16_t)grid_width * (uint16_t)grid_width;
    const int grid_size_mult2 = static_cast<uint8_t>(grid_size) << 1;
    const int grid_size_mult2_pow3 = grid_size_mult2 * grid_size_mult2 * grid_size_mult2;
    smoothed_geo_points.clear();
    smoothed_geo_points.reserve(candidate_points_info.size());
    uint64_t sumTotalgeoN2_geo_smoothed = 0;
    for (const auto& rec_point_info : candidate_points_info) {
        uvgvpcc_dec::Vector3<double> centroid(0.0, 0.0, 0.0);
        int count = 0;

        bool otherClusterPointCount = gridFiltering(
            rec_point_info, centroid, count, 
            geoSmoothingCount, geoSmoothingCenter, geoSmoothingDoSmooth, 
            (int)grid_size, grid_size_div2, 
            grid_width_pow3, grid_size_mult2, grid_size_mult2_pow3
        );

        if (otherClusterPointCount) {
            uvgvpcc_dec::Vector3<double> curVector = rec_point_info.P;
            double dist2 = ((curVector - centroid).norm2())*static_cast<double>( count ) + 0.5;
            if (dist2 >= std::max({static_cast<int>(sei_params.threshold_smoothing), count})*2) {
                centroid = centroid + 0.5;
                smoothedGeoInfo p;
                p.point = centroid;
                p.pointIndex = rec_point_info.pointIndex;
                smoothed_geo_points.push_back(p);
                //smoothed_geo_points.push_back({centroid, {0, 0, 0}, rec_point_info.pointIndex});

                //reconstructed_geo[rec_point_info.pointIndex] = centroid;
                //boundaryPointTypes[rec_point_info.pointIndex] = static_cast<uint16_t>(3);
            }
        }
    }
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Smooth point cloud grids <<<<<<<<<<<<<<<<<<<<<<<<<<< */
}

void smoothReconstructedGeometry(
    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>>& reconstructed_geo,
    std::vector<size_t>& smoothed_point_indices, 
    const sei_reconstruction_info& sei_params, 
    std::vector<uint16_t>& boundaryPointTypes,
    const std::vector<uint32_t>& partition
) {
    const size_t rec_points_count = reconstructed_geo.size();
    const size_t grid_size = sei_params.grid_size;
    const size_t grid_size_div2 = grid_size >> 1;

    std::vector<validPointInfo> candidate_points_info;
    candidate_points_info.reserve(rec_points_count/3);

    // Get max point and compute w
    int16_t maxSize = 0;
    for (size_t i = 0; i < rec_points_count; i++) { // Get max point
        maxSize = std::max({reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2], maxSize});
    } // Get max point

    const size_t grid_width = (maxSize + static_cast<int16_t>(grid_size) - 1) / static_cast<int16_t>(grid_size);

    //std::unordered_map<size_t,int64_t> cellIndex;
    std::vector<int> cellIndex;
    cellIndex.resize(grid_width * grid_width * grid_width);
    std::fill(cellIndex.begin(), cellIndex.end(), -1);

    size_t numBoundaryCells    = 0;
    const int disth            = ( std::max )( static_cast<int>( grid_size ) / 2, 1 );
    const int threshold        = grid_size * grid_width;
    const int upper            = threshold - disth;

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Index boundary cells >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    for (size_t i = 0; i < rec_points_count; i++) { // Index boundary cells
        if (boundaryPointTypes[i] != 1) {
            continue;
        }
        uvgvpcc_dec::Vector3<int> P(reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2]);
        if (P[0] < disth || P[0] >= upper ||
            P[1] < disth || P[1] >= upper ||
            P[2] < disth || P[2] >= upper) {
            continue;
        }

        uvgvpcc_dec::Vector3<int> P2(P[0] / grid_size, P[1] / grid_size, P[2] / grid_size);
        uvgvpcc_dec::Vector3<int> Q(
            P2[0] + ((P[0] % grid_size < grid_size_div2) ? - 1 : 0), 
            P2[1] + ((P[1] % grid_size < grid_size_div2) ? - 1 : 0), 
            P2[2] + ((P[2] % grid_size < grid_size_div2) ? - 1 : 0)
        );

        validPointInfo point;
        for (int ix = 0; ix < 2; ix++) {
            for (int iy = 0; iy < 2; iy++) {
                for (int iz = 0; iz < 2; iz++) {
                    int cellId = (Q[0] + ix) + grid_width*((Q[1] + iy) + (Q[2] + iz)*grid_width);
                    if (cellIndex[cellId] == -1) {
                        cellIndex[cellId] = numBoundaryCells;
                        numBoundaryCells++;
                    } 
                    point.cellKeys_int[iz][iy][ix] = cellId;
                    point.cellIndices_int[iz][iy][ix] = cellIndex[cellId];
                }
            }
        }
        point.P = P;
        point.S = Q;
        point.pointIndex = i;

        candidate_points_info.push_back(point);

    } // Index boundary cells
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Index boundary cells <<<<<<<<<<<<<<<<<<<<<<<<<<< */

    std::vector<uvgvpcc_dec::Vector3<float>> geoSmoothingCenter(numBoundaryCells, {0., 0., 0.});
    std::vector<uint16_t> geoSmoothingCount(numBoundaryCells, 0);
    std::vector<uint32_t> geoSmoothingPartition(numBoundaryCells);
    std::vector<bool> geoSmoothingDoSmooth(numBoundaryCells, false);

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Add grid centroids >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    for (size_t i = 0; i < rec_points_count; i++) { // Add grid centroids
        uvgvpcc_dec::Vector3<int> P(reconstructed_geo[i][0], reconstructed_geo[i][1], reconstructed_geo[i][2]);
        if (P[0] < disth || P[0] >= upper ||
            P[1] < disth || P[1] >= upper ||
            P[2] < disth || P[2] >= upper) {
            continue;
        }

        uvgvpcc_dec::Vector3<int> P2(P[0] / grid_size, P[1] / grid_size, P[2] / grid_size);
        int cellKey = P2[0] + grid_width*(P2[1] + P2[2]*grid_width);

        if (cellIndex[cellKey] != -1) {
            const auto cellIdx = cellIndex[cellKey];
            uint32_t patchIndexPlus1 = partition[i] + 1;
            if (geoSmoothingCount[cellIdx] == 0) {
                geoSmoothingPartition[cellIdx] = patchIndexPlus1;
            } else if (!geoSmoothingDoSmooth[cellIdx] && geoSmoothingPartition[cellIdx] != patchIndexPlus1) {
                geoSmoothingDoSmooth[cellIdx] = true;
            }
            geoSmoothingCenter[cellIdx] += uvgvpcc_dec::Vector3<float>(reconstructed_geo[i]);
            geoSmoothingCount[cellIdx]++;
        }
        
    } // Add grid centroids
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Add grid centroids <<<<<<<<<<<<<<<<<<<<<<<<<<< */

    // Compute the average of the grid centroid
    for (size_t i = 0; i < geoSmoothingCount.size(); i++) { 
        if (geoSmoothingCount[i] != 0U) {
            geoSmoothingCenter[i] /= geoSmoothingCount[i];
        }
    }

    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>> Smooth point cloud grids >>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    const uint64_t grid_width_pow3 = (uint16_t)grid_width * (uint16_t)grid_width * (uint16_t)grid_width;
    const int grid_size_mult2 = static_cast<uint8_t>(grid_size) << 1;
    const int grid_size_mult2_pow3 = grid_size_mult2 * grid_size_mult2 * grid_size_mult2;
    smoothed_point_indices.clear();
    smoothed_point_indices.reserve(candidate_points_info.size());
    uint64_t sumTotalgeoN2_geo_smoothed = 0;
    for (const auto& rec_point_info : candidate_points_info) {
        uvgvpcc_dec::Vector3<double> centroid(0.0, 0.0, 0.0);
        int count = 0;

        bool otherClusterPointCount = gridFiltering(
            rec_point_info, centroid, count, 
            geoSmoothingCount, geoSmoothingCenter, geoSmoothingDoSmooth, 
            (int)grid_size, grid_size_div2, 
            grid_width_pow3, grid_size_mult2, grid_size_mult2_pow3
        );

        if (otherClusterPointCount) {
            uvgvpcc_dec::Vector3<double> curVector = rec_point_info.P;
            double dist2 = ((curVector - centroid).norm2())*static_cast<double>( count ) + 0.5;
            if (dist2 >= std::max({static_cast<int>(sei_params.threshold_smoothing), count})*2) {
                centroid = centroid + 0.5;
                reconstructed_geo[rec_point_info.pointIndex] = centroid;
                smoothed_point_indices.emplace_back(rec_point_info.pointIndex);
                //boundaryPointTypes[rec_point_info.pointIndex] = static_cast<uint16_t>(3);
            }
        }
    }
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<< Smooth point cloud grids <<<<<<<<<<<<<<<<<<<<<<<<<<< */
}

void transferColors16bitBP_fast(
    std::shared_ptr<uvgvpcc_dec::Frame> &frame,
    std::vector<Vector3<typeGeometryInput>>& sourceGeos,
    std::vector<Vector3<typeAttributeInput16bit>>& sourceAttrs,
    std::vector<size_t>& smoothed_point_indices
) {
    constexpr int numNeighborsColorTransferFwd = 8;
    constexpr int numNeighborsColorTransferBwd = 1;
    constexpr double distOffsetFwd = 4.0;
    constexpr double distOffsetBwd = 4.0;

    std::vector<Vector3<typeGeometryInput>>& targetGeos = frame->pointsGeometry;
    std::vector<Vector3<typeAttributeInput16bit>>& targetAttrs = frame->pointsAttribute16bits;

    pccNNResult nnResult;

    // ==========================================================================================
    //                                     Forward direction
    // ==========================================================================================

    std::vector<std::vector<size_t>> nnSourceIndices;
    nnSourceIndices.resize(smoothed_point_indices.size());

    std::vector<std::vector<double>> nnSourceDistances;
    nnSourceDistances.resize(smoothed_point_indices.size());

    pccKdTree kdTreeSource(sourceGeos);
    for (size_t i = 0; i < smoothed_point_indices.size(); i++) {
        kdTreeSource.search(targetGeos[smoothed_point_indices[i]], numNeighborsColorTransferFwd, nnResult);

        nnSourceIndices[i].resize(numNeighborsColorTransferFwd);
        nnSourceDistances[i].resize(numNeighborsColorTransferFwd);
        for (size_t j = 0; j < numNeighborsColorTransferFwd; j++) {
            const size_t sourceIndex = nnResult.indices(j);
            nnSourceIndices[i][j] = sourceIndex;
            nnSourceDistances[i][j] = nnResult.dist(j);
        }
    }

    // ==========================================================================================
    //                                  Backward direction
    // ==========================================================================================

    pccKdTree kdtreeTarget(targetGeos);

    std::vector<std::vector<nnTargetInfo>> refinedColorsDists2;
    refinedColorsDists2.resize(sourceGeos.size());

    for (const auto& indices : nnSourceIndices) {
        for (const auto index : indices) {
            const Vector3<typeAttributeInput16bit>& color = sourceAttrs[index];
            kdtreeTarget.search(sourceGeos[index], numNeighborsColorTransferBwd, nnResult);
            const size_t pointIndex = nnResult.indices(0);

            if (std::abs(color[0] - targetAttrs[pointIndex][0]) >= 40) continue;
            if (std::abs(color[1] - targetAttrs[pointIndex][1]) >= 40) continue;
            if (std::abs(color[2] - targetAttrs[pointIndex][2]) >= 40) continue;
            refinedColorsDists2[pointIndex].push_back(nnTargetInfo{nnResult.dist(0), index});
        }
    }

    for (size_t i = 0; i < smoothed_point_indices.size(); i++) {
        const auto pointIndex = smoothed_point_indices[i];
        auto& colorsDists2 = refinedColorsDists2[pointIndex];
        
        if (colorsDists2.empty() /* || losslessAttribute */) {
            if (nnSourceDistances[i][0] < 0.0001) {
                targetAttrs[pointIndex] = sourceAttrs[nnSourceIndices[i][0]];
            } else {
                Vector3<double> refinedColor(0.0);
                double sumWeights{0.0};
                for (int j = 0; j < numNeighborsColorTransferFwd; j++) {
                    const double weight = 1 / (nnSourceDistances[i][j] + distOffsetFwd);
                    for (int k = 0; k < 3; k++) {
                        refinedColor[k] += sourceAttrs[nnSourceIndices[i][j]][k] * weight;
                    }
                    sumWeights += weight;
                }   
                refinedColor /= sumWeights;
                for (int k = 0; k < 3; k++) {
                    targetAttrs[pointIndex][k] = uint16_t(PCCClip(round(refinedColor[k]), 0.0, 65535.0));
                }
            }
            continue;
        }
        if (colorsDists2.size() == 1) {
            targetAttrs[pointIndex] = sourceAttrs[colorsDists2[0].index];
            continue;
        } 
        Vector3<double> centroid2(0.0);
        double sumWeights{0.0};
        for (auto& colorDist2 : colorsDists2) {
            const double weight = 1 / (sqrt(colorDist2.dist) + distOffsetBwd);
            for (size_t k = 0; k < 3; k++) {
                centroid2[k] += (sourceAttrs[colorDist2.index][k] * weight);
            }
            sumWeights += weight;
        }
        centroid2 /= sumWeights;
        for (size_t k = 0; k < 3; k++) {
            targetAttrs[pointIndex][k] = uint16_t(PCCClip(round(centroid2[k]), 0.0, 65535.0));
        }
    }
}

void transferColors16bitBP_fast___(
    std::shared_ptr<uvgvpcc_dec::Frame> &frame,
    std::vector<smoothedGeoInfo>& smoothed_geo_points
) {
    constexpr int numNeighborsColorTransferFwd = 8;
    constexpr int numNeighborsColorTransferBwd = 1;
    constexpr double distOffsetFwd = 4.0;
    constexpr double distOffsetBwd = 4.0;

    std::vector<Vector3<typeGeometryInput>> sourceGeos = frame->pointsGeometry;
    std::vector<Vector3<typeAttributeInput16bit>> sourceAttrs = frame->pointsAttribute16bits;

    std::vector<Vector3<typeGeometryInput>>& targetGeos = frame->pointsGeometry;
    std::vector<Vector3<typeAttributeInput16bit>>& targetAttrs = frame->pointsAttribute16bits;

    pccNNResult nnResult;

    // ==========================================================================================
    //                                     Forward direction
    // ==========================================================================================

    std::vector<size_t> nnSourceIndices;
    nnSourceIndices.resize(sourceGeos.size(), 0);

    pccKdTree kdTreeSource(sourceGeos);
    for(auto& pointInfo : smoothed_geo_points) {
        kdTreeSource.search(pointInfo.point, numNeighborsColorTransferFwd, nnResult);
        
        for (size_t i = 0; i < 8; ++i) {
            const size_t sourceIndex = nnResult.indices(i);
            nnSourceIndices[sourceIndex] = 1;
        }
        
        if (nnResult.dist(0) < 0.0001) {
            targetAttrs[pointInfo.pointIndex] = sourceAttrs[nnResult.indices(0)];
            continue;
        }

        Vector3<double> refinedColor(0.0);
        double sumWeights{0.0};
        for (int i = 0; i < 8; i++) {
            const double weight = 1 / (nnResult.dist(i) + distOffsetFwd);
            for (int k = 0; k < 3; k++) {
                refinedColor[k] += sourceAttrs[nnResult.indices(i)][k] * weight;
            }
            sumWeights += weight;
        }   
        refinedColor /= sumWeights;
        for (int k = 0; k < 3; k++) {
            targetAttrs[pointInfo.pointIndex][k] = uint16_t(PCCClip(round(refinedColor[k]), 0.0, 65535.0));
        }
    }

    // ==========================================================================================
    //                                  Backward direction
    // ==========================================================================================

    // Replace points of source geometry with smoothed geometry points
    for(const auto& pointInfo : smoothed_geo_points) {
        targetGeos[pointInfo.pointIndex] = pointInfo.point;
    }

    pccKdTree kdtreeTarget(targetGeos);

    std::vector<std::vector<DistColor>> refinedColorsDists2;
    refinedColorsDists2.resize(sourceGeos.size());

    for (size_t index = 0; index < nnSourceIndices.size(); index++) {
        if (nnSourceIndices[index] == 0) {
            continue;
        }
        const Vector3<typeAttributeInput16bit>& color = sourceAttrs[index];
        kdtreeTarget.search(sourceGeos[index], numNeighborsColorTransferBwd, nnResult);
        const size_t pointIndex = nnResult.indices(0);

        if (std::abs(color[0] - sourceAttrs[pointIndex][0]) >= 40) continue;
        if (std::abs(color[1] - sourceAttrs[pointIndex][1]) >= 40) continue;
        if (std::abs(color[2] - sourceAttrs[pointIndex][2]) >= 40) continue;
        refinedColorsDists2[pointIndex].push_back(DistColor{nnResult.dist(0), color});
    }

    for (const auto& pointInfo : smoothed_geo_points) {
        auto& colorsDists2 = refinedColorsDists2[pointInfo.pointIndex];

        if (colorsDists2.empty() /* || losslessAttribute */) {
            continue;
        }
        if (colorsDists2.size() == 1) {
            targetAttrs[pointInfo.pointIndex] = colorsDists2[0].color;
            continue;
        } 
        Vector3<double> centroid2(0.0);
        double sumWeights{0.0};
        for (auto& colorDist2 : colorsDists2) {
            const double weight = 1 / (sqrt(colorDist2.dist) + distOffsetBwd);
            for (size_t k = 0; k < 3; k++) {
                centroid2[k] += (colorDist2.color[k] * weight);
            }
            sumWeights += weight;
        }
        centroid2 /= sumWeights;
        for (size_t k = 0; k < 3; k++) {
            targetAttrs[pointInfo.pointIndex][k] = uint16_t(PCCClip(round(centroid2[k]), 0.0, 65535.0));
        }
    }
}

void transferColors16bitBP_fast_correct2(
    std::shared_ptr<uvgvpcc_dec::Frame> &frame,
    std::vector<smoothedGeoInfo>& smoothed_geo_points
) {
    constexpr int numNeighborsColorTransferFwd = 8;
    constexpr int numNeighborsColorTransferBwd = 1;
    constexpr double distOffsetFwd = 4.0;
    constexpr double distOffsetBwd = 4.0;

    std::vector<std::vector<nnPointInfo>> smoothedPointNeighbors;

    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>>& sourceGeos = frame->pointsGeometry;
    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeAttributeInput16bit>>& sourceAttrs = frame->pointsAttribute16bits;

    pccNNResult nnResult;

    // ==========================================================================================
    //                                     Forward direction
    // ==========================================================================================

    smoothedPointNeighbors.resize(smoothed_geo_points.size());

    pccKdTree kdTreeSource(sourceGeos);
    
    for (size_t index = 0; index < smoothed_geo_points.size(); index++) {
        auto& pointInfo = smoothed_geo_points[index];
        kdTreeSource.search(pointInfo.point, numNeighborsColorTransferFwd, nnResult);

        smoothedPointNeighbors[index].resize(numNeighborsColorTransferFwd);
        for (size_t i = 0; i < numNeighborsColorTransferFwd; i++) {
            const size_t sourceIndex = nnResult.indices(i);
            smoothedPointNeighbors[index][i].point      = sourceGeos[sourceIndex];
            smoothedPointNeighbors[index][i].color      = sourceAttrs[sourceIndex];
            smoothedPointNeighbors[index][i].pointIndex = sourceIndex;
            smoothedPointNeighbors[index][i].dist       = nnResult.dist(i);
        }
    }

    // ==========================================================================================
    //                                  Backward direction
    // ==========================================================================================

    // Replace points of source geometry with smoothed geometry points
    for(const auto& pointInfo : smoothed_geo_points) {
        sourceGeos[pointInfo.pointIndex] = pointInfo.point;
    }

    pccKdTree kdtreeTarget(sourceGeos);

    std::vector<smoothedColorInfo> refinedColorsDists2;
    refinedColorsDists2.resize(sourceGeos.size());

    for (const auto& smoothedPointNN : smoothedPointNeighbors) {
        for (size_t i = 0; i < numNeighborsColorTransferFwd; i++) {
            const Vector3<typeAttributeInput16bit>& color = smoothedPointNN[i].color;
            kdtreeTarget.search(smoothedPointNN[i].point, numNeighborsColorTransferBwd, nnResult);
            const size_t pointIndex = nnResult.indices(0);

            if (std::abs(color[0] - sourceAttrs[pointIndex][0]) >= 40) continue;
            if (std::abs(color[1] - sourceAttrs[pointIndex][1]) >= 40) continue;
            if (std::abs(color[2] - sourceAttrs[pointIndex][2]) >= 40) continue;

            auto& smoothedColorInfo = refinedColorsDists2[pointIndex]; 
            if (smoothedColorInfo.count == 0) {
                smoothedColorInfo.firstColor = color;
            }
            const double weight = 1 / (sqrt(nnResult.dist(0)) + distOffsetBwd);
            for (size_t k = 0; k < 3; k++) {
                smoothedColorInfo.centroid2[k] += (color[k] * weight);
            }
            smoothedColorInfo.sumWeights += weight;
            smoothedColorInfo.count++;
        }
    }

    for (size_t index = 0; index < smoothed_geo_points.size(); index++) {
        auto& pointInfo = smoothed_geo_points[index];
        auto& colorsDists2 = refinedColorsDists2[pointInfo.pointIndex];
        
        const auto& nnPointInfo = smoothedPointNeighbors[index]; 
        if (colorsDists2.count == 0 /* || losslessAttribute */) {
            if (nnPointInfo[0].dist < 0.0001) {
                sourceAttrs[pointInfo.pointIndex] = nnPointInfo[0].color;
                continue;
            }

            Vector3<double> refinedColor(0.0);
            double sumWeights{0.0};
            for (int i = 0; i < 8; i++) {
                const double weight = 1 / (nnPointInfo[i].dist + distOffsetFwd);
                for (int k = 0; k < 3; k++) {
                    refinedColor[k] += nnPointInfo[i].color[k] * weight;
                }
                sumWeights += weight;
            }   
            refinedColor /= sumWeights;
            for (int k = 0; k < 3; k++) {
                sourceAttrs[pointInfo.pointIndex][k] = uint16_t(PCCClip(round(refinedColor[k]), 0.0, 65535.0));
            }
            continue;
        }
        if (colorsDists2.count == 1) {
            sourceAttrs[pointInfo.pointIndex] = colorsDists2.firstColor;
            continue;
        }
        colorsDists2.centroid2 /= colorsDists2.sumWeights;
        for (size_t k = 0; k < 3; k++) {
            sourceAttrs[pointInfo.pointIndex][k] = uint16_t(PCCClip(round(colorsDists2.centroid2[k]), 0.0, 65535.0));
        }
    }
}

void transferColors16bitBP_fast(
    std::shared_ptr<uvgvpcc_dec::Frame> &frame,
    std::vector<smoothedGeoInfo>& smoothed_geo_points
) {
    constexpr int numNeighborsColorTransferFwd = 8;
    constexpr int numNeighborsColorTransferBwd = 1;
    constexpr double distOffsetFwd = 4.0;
    constexpr double distOffsetBwd = 4.0;

    std::vector<Vector3<typeGeometryInput>> partSourceGeo;
    std::vector<size_t> partSourceAttr_indices;

    std::vector<Vector3<typeGeometryInput>>& sourceGeos = frame->pointsGeometry;
    std::vector<Vector3<typeAttributeInput16bit>>& sourceAttrs = frame->pointsAttribute16bits;

    pccNNResult nnResult;

    // ==========================================================================================
    //                                     Forward direction
    // ==========================================================================================
    partSourceGeo.reserve(smoothed_geo_points.size() * numNeighborsColorTransferFwd);
    partSourceAttr_indices.reserve(smoothed_geo_points.size() * numNeighborsColorTransferFwd);

    pccKdTree kdTreeSource(sourceGeos);
    for(auto& pointInfo : smoothed_geo_points) {
        kdTreeSource.search(pointInfo.point, numNeighborsColorTransferFwd, nnResult);

        for (size_t rI = 0; rI < numNeighborsColorTransferFwd; rI++) {
            auto indexInSource = nnResult.indices(rI);
            partSourceGeo.push_back(sourceGeos[indexInSource]);
            partSourceAttr_indices.push_back(indexInSource);
        }
        
        if (nnResult.dist(0) < 0.0001) {
            pointInfo.color = sourceAttrs[nnResult.indices(0)];
            continue;
        }

        Vector3<double> refinedColor(0.0);
        double sumWeights{0.0};
        for (int i = 0; i < numNeighborsColorTransferFwd; i++) {
            const double weight = 1 / (nnResult.dist(i) + distOffsetFwd);
            for (int k = 0; k < 3; k++) {
                refinedColor[k] += sourceAttrs[nnResult.indices(i)][k] * weight;
            }
            sumWeights += weight;
        }   
        refinedColor /= sumWeights;
        for (int k = 0; k < 3; k++) {
            pointInfo.color[k] = uint16_t(PCCClip(round(refinedColor[k]), 0.0, 65535.0));
        }
    }

    // ==========================================================================================
    //                                  Backward direction
    // ==========================================================================================

    // Replace points of source geometry with smoothed geometry points
    for(const auto& pointInfo : smoothed_geo_points) {
        sourceGeos[pointInfo.pointIndex] = pointInfo.point;
    }

    pccKdTree kdtreeTarget(sourceGeos);

    std::vector<std::vector<DistColor>> refinedColorsDists2;
    refinedColorsDists2.resize(sourceGeos.size());
    for (size_t index = 0; index < partSourceGeo.size(); index++) {
        const Vector3<typeAttributeInput16bit> color = sourceAttrs[partSourceAttr_indices[index]];
        kdtreeTarget.search(partSourceGeo[index], numNeighborsColorTransferBwd, nnResult);
        const size_t pointIndex = nnResult.indices(0);
        if (std::abs(color[0] - sourceAttrs[pointIndex][0]) >= 40) continue;
        if (std::abs(color[1] - sourceAttrs[pointIndex][1]) >= 40) continue;
        if (std::abs(color[2] - sourceAttrs[pointIndex][2]) >= 40) continue;
        refinedColorsDists2[pointIndex].push_back(DistColor{nnResult.dist(0), color});
        
        // if (
        //     std::abs(color[0] - sourceAttrs[pointIndex][0]) < 40 &&
        //     std::abs(color[1] - sourceAttrs[pointIndex][1]) < 40 &&
        //     std::abs(color[2] - sourceAttrs[pointIndex][2]) < 40 
        // ) {
        //     refinedColorsDists2[pointIndex].push_back( 
        //         DistColor{nnResult.dist(0), color}
        //     );
        // }
    }

    // Sort refinedColorsDists2 according to distance
    for (auto& refinedColorsDist2: refinedColorsDists2) {
        std::sort(
            refinedColorsDist2.begin(), 
            refinedColorsDist2.end(), 
            [](DistColor& dc1, DistColor& dc2) {
                return dc1.dist < dc2.dist;
            }
        );
    }
    
    for (const auto& pointInfo : smoothed_geo_points) {
        auto& colorsDists2 = refinedColorsDists2[pointInfo.pointIndex];

        if (colorsDists2.empty() /* || losslessAttribute */) {
            sourceAttrs[pointInfo.pointIndex] = pointInfo.color;
            continue;
        }
        if (colorsDists2.size() == 1) {
            sourceAttrs[pointInfo.pointIndex] = colorsDists2[0].color;
        } else {
            Vector3<double> centroid2(0.0);
            double sumWeights{0.0};
            for (auto& colorDist2 : colorsDists2) {
                const double weight = 1 / (sqrt(colorDist2.dist) + distOffsetBwd);
                for (size_t k = 0; k < 3; k++) {
                    centroid2[k] += (colorDist2.color[k] * weight);
                }
                sumWeights += weight;
            }
            centroid2 /= sumWeights;
            for (size_t k = 0; k < 3; k++) {
                sourceAttrs[pointInfo.pointIndex][k] = uint16_t(PCCClip(round(centroid2[k]), 0.0, 65535.0));
            }
        }
    }
}

void transferColors16bitBP_fast__(
    std::shared_ptr<uvgvpcc_dec::Frame> &frame,
    std::vector<smoothedGeoInfo>& smoothed_geo_points
) {
    constexpr int numNeighborsColorTransferFwd = 8;
    constexpr int numNeighborsColorTransferBwd = 1;
    constexpr double distOffsetFwd = 4.0;
    constexpr double distOffsetBwd = 4.0;
    constexpr double maxGeometryDist2Fwd = std::numeric_limits<double>::max();
    constexpr double maxGeometryDist2Bwd = std::numeric_limits<double>::max();
    constexpr double maxColorDist2Fwd    = std::numeric_limits<double>::max();
    constexpr double maxColorDist2Bwd    = std::numeric_limits<double>::max();

    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>> partSourceGeo;
    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeAttributeInput16bit>> partSourceAttr;

    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>>& sourceGeos = frame->pointsGeometry;
    std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeAttributeInput16bit>>& sourceAttrs = frame->pointsAttribute16bits;
     
    // std::vector<uvgvpcc_dec::Vector3<uvgvpcc_dec::typeAttributeInput16bit>> refinedColors1;
    // refinedColors1.resize(frame.pointsAttribute16bits.size());
    std::vector<smoothedGeoInfo> refinedColors1;
    refinedColors1.reserve(smoothed_geo_points.size());

    pccNNResult nnResult;

    // ==========================================================================================
    //                                     Forward direction
    // ==========================================================================================
    pccKdTree kdTreeSource(sourceGeos);
    for(auto& pointInfo : smoothed_geo_points) {
        refinedColors1.push_back({sourceAttrs[pointInfo.pointIndex], pointInfo.pointIndex});
        kdTreeSource.search(pointInfo.point, numNeighborsColorTransferFwd, nnResult);

        for (size_t rI = 0; rI < nnResult.size(); rI++) {
            auto indexInSource = nnResult.indices(rI);
            partSourceGeo.push_back(sourceGeos[indexInSource]);
            partSourceAttr.push_back(sourceAttrs[indexInSource]);
        }
        
        if (nnResult.dist(0) < 0.0001) {
            refinedColors1.back().point = sourceAttrs[nnResult.indices(0)];
            continue;
        }

        // If !isDone
        int nNN = static_cast<int>(nnResult.count());
        // printf("---------->nNN: %d\n", nNN);
        if (nNN == 1) {
            refinedColors1.back().point = sourceAttrs[nnResult.indices(0)];
            continue;
        }
        // If !isDone
        Vector3<double> refinedColor(0.0);
        double sumWeights{0.0};
        for (int i = 0; i < nNN; i++) {
            const double weight = 1 / (nnResult.dist(i) + distOffsetFwd);
            for (int k = 0; k < 3; k++) {
                refinedColor[k] += sourceAttrs[nnResult.indices(i)][k] * weight;
            }
            sumWeights += weight;
        }   
        refinedColor /= sumWeights;
        for (int k = 0; k < 3; k++) {
            refinedColors1.back().point[k] = PCCClip(round(refinedColor[k]), 0.0, 65535.0);
        }
    }

    // ==========================================================================================
    //                                  Backward direction
    // ==========================================================================================

    // Replace points of source geometry with smoothed geometry points
    for(auto& pointInfo : smoothed_geo_points) {
        sourceGeos[pointInfo.pointIndex] = pointInfo.point;
    }

    pccKdTree kdtreeTarget(sourceGeos);

    std::vector<std::vector<DistColor>> refinedColorsDists2;
    refinedColorsDists2.resize(sourceGeos.size());
    for (size_t index = 0; index < partSourceGeo.size(); index++) {
        kdtreeTarget.search(partSourceGeo[index], numNeighborsColorTransferBwd, nnResult);
        for (int i = 0; i < nnResult.size(); i++) {
            const Vector3<typeAttributeInput16bit> color = partSourceAttr[index];
            const size_t pointIndex = nnResult.indices(i);
            if (
                std::abs(color[0] - sourceAttrs[pointIndex][0]) < 40 &&
                std::abs(color[1] - sourceAttrs[pointIndex][1]) < 40 &&
                std::abs(color[2] - sourceAttrs[pointIndex][2]) < 40 
            ) {
                refinedColorsDists2[nnResult.indices(i)].push_back( 
                    DistColor{nnResult.dist(i), color}
                );
            }
        }
    }
    // Sort refinedColorsDists2 according to distance
    for (auto& refinedColorsDist2: refinedColorsDists2) {
        std::sort(
            refinedColorsDist2.begin(), 
            refinedColorsDist2.end(), 
            [](DistColor& dc1, DistColor& dc2) {
                return dc1.dist < dc2.dist;
            }
        );
    }

    const double eps = 0.000001;
    // const double rSource  = 1.0 / double( sourceGeos.size() );
    // const double rTarget  = 1.0 / double( sourceGeos.size() );
    // const double maxValue = std::numeric_limits<uint16_t>::max();

    for (const auto& color1: refinedColors1) {
        auto& colorsDists2 = refinedColorsDists2[color1.pointIndex];

        if (colorsDists2.empty() /* || losslessAttribute */) {
            sourceAttrs[color1.pointIndex] = color1.point;
            continue;
        }

        const Vector3<double> centroid1(color1.point);
        Vector3<double> centroid2(0.0);

        int nNN = static_cast<int>(colorsDists2.size());
        // printf("---------->nNN: %d\n", nNN);
        if (nNN == 1) {
            centroid2 = colorsDists2[0].color;
        } else if (nNN > 1) {
            centroid2 = 0;
            double sumWeights{0.0};
            for (auto& colorDist2 : colorsDists2) {
                const double weight = 1 / (sqrt(colorDist2.dist) + distOffsetBwd);
                for (size_t k = 0; k < 3; k++) {
                    centroid2[k] += colorDist2.color[k] * weight;
                }
                sumWeights += weight;
            }
            centroid2 /= sumWeights;
        }

        const double delta2 = (centroid2 - centroid1).norm2();
        if (delta2 > eps) {
            Vector3<double> bestColor;
            for (size_t k = 0; k < 3; k++) {
                bestColor[k] = PCCClip(round(centroid2[k]), 0.0, 65535.0);
            }

            // double minError = std::numeric_limits<double>::max();
            // double e1 = 0.0;
            // for (size_t k = 0; k < 3; k++) {
            //     const double d = color[k] - color1.point[k];
            //     e1 += d * d;
            // }
            // e1 *= rTarget;
            
            // double e2 = 0.0;
            // for (const auto& colorsDist2 : colorsDists2) {
            //     auto color2 = colorsDist2.color;
            //     for (size_t k = 0; k < 3; k++) {
            //         const double d = color[k] - color2[k];
            //         e2 += d * d;
            //     }    
            // }
            // e2 *= rSource;

            // const double error = std::max(e1, e2);
            // if (error < minError)

            sourceAttrs[color1.pointIndex] = bestColor;
        }

    }

}