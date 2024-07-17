#include "reconstruction.hpp"

using namespace uvgvpcc_dec;

/*


a 4Darray occFramesNF[ compTimeIdx ][ 0 ][ y ][ x ] specifying the decoded occupancy frames in the
nominal format, where y is in the range of 0 to asps_frame_height – 1, inclusive, and x is in the range
of 0 to asps_frame_width – 1, inclusive

a 5D array geoFramesNF[ mapIdx ][ compTimeIdx ][ 0 ][ y ][ x ] specifying the decoded geometry
frames in the nominal format, where mapIdx is in the range of 0 to asps_map_count_minus1,
inclusive, y is in the range of 0 to asps_frame_height – 1, inclusive, and x is in the range of 0 to
asps_frame_width – 1, inclusive,
*/

/*
void PCCCodec::generatePointCloud( PCCPointSet3&                       reconstruct,
                                   PCCContext&                         context,
                                   size_t                              frameIndex,
                                   size_t                              tileIndex,
                                   const GeneratePointCloudParameters& params,
                                   std::vector<uint32_t>&              partition,
                                   bool                                bDecoder )

                                   class PCCPointSet3 {
                                     std::vector<PCCPoint3D>                positions_;
  std::vector<PCCColor3B>                colors_;
  std::vector<PCCColor16bit>             colors16bit_;
  std::vector<uint16_t>                  reflectances_;
  std::vector<uint16_t>                  boundaryPointTypes_;
  std::vector<std::pair<size_t, size_t>> pointPatchIndexes_;
  std::vector<uint64_t>                  parentPointIndex_;
  std::vector<uint8_t>                   types_;
  std::vector<PCCNormal3D>               normals_;
  bool                                   withNormals_;
  bool                                   withColors_;
  bool                                   withReflectances_;
*/
void Reconstruction::reconstructPointCloud(decompressed_data* data, point_set* reconstruct)
{
    video_map occFrames = data->occupancy_map;

    auto b = reconstruct->points.size();

    auto i = data->asps.asps_frame_height;
}