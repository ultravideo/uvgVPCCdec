#include "sei.hpp"

// H.20.2.18 Occupancy synthesis SEI message syntax
void occupancySynthesis(bitstream_t* stream, sei& seiAbstract) {
    auto& sei = static_cast<sei_occupancy_synthesis&>(seiAbstract);
    sei.persistenceFlag  = bitstream_read(stream, 1);
    sei.resetFlag        = bitstream_read(stream, 1);
    sei.instancesUpdated = bitstream_read(stream, 8);
    sei.allocate();
    for (size_t i = 0; i < sei.instancesUpdated; i++) {
        sei.instanceIndices.at(i)              = bitstream_read(stream, 8);
        size_t k = sei.instanceIndices.at(i);
        sei.instanceCancelFlags.at(k)          = bitstream_read(stream, 1);
        if (!sei.instanceCancelFlags.at(k)) {
            sei.methodTypes.at(k)              = bitstream_read_ue(stream);
            if (sei.methodTypes.at(k)) {
                sei.pbfLog2ThresholdMinus1.at(k) = bitstream_read(stream, 2);
                sei.pbfPassesCountMinus1.at(k)   = bitstream_read(stream, 2);
                sei.pbfFilterSizeMinus1.at(k)    = bitstream_read(stream, 3);
            }
        }
    }
}

// H.20.2.19 Geometry smoothing SEI message syntax
void geometrySmoothing(bitstream_t* stream, sei& seiAbstract) {
    auto& sei = static_cast<sei_geometry_smoothing&>(seiAbstract);
    sei.persistenceFlag  = bitstream_read(stream, 1);
    sei.resetFlag        = bitstream_read(stream, 1);
    sei.instancesUpdated = bitstream_read(stream, 8);
    sei.allocate();
    for (size_t i = 0; i < sei.instancesUpdated; i++) {
        sei.instanceIndices.at(i)              = bitstream_read(stream, 8);
        size_t k = sei.instanceIndices.at(i);
        sei.instanceCancelFlags.at(k)          = bitstream_read(stream, 1);
        if (!sei.instanceCancelFlags.at(k)) {
            sei.methodTypes.at(k)              = bitstream_read_ue(stream);
            if (sei.methodTypes.at(k) == 1) {
                sei.filterEomPointsFlags.at(k) = bitstream_read(stream, 1);
                sei.gridSizeMinus2s.at(k)      = bitstream_read(stream, 7);
                sei.thresholds.at(k)           = bitstream_read(stream, 8);
            }
        }
    }
    printf("geometrySmoothing Done, bytes = %d, bits = %d\n", stream->len, stream->cur_bit);
}

