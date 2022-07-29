#ifndef FLASHPOINT_COMMAND_HPP
#define FLASHPOINT_COMMAND_HPP

#include <string>
#include <chrono>

namespace flashpoint::keyvalue {

    enum OperationType {
        PUT,
        GET,
    };

    struct Operation {
        OperationType type;
        std::string data;
    };

}

#endif //FLASHPOINT_COMMAND_HPP
