#include "bitstream_common.hpp"
#include "bitstream_util.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

// class sei {
// public:
//     size_t payload_size = 0;
//     std::vector<uint8_t> data; // byteStrData_
//     SEI_PAYLOAD_TYPE payload_type;
// };

// class sei_geometry_smoothing : public sei {
// public:
//     bool persistenceFlag;
//     bool resetFlag;
//     uint8_t instancesUpdated;
//     std::vector<uint8_t> instanceIndices;
//     std::vector<bool> instanceCancelFlags;
//     std::vector<uint8_t> methodTypes;
//     std::vector<bool> filterEomPointsFlags;
//     std::vector<uint8_t> gridSizeMinus2s;
//     std::vector<uint8_t> thresholds;

//     sei_geometry_smoothing() : persistenceFlag(false), resetFlag(false), instancesUpdated(0) {
//         payload_type = GEOMETRY_SMOOTHING;

//     };

//     void allocate() {
//         instanceIndices.resize(instancesUpdated, 0);
//         instanceCancelFlags.resize(instancesUpdated, false);
//         methodTypes.resize(instancesUpdated, 0);
//         filterEomPointsFlags.resize(instancesUpdated, false);
//         gridSizeMinus2s.resize(instancesUpdated, 0);
//         thresholds.resize(instancesUpdated, 0);
//     }
// };


struct sei {
    virtual ~sei() = default;

    std::vector<uint8_t> data; // byteStrData_
    size_t payload_size = 0;
    SEI_PAYLOAD_TYPE payload_type;
};


struct sei_occupancy_synthesis : sei{
    bool persistenceFlag;
    bool resetFlag;
    uint8_t instancesUpdated;
    std::vector<uint8_t> instanceIndices;
    std::vector<bool> instanceCancelFlags;
    std::vector<uint8_t> methodTypes;
    std::vector<uint8_t> pbfLog2ThresholdMinus1;
    std::vector<uint8_t> pbfPassesCountMinus1;
    std::vector<uint8_t> pbfFilterSizeMinus1;

    sei_occupancy_synthesis(): persistenceFlag(false), resetFlag(false), instancesUpdated(0) {
        payload_type = OCCUPANCY_SYNTHESIS;
    };

    void allocate() {
        instanceIndices.resize( instancesUpdated, 0);
        instanceCancelFlags.resize( instancesUpdated, false);
        methodTypes.resize( instancesUpdated, 0);
        pbfLog2ThresholdMinus1.resize( instancesUpdated, 0);
        pbfPassesCountMinus1.resize( instancesUpdated, 0);
        pbfFilterSizeMinus1.resize( instancesUpdated, 0);
    }
};

struct sei_geometry_smoothing : sei {
    bool persistenceFlag;
    bool resetFlag;
    uint8_t instancesUpdated;
    std::vector<uint8_t> instanceIndices;
    std::vector<bool> instanceCancelFlags;
    std::vector<uint8_t> methodTypes;
    std::vector<bool> filterEomPointsFlags;
    std::vector<uint8_t> gridSizeMinus2s;
    std::vector<uint8_t> thresholds;

    sei_geometry_smoothing() : persistenceFlag(false), resetFlag(false), instancesUpdated(0) {
        payload_type = GEOMETRY_SMOOTHING;
    };

    void allocate() {
        instanceIndices.resize(instancesUpdated, 0);
        instanceCancelFlags.resize(instancesUpdated, false);
        methodTypes.resize(instancesUpdated, 0);
        filterEomPointsFlags.resize(instancesUpdated, false);
        gridSizeMinus2s.resize(instancesUpdated, 0);
        thresholds.resize(instancesUpdated, 0);
    }
};

struct pccsei {
    std::vector<std::shared_ptr<sei>> sei_prefix;
    std::vector<std::shared_ptr<sei>> sei_suffix;