// F.2.16 Decoded Atlas Information Hash SEI message syntax
void decodedAtlasInformationHash(bitstream_t* stream, sei& seiAbstract) {
    auto& sei = static_cast<decode_atlas_information_hash&>(seiAbstract);
    // printf( "-------> Bitsream -> current byte: %d current bits: %d \n", stream->len, stream->cur_bit );
    sei.cancelFlag = bitstream_read(stream, 1);
    if (sei.cancelFlag) {
        return;
    }
    sei.persistenceFlag                     = bitstream_read(stream, 1);
    sei.hashType                            = bitstream_read(stream, 8);
    sei.decodedHighLevelHashPresentFlag     = bitstream_read(stream, 1);
    sei.decodedAtlasHashPresentFlag         = bitstream_read(stream, 1);
    sei.decodedAtlasB2pHashPresentFlag      = bitstream_read(stream, 1);
    sei.decodedAtlasTilesHashPresentFlag    = bitstream_read(stream, 1);
    sei.decodedAtlasTilesB2pHashPresentFlag = bitstream_read(stream, 1);
    bitstream_advance(stream, 1);
    // printf( "-------> Bitsream -> current byte: %d current bits: %d \n", stream->len, stream->cur_bit );

    if (sei.decodedHighLevelHashPresentFlag) {
        if (sei.hashType == 0) {
            for (size_t i = 0; i < 16; i++) {
                sei.highLevelMd5[i] = bitstream_read(stream, 8);
            } 
        } else if (sei.hashType == 1) {
            sei.highLevelCrc = bitstream_read(stream, 16);
        } else if (sei.hashType == 2) {
            sei.highLevelChecksum = bitstream_read(stream, 32);
        }
    }
    // printf( "-------> Bitsream -> current byte: %d current bits: %d \n", stream->len, stream->cur_bit );
    if (sei.decodedAtlasHashPresentFlag) {
        if (sei.hashType == 0) {
            for (size_t i = 0; i < 16; i++) {
                sei.atlasMd5[i] = bitstream_read(stream, 8);
            } 
        } else if (sei.hashType == 1) {
            sei.atlasCrc = bitstream_read(stream, 16);
        } else if (sei.hashType == 2) {
            sei.atlasChecksum = bitstream_read(stream, 32);
        }
    }
    // printf( "-------> Bitsream -> current byte: %d current bits: %d \n", stream->len, stream->cur_bit );
    if (sei.decodedAtlasB2pHashPresentFlag) {
        if (sei.hashType == 0) {
            for (size_t i = 0; i < 16; i++) {
                sei.atlasB2pMd5[i] = bitstream_read(stream, 8);
            } 
        } else if (sei.hashType == 1) {
            sei.atlasB2pCrc = bitstream_read(stream, 16);
        } else if (sei.hashType == 2) {
            sei.atlasB2pChecksum = bitstream_read(stream, 32);
        }
    }
    // printf( "-------> Bitsream -> current byte: %d current bits: %d \n", stream->len, stream->cur_bit );
    if (sei.decodedAtlasTilesHashPresentFlag || sei.decodedAtlasTilesB2pHashPresentFlag) {
        sei.numTilesMinus1  = bitstream_read_ue(stream);
        sei.tileIdLenMinus1 = bitstream_read_ue(stream);
        size_t numTiles = sei.numTilesMinus1 + 1;
        sei.tileIds.resize(numTiles);
        sei.atlasTilesMd5.resize(numTiles);
        sei.atlasTilesCrc.resize(numTiles);
        sei.atlasTilesChecksum.resize(numTiles);
        sei.atlasTilesB2pMd5.resize(numTiles);
        sei.atlasTilesB2pCrc.resize(numTiles);
        sei.atlasTilesB2pChecksum.resize(numTiles);
        for (size_t t = 0; t <= sei.numTilesMinus1; t++) {
            sei.tileIds[t] = bitstream_read(stream, sei.tileIdLenMinus1 + 1);
            sei.atlasTilesMd5[t].resize(16);
            sei.atlasTilesB2pMd5[t].resize(16);
        }
        // printf( "-------> Before: Bitsream -> bitstream_align current byte: %d current bits: %d \n", stream->len, stream->cur_bit );
        bitstream_align(stream);
        // printf( "-------> After: Bitsream -> bitstream_align current byte: %d current bits: %d \n", stream->len, stream->cur_bit );
        for (size_t t = 0; t <= sei.numTilesMinus1; t++) {
            size_t i = sei.tileIds[t];
            if (sei.decodedAtlasTilesHashPresentFlag) {
                if (sei.hashType == 0) {
                    for (size_t j = 0; j < 16; j++) {
                        sei.atlasTilesMd5[i][j] = bitstream_read(stream, 8);
                    } 
                } else if (sei.hashType == 1) {
                    sei.atlasTilesCrc[i] = bitstream_read(stream, 16);
                } else if (sei.hashType == 2) {
                    sei.atlasTilesChecksum[i] = bitstream_read(stream, 32);
                }
            }
            if (sei.decodedAtlasTilesB2pHashPresentFlag) {
                if (sei.hashType == 0) {
                    for (size_t j = 0; j < 16; j++) {
                        sei.atlasTilesB2pMd5[i][j] = bitstream_read(stream, 8);
                    } 
                } else if (sei.hashType == 1) {
                    sei.atlasTilesB2pCrc[i] = bitstream_read(stream, 16);
                } else if (sei.hashType == 2) {
                    sei.atlasTilesB2pChecksum[i] = bitstream_read(stream, 32);
                }
            }
        }
    }
}

