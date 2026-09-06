#pragma once

#include "rate.h"

#include <algorithm>
#include <array>
#include <span>
#include <vector>

namespace iidxfreq {

struct ChartEvent {
    std::uint32_t milliseconds;
    std::uint8_t type;
    std::uint8_t parameter;
    std::uint16_t value;
    bool operator==(const ChartEvent&) const = default;
};
static_assert(sizeof(ChartEvent) == 8);

inline Result<std::vector<ChartEvent>> transform_chart(std::span<const ChartEvent> source,
                                                       double rate) {
    constexpr std::array<std::uint8_t, 4> duration_event_types{0, 1, 100, 101};
    constexpr std::uint8_t bpm_event_type = 4;
    constexpr std::uint8_t end_event_type = 6;
    if (!valid_rate(rate)) {
        return Error{"Chart speed must be between 0.5 and 2.0."};
    }
    std::vector<ChartEvent> result;
    result.reserve(source.size());
    std::uint32_t previous_milliseconds = 0;
    for (const auto& event : source) {
        if (event.milliseconds < previous_milliseconds) {
            return Error{"Chart events are not ordered."};
        }
        previous_milliseconds = event.milliseconds;
        auto transformed = event;
        const auto timestamp = scale_milliseconds(event.milliseconds, rate);
        if (!timestamp) {
            return Error{timestamp.error};
        }
        transformed.milliseconds = timestamp.value;
        if (std::ranges::find(duration_event_types, event.type) != duration_event_types.end()) {
            const auto duration = scale_duration(event.value, rate);
            if (!duration) {
                return Error{duration.error};
            }
            // Zero denotes a tap; a nonzero hold must not round down to a tap.
            transformed.value = event.value == 0 ? 0 : std::max<std::uint16_t>(1, duration.value);
        } else if (event.type == bpm_event_type && rate != 1.0) {
            // Match the game's integer BPM rounding before applying the playback rate.
            const auto original_bpm = std::round(static_cast<double>(event.value) /
                                                 std::max<std::uint32_t>(event.parameter, 1));
            const auto bpm = std::round(original_bpm * rate);
            if (bpm < 1 || bpm > std::numeric_limits<std::uint16_t>::max()) {
                return Error{"Scaled BPM is outside the chart format."};
            }
            transformed.parameter = 1;
            transformed.value = static_cast<std::uint16_t>(bpm);
        }
        result.push_back(transformed);
        if (event.type == end_event_type) {
            return result;
        }
    }
    return Error{"Chart has no bounded end marker."};
}

}