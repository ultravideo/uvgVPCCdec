/// \file Library parameters related operations.

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "utils.hpp"

namespace uvgvpcc_dec {

struct Parameters {
    int hello = 0;

    size_t max_points = 0;
    bool keep_intermediate_files = false;
    bool fastColorConversion = false;
    
    bool useTMC2AttributeYUVConversion=false;
    bool nbThread=20;

    bool color_inversion_16bits = true;

    bool exportIntermediateFiles = false;
    std::string intermediateFilesDir;

    // size_t nbThreadPCPart = 40;       // 0 means the actual number of detected threads
    size_t nbThreadPCPart = 0;       // 0 means the actual number of detected threads
    size_t maxConcurrentFrames = 40;  // 0 means the actual value is set to 4 times sizeGOF
    bool errorsAreFatal = true;
};

enum ParameterType { BOOL, INT, UINT, STRING, FLOAT, DOUBLE };

struct ParameterInfo {
    ParameterType type;
    std::string possibleValues;
    void* parameterPtr;
    bool inPreset = false;

    ParameterInfo(const ParameterType& type, const std::string& possibleValues, bool* parameterPtr)
        : type(type), possibleValues(possibleValues), parameterPtr((void*)parameterPtr) {
        if (type != BOOL) {
            throw std::runtime_error(
                "During the initialization of the library parameter maps, a type mismatch has been found. Apparently, the given "
                "parameterType is: '" +
                std::to_string(type) +
                "' while the type of the parameter variable is BOOL (0). The corresponding variable name is not known, but here are its "
                "possible values :'" +
                possibleValues + "'. If you recently added a new parameter in the parameter map, the given type is probably wrong.");
        }
    }
    ParameterInfo(const ParameterType& type, const std::string& possibleValues, int* parameterPtr)
        : type(type), possibleValues(possibleValues), parameterPtr((void*)parameterPtr) {
        if (type != INT) {
            throw std::runtime_error(
                "During the initialization of the library parameter maps, a type mismatch has been found. Apparently, the given "
                "parameterType is: '" +
                std::to_string(type) +
                "' while the type of the parameter variable is INT (1). The corresponding variable name is not known, but here are its "
                "possible values :'" +
                possibleValues + "'. If you recently added a new parameter in the parameter map, the given type is probably wrong.");
        }
    }
    ParameterInfo(const ParameterType& type, const std::string& possibleValues, size_t* parameterPtr)
        : type(type), possibleValues(possibleValues), parameterPtr((void*)parameterPtr) {
        if (type != UINT) {
            throw std::runtime_error(
                "During the initialization of the library parameter maps, a type mismatch has been found. Apparently, the given "
                "parameterType is: '" +
                std::to_string(type) +
                "' while the type of the parameter variable is UINT (2). The corresponding variable name is not known, but here are its "
                "possible values :'" +
                possibleValues + "'. If you recently added a new parameter in the parameter map, the given type is probably wrong.");
        }
    }
    ParameterInfo(const ParameterType& type, const std::string& possibleValues, std::string* parameterPtr)
        : type(type), possibleValues(possibleValues), parameterPtr((void*)parameterPtr) {
        if (type != STRING) {
            throw std::runtime_error(
                "During the initialization of the library parameter maps, a type mismatch has been found. Apparently, the given "
                "parameterType is: '" +
                std::to_string(type) +
                "' while the type of the parameter variable is STRING (3). The corresponding variable name is not known, but here are its "
                "possible values :'" +
                possibleValues + "'. If you recently added a new parameter in the parameter map, the given type is probably wrong.");
        }
    }
    ParameterInfo(const ParameterType& type, const std::string& possibleValues, float* parameterPtr)
        : type(type), possibleValues(possibleValues), parameterPtr((void*)parameterPtr) {
        if (type != FLOAT) {
            throw std::runtime_error(
                "During the initialization of the library parameter maps, a type mismatch has been found. Apparently, the given "
                "parameterType is: '" +
                std::to_string(type) +
                "' while the type of the parameter variable is FLOAT (4). The corresponding variable name is not known, but here are its "
                "possible values :'" +
                possibleValues + "'. If you recently added a new parameter in the parameter map, the given type is probably wrong.");
        }
    }
    ParameterInfo(const ParameterType& type, const std::string& possibleValues, double* parameterPtr)
        : type(type), possibleValues(possibleValues), parameterPtr((void*)parameterPtr) {
        if (type != DOUBLE) {
            throw std::runtime_error(
                "During the initialization of the library parameter maps, a type mismatch has been found. Apparently, the given "
                "parameterType is: '" +
                std::to_string(type) +
                "' while the type of the parameter variable is DOUBLE (5). The corresponding variable name is not known, but here are its "
                "possible values :'" +
                possibleValues + "'. If you recently added a new parameter in the parameter map, the given type is probably wrong.");
        }
    }
};

extern const Parameters* p_;  // Const pointer to a non-const Parameter struct instance in parameters.cpp

void initializeParameterMap(Parameters& param);
void setParameterValue(const std::string& parameterName, const std::string& parameterValue, const bool& fromPreset);

} // namespace uvgvpcc_dec

