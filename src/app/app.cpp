#include "uvgvpccdec/uvgvpccdec.hpp"
#include <iostream>
#include <fstream>

void readFile(const std::string filename, std::vector<uint8_t> &data);
void readFile(const std::string filename, uvgvpcc_dec::API::v3c_unit_stream &unit_stream);

size_t read_value(const uint8_t* src, size_t len) {
    size_t value = 0;
    for (size_t i = 0; i < len; ++i) {
        value |= static_cast<size_t>(src[i]) << (8 * (len - 1 - i));
    }
    return value;
}

int main() {
    uvgvpcc_dec::Logger::setLogLevel(uvgvpcc_dec::LogLevel::TRACE);
    uvgvpcc_dec::Parameters param;
    param.occupancy_width = 640;
    param.occupancy_height = 640;
    param.video_width = 1280;
    param.video_height = 1280;
    param.keep_intermediate_files = false;
    //param.max_points = 738590;
    uvgvpcc_dec::API::initializeDecoder(param);

    std::string input_file = "longdress-f1.vpcc";

    uvgvpcc_dec::API::v3c_unit_stream unit_stream;
    readFile(input_file, unit_stream);
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::DEBUG, "Application", "Number of V3C chunks " + std::to_string(unit_stream.v3c_chunks.size()) + " \n");
    for (size_t i = 0; i < unit_stream.v3c_chunks.size(); ++i) {
        /*const*/ auto& chunk = unit_stream.v3c_chunks.front();
        uvgvpcc_dec::API::decodeV3CChunk(chunk);
        unit_stream.v3c_chunks.pop();
    }
    
        
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::INFO, "Application", "Done \n");
    return 0;
}

void readFile(const std::string filename, std::vector<uint8_t> &data)
{
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::DEBUG, "Application", "Opening file " + filename + " \n");
    std::ifstream input_file (filename);
    if (input_file.is_open()) {
        input_file.seekg(0, std::ios::end);
        std::size_t size = input_file.tellg();
        input_file.seekg(0, std::ios::beg);
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::DEBUG, "Application", "Size of file " + std::to_string(size) + " \n");

        data.resize(static_cast<std::size_t>(size)); // Allocate required storage
        input_file.read(reinterpret_cast<char*> (&data[0]), size);

        input_file.close();
    }
    else {
        throw std::runtime_error("Error reading input file");
    }
}

void readFile(const std::string filename, uvgvpcc_dec::API::v3c_unit_stream &unit_stream)
{
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::DEBUG, "Application", "Opening file " + filename + " \n");
    std::ifstream input_file (filename);

    if(!input_file.is_open()) {
        throw std::runtime_error("File reading : Could not open input file " + filename);
    }
    uint8_t header_byte = 0;
    input_file.read(reinterpret_cast<char*> (&header_byte), sizeof(uint8_t));
    size_t v3c_unit_size_precision = (header_byte >> 5) + 1;

    size_t data_read = 1; // hdr byte already read
    unit_stream.v3c_unit_size_precision_bytes = v3c_unit_size_precision;
    
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::DEBUG, "Application", "V3C unit size precision " + std::to_string(v3c_unit_size_precision) + " \n");
    uvgvpcc_dec::API::v3c_chunk latest_chunk;
    while (input_file.peek() != EOF) {
        
        uint8_t v3c_size_array[v3c_unit_size_precision];
        input_file.read(reinterpret_cast<char*>(v3c_size_array), v3c_unit_size_precision);
        data_read += input_file.gcount();

        size_t v3c_unit_size = read_value(v3c_size_array, v3c_unit_size_precision);;
        latest_chunk.v3c_unit_sizes.push_back(v3c_unit_size);
        size_t old_size = latest_chunk.data.size();
        latest_chunk.data.resize(old_size + v3c_unit_size);
        input_file.read(reinterpret_cast<char*>(&latest_chunk.data[old_size]), v3c_unit_size);
        data_read += input_file.gcount();
        
    }
    unit_stream.v3c_chunks.push(latest_chunk);
    input_file.close();
}