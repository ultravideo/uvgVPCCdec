#include "bitstreamParsing.hpp"

void BitstreamParsing::initializeStaticParameters(const uvgvpcc_dec::Parameters& param)
{
    int x = param.hello;
}

void BitstreamParsing::parseV3CSampleStream(const std::vector<uint8_t> &data)
{
    const uint8_t* cbuf = data.data();
    std::size_t ptr = 0;

    // First byte is the file header
    uint8_t first_byte = cbuf[ptr];
    std::cout << "First byte " << uint32_t(first_byte) << std::endl;
    uint8_t v3c_size_precision = (first_byte >> 5) + 1;
    std::cout << "V3C size precision: " << (uint32_t)v3c_size_precision << std::endl;
    std::cout << std::endl;
    ++ptr;

    /*uint8_t* v3c_size = new uint8_t[v3c_size_precision];
    uint8_t nal_size_precision = 0;
    while (true) {
        if (ptr >= len) {
            break;
        }
        // Readthe V3C unit size (2-3 bytes usually)
        memcpy(v3c_size, &cbuf[ptr], v3c_size_precision);
        ptr += v3c_size_precision; // Jump over the V3C unit size bytes
        uint32_t combined_v3c_size = 0;
        if (v3c_size_precision == 2) {
            combined_v3c_size = combineBytes(v3c_size[0], v3c_size[1]);
        }
        else if (v3c_size_precision == 3) {
            combined_v3c_size = combineBytes(v3c_size[0], v3c_size[1], v3c_size[2]);
        }
        else {
            std::cout << "Error " << std::endl;
            return EXIT_FAILURE;
        }
        // Inside v3c unit now
        std::cout << "Current V3C unit location " << ptr << ", size " << combined_v3c_size << std::endl;
        uint64_t v3c_ptr = ptr;

        // Next 4 bytes are the V3C unit header
        v3c_unit_header v3c_hdr = {};
        parse_v3c_header(v3c_hdr, cbuf, v3c_ptr);
        uint8_t vuh_t = v3c_hdr.vuh_unit_type;
        std::cout << "-- vuh_unit_type: " << (uint32_t)vuh_t << std::endl;
        v3c_unit_info unit = { v3c_hdr, {}};

        if (vuh_t == V3C_VPS) {
            // Parameter set contains no NAL units, skip over
            std::cout << "-- Parameter set V3C unit" << std::endl;
            nal_info nalu = {ptr, combined_v3c_size, nullptr};
            unit.nal_infos.push_back(nalu);
            mmap.vps_units.push_back(unit);
            ptr += combined_v3c_size;
            std::cout << std::endl;
            continue;
        }

        // Rest of the function goes inside the V3C unit payload and parses it into NAL units
        v3c_ptr += V3C_HDR_LEN; // Jump over 4 bytes of V3C unit header
        if (vuh_t == V3C_AD || vuh_t == V3C_CAD) {
            uint8_t v3cu_first_byte = cbuf[v3c_ptr]; // Next up is 1 byte of NAL unit size precision
            nal_size_precision = (v3cu_first_byte >> 5) + 1;
            std::cout << "  -- Atlas NAL Sample stream, 1 byte for NAL unit size precision: " << (uint32_t)nal_size_precision << std::endl;
            ++v3c_ptr;
        }
        else {
            nal_size_precision = 4;
            std::cout << "  -- Video NAL Sample stream, using NAL unit size precision of: " << (uint32_t)nal_size_precision << std::endl;
        }
        uint64_t amount_of_nal_units = 0;
        // Now start to parse the NAL sample stream
        while (true) {
            if (v3c_ptr >= (ptr + combined_v3c_size)) {
                break;
            }
            amount_of_nal_units++;
            uint32_t combined_nal_size = 0;
            if (nal_size_precision == 2) {
                combined_nal_size = combineBytes(cbuf[v3c_ptr], cbuf[v3c_ptr + 1]);
            }
            else if (nal_size_precision == 3) {
                combined_nal_size = combineBytes(cbuf[v3c_ptr], cbuf[v3c_ptr + 1], cbuf[v3c_ptr + 2]);
            }
            else if (nal_size_precision == 4) {
                combined_nal_size = combineBytes(cbuf[v3c_ptr], cbuf[v3c_ptr + 1], cbuf[v3c_ptr + 2], cbuf[v3c_ptr + 3]);
            }
            else {
                std::cout << "  -- Error, invalid NAL size " << std::endl;
                return EXIT_FAILURE;
            }
            v3c_ptr += nal_size_precision;
            switch (vuh_t) {
            case V3C_AD:
            case V3C_CAD: {
                uint8_t atlas_nalu_t = (cbuf[v3c_ptr] & 0b01111110) >> 1;
                std::cout << "  -- v3c_ptr: " << v3c_ptr << ", NALU size: " << combined_nal_size << ", Atlas NALU type: " << (uint32_t)atlas_nalu_t << std::endl;
                break; }
            case V3C_OVD:
            case V3C_GVD:
            case V3C_AVD:
            case V3C_PVD:
                uint8_t h265_nalu_t = (cbuf[v3c_ptr] & 0b01111110) >> 1;
                std::cout << "  -- v3c_ptr: " << v3c_ptr << ", NALU size: " << combined_nal_size << ", HEVC NALU type: " << (uint32_t)h265_nalu_t << std::endl;
            }
            nal_info nalu = { v3c_ptr, combined_nal_size, nullptr };
            unit.nal_infos.push_back({ v3c_ptr, combined_nal_size, nullptr });
            v3c_ptr += combined_nal_size;

        }
        std::cout << "  -- Amount of NAL units in v3c unit: " << amount_of_nal_units << std::endl;
            
        switch (vuh_t) {
            case V3C_AD:
                mmap.ad_units.push_back(unit);
                break;
            case V3C_CAD:
                mmap.cad_units.push_back(unit);
                break;
            case V3C_OVD:
                mmap.ovd_units.push_back(unit);
                break;
            case V3C_GVD:
                mmap.gvd_units.push_back(unit);
                break;
            case V3C_AVD:
                mmap.avd_units.push_back(unit);
                break;
            case V3C_PVD:
                mmap.pvd_units.push_back(unit);
                break;
        }
        std::cout << std::endl;
        ptr += combined_v3c_size;
    }*/
    std::cout << "File parsed" << std::endl;
}