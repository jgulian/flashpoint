#ifndef FLASHPOINT_SIMPLE_STORAGE_HPP
#define FLASHPOINT_SIMPLE_STORAGE_HPP

#include "keyvalue/keyvalue.hpp"

namespace flashpoint::keyvalue {

    class SimpleStorage : Storage {
    public:
        bool doOperation(Operation &operation) override;

    private:
        std::unordered_map<std::string, std::string> storage_ = {};
    };

}
#endif //FLASHPOINT_SIMPLE_STORAGE_HPP