void read_prefix_sei_rbsp(bitstream_t* stream, NAL_UNIT_TYPE nal_unit_type, pccsei& seis) {
    do {
        int32_t payload_type = 0;
        int32_t payload_size = 0;
        int32_t byte         = 0;
        do {
            byte = bitstream_read(stream, 8);  // u(8)
            payload_type += byte;
        } while ( byte == 0xff );
        do {
            byte = bitstream_read(stream, 8);  // u(8)
            payload_size += byte;
        } while ( byte == 0xff );

        // SEI& sei = seiList.addSei( nalUnitType, payload_type );
        printf( "        seiMessage: type = %d %s payloadSize = %zu \n", payload_type, toString( static_cast<SEI_PAYLOAD_TYPE>(payload_type) ).c_str(), (size_t)payload_size );
        sei& sei_ = seis.addSei(nal_unit_type, static_cast<SEI_PAYLOAD_TYPE>(payload_type));
        if ( payload_size == BUFFERING_PERIOD ) {  // 0
            // bufferingPeriod( bitstream, sei );
        } else if ( payload_size == ATLAS_FRAME_TIMING ) {  // 1
            // assert( seiList.seiIsPresent( NAL_PREFIX_NSEI, BUFFERING_PERIOD ) );
            // auto& bpsei = *seiList.getLastSei( NAL_PREFIX_NSEI, BUFFERING_PERIOD );
            // atlasFrameTiming( bitstream, sei, bpsei, false );
        } else if ( payload_type == FILLER_PAYLOAD ) {  // 2
            // fillerPayload( bitstream, sei, payloadSize );
        } else if ( payload_type == USER_DATAREGISTERED_ITUTT35 ) {  // 3
            // userDataRegisteredItuTT35( bitstream, sei, payloadSize );
        } else if ( payload_type == USER_DATA_UNREGISTERED ) {  // 4
            // userDataUnregistered( bitstream, sei, payloadSize );
        } else if ( payload_type == RECOVERY_POINT ) {  // 5
            // recoveryPoint( bitstream, sei );
        } else if ( payload_type == NO_RECONSTRUCTION ) {  // 6
            // noReconstruction( bitstream, sei );
        } else if ( payload_type == TIME_CODE ) {  // 7
            // timeCode( bitstream, sei );
        } else if ( payload_type == SEI_MANIFEST ) {  // 8
            // seiManifest( bitstream, sei );
        } else if ( payload_type == SEI_PREFIX_INDICATION ) {  // 9
            // seiPrefixIndication( bitstream, sei );
        } else if ( payload_type == ACTIVE_SUB_BITSTREAMS ) {  // 10
            // activeSubBitstreams( bitstream, sei );
        } else if ( payload_type == COMPONENT_CODEC_MAPPING ) {  // 11
            // componentCodecMapping( bitstream, sei );
        } else if ( payload_type == SCENE_OBJECT_INFORMATION ) {  // 12
            // sceneObjectInformation( bitstream, sei );
        } else if ( payload_type == OBJECT_LABEL_INFORMATION ) {  // 13
            // objectLabelInformation( bitstream, sei );
        } else if ( payload_type == PATCH_INFORMATION ) {  // 14
            // patchInformation( bitstream, sei );
        } else if ( payload_type == VOLUMETRIC_RECTANGLE_INFORMATION ) {  // 15
            // volumetricRectangleInformation( bitstream, sei );
        } else if ( payload_type == ATLAS_OBJECT_INFORMATION ) {  // 16
            // atlasObjectInformation( bitstream, sei );
        } else if ( payload_type == VIEWPORT_CAMERA_PARAMETERS ) {  // 17
            // viewportCameraParameters( bitstream, sei );
        } else if ( payload_type == VIEWPORT_POSITION ) {  // 18
            // viewportPosition( bitstream, sei );
        } else if ( payload_type == ATTRIBUTE_TRANSFORMATION_PARAMS ) {  // 64
            // attributeTransformationParams( bitstream, sei );
        } else if ( payload_type == OCCUPANCY_SYNTHESIS ) {  // 65
            occupancySynthesis(stream, sei_);
        } else if ( payload_type == GEOMETRY_SMOOTHING ) {  // 66
            geometrySmoothing(stream, sei_);
        } else if ( payload_type == ATTRIBUTE_SMOOTHING ) {  // 67
            // attributeSmoothing( bitstream, sei );
        } else {
            // reservedSeiMessage( bitstream, sei, payloadSize );
        }
        bool moredata = !(stream->cur_bit == 0);
        if (moredata) {
            // if ( payloadExtensionPresent( bitstream ) ) {
            //     bitstream.read( 1 );  // u(v)
            // }

            // printf("Before bitstream_align, current bytes: %d, current bits: %d\n", stream->len, stream->cur_bit);
            bitstream_align(stream);
            // printf("After bitstream_align, current bytes: %d, current bits: %d\n", stream->len, stream->cur_bit);
        }
    } while(moreRbspData(stream));
    bitstream_align_rbsp_trailing_bits(stream);
}