    sei& addSei(NAL_UNIT_TYPE nal_unit_type, SEI_PAYLOAD_TYPE payload_type) {
        std::shared_ptr<sei> sei_;
        switch(payload_type) {
            // case BUFFERING_PERIOD: sharedPtr = std::make_shared<SEIBufferingPeriod>(); break;
            // case ATLAS_FRAME_TIMING: sharedPtr = std::make_shared<SEIAtlasFrameTiming>(); break;
            // case FILLER_PAYLOAD: break;
            // case USER_DATAREGISTERED_ITUTT35: sharedPtr = std::make_shared<SEIUserDataRegisteredItuTT35>(); break;
            // case USER_DATA_UNREGISTERED: sharedPtr = std::make_shared<SEIUserDataUnregistered>(); break;
            // case RECOVERY_POINT: sharedPtr = std::make_shared<SEIRecoveryPoint>(); break;
            // case NO_RECONSTRUCTION: sharedPtr = std::make_shared<SEINoDisplay>(); break;
            // case TIME_CODE: sharedPtr = std::make_shared<SEITimeCode>(); break;
            // case SEI_MANIFEST: sharedPtr = std::make_shared<SEIManifest>(); break;
            // case SEI_PREFIX_INDICATION: sharedPtr = std::make_shared<SEIPrefixIndication>(); break;
            // case ACTIVE_SUB_BITSTREAMS: sharedPtr = std::make_shared<SEIActiveSubBitstreams>(); break;
            // case COMPONENT_CODEC_MAPPING: sharedPtr = std::make_shared<SEIComponentCodecMapping>(); break;
            // case SCENE_OBJECT_INFORMATION: sharedPtr = std::make_shared<SEISceneObjectInformation>(); break;
            // case OBJECT_LABEL_INFORMATION: sharedPtr = std::make_shared<SEIObjectLabelInformation>(); break;
            // case PATCH_INFORMATION: sharedPtr = std::make_shared<SEIPatchInformation>(); break;
            // case VOLUMETRIC_RECTANGLE_INFORMATION: sharedPtr = std::make_shared<SEIVolumetricRectangleInformation>(); break;
            // case ATLAS_OBJECT_INFORMATION: sharedPtr = std::make_shared<SEIAtlasInformation>(); break;
            // case VIEWPORT_CAMERA_PARAMETERS: sharedPtr = std::make_shared<SEIViewportCameraParameters>(); break;
            // case VIEWPORT_POSITION: sharedPtr = std::make_shared<SEIViewportPosition>(); break;
            // case DECODED_ATLAS_INFORMATION_HASH: sharedPtr = std::make_shared<SEIDecodedAtlasInformationHash>(); break;
            // case ATTRIBUTE_TRANSFORMATION_PARAMS: sharedPtr = std::make_shared<SEIAttributeTransformationParams>(); break;
            case OCCUPANCY_SYNTHESIS: sei_ = std::make_shared<sei_occupancy_synthesis>(); break;
            case GEOMETRY_SMOOTHING: sei_ = std::make_shared<sei_geometry_smoothing>(); break;
            // case ATTRIBUTE_SMOOTHING: sharedPtr = std::make_shared<SEIAttributeSmoothing>(); break;
            // case RESERVED_SEI_MESSAGE: sharedPtr = std::make_shared<SEIReservedSeiMessage>(); break; 
            default:
                fprintf( stderr, "SEI payload type not supported \n" );
                exit( -1 );
                break;
        }

        if (nal_unit_type == NAL_PREFIX_ESEI || nal_unit_type == NAL_PREFIX_NSEI) {
            sei_prefix.push_back(sei_);
            return *sei_prefix.back();
        } else if (nal_unit_type == NAL_SUFFIX_ESEI || nal_unit_type == NAL_SUFFIX_NSEI) {
            sei_suffix.push_back(sei_);
            return *sei_suffix.back();
        } else {
            fprintf( stderr, "Nal unit type of SEI not correct\n" );
            exit( -1 );
        }
        return *sei_;
    };

    bool prefix_sei_is_present(SEI_PAYLOAD_TYPE sei_payload_type, int sei_idx) {
    for (size_t i = 0; i < sei_prefix.size(); i ++) {
        if (sei_prefix.at(i)->payload_type == sei_payload_type) {
        sei_idx = i;
        return true;
        }
    }
        sei_idx = - 1;
        return false;
    };

};

// Occupancy synthesis sei message syntax
void occupancySynthesis(bitstream_t* stream, sei& seiAbstract);

// Geometry smooth sei message syntax
void geometrySmoothing(bitstream_t* stream, sei& seiAbstract);

// Prefix supplemental enhancement information Rbsp
void read_prefix_sei_rbsp(bitstream_t* stream, NAL_UNIT_TYPE nal_unit_type, pccsei& seis);

// Suffix supplemental enhancement information Rbsp
void read_suffix_sei_rbsp(bitstream_t* stream, NAL_UNIT_TYPE nal_unit_type, pccsei& seis);