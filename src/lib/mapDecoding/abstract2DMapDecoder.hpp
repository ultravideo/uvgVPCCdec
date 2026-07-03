#pragma once

/// \file Abstract class defining the behaviour of any 2D encoder to be used within the uvgVPCCdec library. 

#include "uvgvpcc/uvgvpcc.hpp"

enum DECODER_TYPE {OCCUPANCY, GEOMETRY, ATTRIBUTE};

// All 2D encoder should derived from this class. Notice that there is one 2D encoder for each map (occupancy, geometry and attribute). Static functions can't be overrided. For example, the handling of the function pointer is not done by the derived class, as it should always be the same whatever the 2D encoder used.
class Abstract2DMapDecoder {
public:
    Abstract2DMapDecoder(const DECODER_TYPE& decoderType): decoderType_(decoderType) {};
    virtual ~Abstract2DMapDecoder() = default;
    
    virtual void decodeGOFMaps(const std::shared_ptr<uvgvpcc_dec::GOF>& gof) = 0;

protected:
    const DECODER_TYPE decoderType_;
};
