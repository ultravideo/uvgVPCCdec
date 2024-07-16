#include <fstream>

#include "uvgvpccdec/uvgvpccdec.hpp"
#include "Decompression/decompression.hpp"
#include "FormatConversion/formatConversion.hpp"

namespace uvgvpcc_dec
{
void readFile(const std::string filename, std::vector<uint8_t> &data);

void API::initializeDecoder(const Parameters& param)
{
    Logger::log(LogLevel::INFO, "API", "Hello, World! " + std::to_string(param.hello) + "\n");

}

void API::decodeV3CSampleStream(const std::string filename)
{
    std::vector<uint8_t> data;
    readFile(filename, data);

    decompressed_data decompressed;
    
    BitstreamParsing::decompressV3CSampleStream(data, &decompressed);
    FormatConversion::convertToNominalFormat(&decompressed);

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
        std::cout << "error reading file" << std::endl;
    }
}
} // namespace uvgvpcc_dec

