#pragma once

#include "versions/profile.h"
#include "result.h"

#include <cstdint>

namespace iidxfreq {

struct ProcessedWave {
    void* wave;
    std::size_t frames;
    std::size_t original_frames;
    std::uint32_t sample_rate;
};

Result<ProcessedWave> process_wave(void* output, const std::byte* module_base,
                                   const versions::WaveLayout& layout, double speed,
                                   bool preserve_pitch);

}