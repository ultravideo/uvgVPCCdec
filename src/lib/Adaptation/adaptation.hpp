#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Adaptation {
public:
    /* ------------------------ ripped from tmc2------------------------ */
    static void convertYUV8ToRGB8(point_cloud_frame* reconstruct);

    /* ------------------------ ripped from tmc2------------------------ */
    static bool write( const std::string& fileName, point_cloud_frame* frame, const bool asAscii = true );
};