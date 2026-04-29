#pragma once

#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

class Adaptation {

public:
    // static void adapt(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, const std::string& outputFilePath);
    static void adapt(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, uvgvpcc_dec::API::point_cloud_frame_stream* output, const bool& in_order_output, const std::string& outputFilePath);
    static void adapt_delay(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, uvgvpcc_dec::API::point_cloud_frame_stream* output);
    static void adapt_in_order(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, uvgvpcc_dec::API::point_cloud_frame_stream* output);
    static void adapt_remote_output(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, const std::shared_ptr<zmqHandler> zmq_handler);
};