#pragma once

#include "result.h"

#include <cstdint>
#include <filesystem>

namespace iidxfreq {

struct Config {
    bool enabled = false;
    double speed = 1.0;
    bool preserve_pitch = false;
    std::uint32_t increase_key = 0x24;
    std::uint32_t decrease_key = 0x23;
    std::uint32_t reset_key = 0;
    std::uint32_t preserve_pitch_key = 0x50;
    std::uint32_t key_modifiers = 0;
    double speed_step = 0.05;
};

Result<Config> load_config(const std::filesystem::path& path);

}