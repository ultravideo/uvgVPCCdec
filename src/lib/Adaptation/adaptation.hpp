#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Adaptation {
public:
    static void convertYUV8ToRGB8(point_cloud_frame* reconstruct);
};