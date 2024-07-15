#include "uvgvpccdec/uvgvpccdec.hpp"

namespace uvgvpcc_dec
{

void API::initializeDecoder(const Parameters& param){
    Logger::log(LogLevel::INFO, "API", "Hello, World! " + std::to_string(param.hello) + "\n");

}

} // namespace uvgvpcc_dec

