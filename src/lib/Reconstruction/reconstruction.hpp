#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Reconstruction {
public: 
    static void reconstructPointCloud(decompressed_data* data);

};


/*
a 4Darray occFramesNF[ compTimeIdx ][ 0 ][ y ][ x ] specifying the decoded occupancy frames in the
nominal format, where y is in the range of 0 to asps_frame_height – 1, inclusive, and x is in the range
of 0 to asps_frame_width – 1, inclusive
*/