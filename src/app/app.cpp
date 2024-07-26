#include "uvgvpccdec/uvgvpccdec.hpp"
#include <iostream>
#include <fstream>

void readFile(const std::string filename, std::vector<uint8_t> &data);

int main() {
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::INFO, "Application", "Decode .vpcc file \n");
    uvgvpcc_dec::Parameters param;
    uvgvpcc_dec::API::initializeDecoder(param);

    std::vector<uint8_t> data;
    readFile("decoder-testing-3f.vpcc", data);

    //uvgvpcc_dec::API::decodeV3CSampleStream("BITSTREAM-V3C.vpcc");
    uvgvpcc_dec::API::decodeV3CSampleStream(data);
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::INFO, "Application", "Done \n");
    return 0;
}

void readFile(const std::string filename, std::vector<uint8_t> &data)
{
    std::cout << "Opening file " << filename << std::endl;
    std::ifstream input_file (filename);
    if (input_file.is_open()) {
        input_file.seekg(0, std::ios::end);
        std::size_t size = input_file.tellg();
        input_file.seekg(0, std::ios::beg);
        std::cout << "size of file is " << size << std::endl;

        data.resize(static_cast<std::size_t>(size)); // Allocate required storage
        input_file.read(reinterpret_cast<char*> (&data[0]), size);

        input_file.close();
    }
    else {
        throw std::runtime_error("Error reading input file");
    }
}