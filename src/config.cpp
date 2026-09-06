#include "config.h"
#include "rate.h"

#include <SimpleIni.h>

#include <array>
#include <cerrno>
#include <limits>
#include <string>

namespace iidxfreq {
namespace {

double number_setting(const CSimpleIniA& ini, const char* name, double fallback) {
    return ini.GetDoubleValue(
        "iidxfreq", name,
        ini.KeyExists("iidxfreq", name) ? std::numeric_limits<double>::quiet_NaN() : fallback);
}

std::string load_hotkey_settings(const CSimpleIniA& ini, Config& config) {
    struct KeySetting {
        const char* name;
        std::uint32_t* value;
        std::uint32_t maximum;
    };
    const std::array keys{KeySetting{"increase_key", &config.increase_key, 255},
                          KeySetting{"decrease_key", &config.decrease_key, 255},
                          KeySetting{"reset_key", &config.reset_key, 255},
                          KeySetting{"preserve_pitch_key", &config.preserve_pitch_key, 255},
                          KeySetting{"key_modifiers", &config.key_modifiers, 7}};
    for (const auto& key : keys) {
        const auto fallback =
            ini.KeyExists("iidxfreq", key.name) ? -1L : static_cast<long>(*key.value);
        const auto value = ini.GetLongValue("iidxfreq", key.name, fallback);
        if (value < 0 || static_cast<std::uint32_t>(value) > key.maximum) {
            return std::string("Invalid hotkey setting: ") + key.name;
        }
        *key.value = static_cast<std::uint32_t>(value);
    }
    const std::array bindings{config.increase_key, config.decrease_key, config.reset_key,
                              config.preserve_pitch_key};
    for (std::size_t first = 0; first < bindings.size(); ++first) {
        for (std::size_t second = first + 1; second < bindings.size(); ++second) {
            if (bindings[first] != 0 && bindings[first] == bindings[second]) {
                return "Hotkeys must be distinct, or zero to disable.";
            }
        }
    }
    return {};
}

}

Result<Config> load_config(const std::filesystem::path& path) {
    Config config;
    CSimpleIniA ini;
    ini.SetUnicode();
    ini.SetQuotes();
    errno = 0;
    const auto status = ini.LoadFile(path.c_str());
    if (status < 0) {
        if (status == SI_FILE && errno == ENOENT) {
            return config;
        }
        return Error{"Unable to load INI file. Use UTF-8 encoding."};
    }
    config.enabled = ini.GetBoolValue("iidxfreq", "enabled", config.enabled);
    // Disabled mode ignores the remaining options and leaves the game untouched.
    if (!config.enabled) {
        return config;
    }
    config.speed = number_setting(ini, "speed", config.speed);
    if (!valid_rate(config.speed)) {
        return Error{"Speed must be between 0.5 and 2.0."};
    }
    config.preserve_pitch = ini.GetBoolValue("iidxfreq", "preserve_pitch", config.preserve_pitch);
    const auto hotkey_error = load_hotkey_settings(ini, config);
    if (!hotkey_error.empty()) {
        return Error{hotkey_error};
    }
    config.speed_step = number_setting(ini, "speed_step", config.speed_step);
    // Hotkeys round to hundredths, so smaller increments would be lost during polling.
    if (!std::isfinite(config.speed_step) || config.speed_step < 0.01 || config.speed_step > 1.0 ||
        std::abs(config.speed_step * 100.0 - std::round(config.speed_step * 100.0)) > 0.000001) {
        return Error{"speed_step must be 0.01 to 1.00 in increments of 0.01."};
    }
    return config;
}

}