#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Reconstruction {
public: 
    static void reconstructPointCloud(decompressed_data* data, point_set* reconstruct);

};