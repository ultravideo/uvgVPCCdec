#ifndef PORTAL_HPP
#define PORTAL_HPP

#include <zmq.hpp>
#include <thread>  
#include <queue>
#include <vector>

#include <mutex>
#include <condition_variable>

struct zmqHandler {
    // Communication
    std::string color_address = "tcp://*:5555";             // Default address for color
    std::string position_address = "tcp://*:5556";          // Default address for position
    // std::string color_address = "tcp://10.21.25.231:5555";             // Default address for color
    // std::string position_address = "tcp://10.21.25.231:5556";          // Default address for position
    std::queue<zmq::message_t> colorMessages;               // Queue for color messages
    std::queue<zmq::message_t> positionMessages;            // Queue for position messages
    std::condition_variable receive_message_cv;             // Condition variable for receiving messages

    zmq::context_t context;
    zmq::socket_t colorSocket;
    zmq::socket_t positionSocket;

    std::mutex zmq_mutex;  //

    zmqHandler()
        : context(1),
          colorSocket(context, zmq::socket_type::push),
          positionSocket(context, zmq::socket_type::push) {}
          
        // :context(1),
        //   colorSocket(context, ZMQ_PUSH),
        //   positionSocket(context, ZMQ_PUSH) {}

};

enum SourceMode
{
    SOURCE_SEQUENCE = 0,
    SOURCE_ZMQ = 1,
};

static std::shared_ptr<zmqHandler> zmq_handler = nullptr;       // ZMQ Handler


static bool stop_flag = false;

// void stop_signal() {
//     stop_flag = true;
//     zmq_handler->receive_message_cv.notify_all();
// }

#endif // PORTAL_HPP