#pragma once

#include "result.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace iidxfreq {

inline bool valid_rate(double rate) {
    return std::isfinite(rate) && rate >= 0.5 && rate <= 2.0;
}

inline Result<std::uint32_t> scale_milliseconds(std::uint32_t milliseconds, double rate) {
    if (!valid_rate(rate)) {
        return Error{"Rate must be between 0.5 and 2.0."};
    }
    const double scaled = std::round(static_cast<double>(milliseconds) / rate);
    // Chart timestamps are unsigned on disk, but the game uses a signed clock.
    if (scaled > std::numeric_limits<std::int32_t>::max()) {
        return Error{"Scaled timestamp exceeds the game clock range."};
    }
    return static_cast<std::uint32_t>(scaled);
}

inline Result<std::uint16_t> scale_duration(std::uint16_t milliseconds, double rate) {
    const auto scaled = scale_milliseconds(milliseconds, rate);
    if (!scaled) {
        return Error{scaled.error};
    }
    if (scaled.value > std::numeric_limits<std::uint16_t>::max()) {
        return Error{"Scaled hold duration exceeds the chart format."};
    }
    return static_cast<std::uint16_t>(scaled.value);
}

}