#pragma once

#include "result.h"

#include <cstdint>
#include <span>
#include <vector>

namespace iidxfreq {

Result<std::vector<std::int16_t>> process_audio(std::span<const std::int16_t> samples,
                                                std::uint32_t channels, std::uint32_t sample_rate,
                                                double rate, bool preserve_pitch);

}