void read_suffix_sei_rbsp(bitstream_t* stream, NAL_UNIT_TYPE nal_unit_type, pccsei& seis) {
    do {
        int32_t payload_type = 0;
        int32_t payload_size = 0;
        int32_t byte        = 0;
        do {
            byte = bitstream_read(stream, 8);  // u(8)
            payload_type += byte;
        } while ( byte == 0xff );
        do {
            byte = bitstream_read(stream, 8);  // u(8)
            payload_size += byte;
        } while ( byte == 0xff );
        
        // SEI& sei = seiList.addSei( nalUnitType, payload_type );
         printf( "        seiMessage: type = %d %s payloadSize = %zu \n", payload_type, toString( static_cast<SEI_PAYLOAD_TYPE>(payload_type) ).c_str(), (size_t)payload_size );
        sei& sei_ = seis.addSei(nal_unit_type, static_cast<SEI_PAYLOAD_TYPE>(payload_type));
        if ( payload_type == FILLER_PAYLOAD ) {  // 2
            // fillerPayload( bitstream, sei, payloadSize );
        } else if ( payload_type == USER_DATAREGISTERED_ITUTT35 ) {  // 3
            // userDataRegisteredItuTT35( bitstream, sei, payloadSize );
        } else if ( payload_type == USER_DATA_UNREGISTERED ) {  // 4
            // userDataUnregistered( bitstream, sei, payloadSize );
        } else if ( payload_type == DECODED_ATLAS_INFORMATION_HASH ) {  // 19
            decodedAtlasInformationHash(stream, sei_);
        } else {
            // reservedSeiMessage( bitstream, sei, payloadSize );
        }

        if (!(stream->cur_bit == 0)) {
            // if ( payloadExtensionPresent( bitstream ) ) {
            //     bitstream.read( 1 );  // u(v)
            // }
            // printf("Before bitstream_align, current bytes: %d, current bits: %d\n", stream->len, stream->cur_bit);
            bitstream_align(stream);
            // printf("After bitstream_align, current bytes: %d, current bits: %d\n", stream->len, stream->cur_bit);
        }

    } while(moreRbspData(stream));
    bitstream_align_rbsp_trailing_bits(stream);
}