#pragma once

#include <string>
#include <utility>

namespace iidxfreq {

struct Error {
    std::string message;
};

template <class Value> struct Result {
    Value value{};
    std::string error;

    Result(Value value) : value(std::move(value)) {}
    Result(Error failure) : error(std::move(failure.message)) {}

    explicit operator bool() const {
        return error.empty();
    }
};

}