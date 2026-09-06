#include "runtime.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <system_error>
#include <thread>

namespace iidxfreq {

std::atomic<bool> active{false};
std::atomic<double> pending_speed{1.0};
std::atomic<bool> pending_preserve_pitch{false};

namespace {

SPICE_SDK_V0 sdk{};
std::atomic<bool> sdk_ready{false};
std::jthread hotkey_thread;
Config settings;

void __cdecl shutdown() {
    // Stop the SDK's caller before the host tears down its function table.
    if (hotkey_thread.joinable()) {
        hotkey_thread.request_stop();
        hotkey_thread.join();
    }
    sdk_ready.store(false);
}

enum class HotkeyAction { Increase, Decrease, Reset, TogglePitch };

struct HotkeyBinding {
    std::uint32_t key;
    HotkeyAction action;
    bool was_down = false;
};

bool key_pressed(std::uint32_t key) {
    return key != 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool hotkeys_focused() {
    DWORD foreground_process{};
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground_process);
    const std::uint32_t modifiers = (key_pressed(VK_CONTROL) ? 1U : 0U) |
                                    (key_pressed(VK_MENU) ? 2U : 0U) |
                                    (key_pressed(VK_SHIFT) ? 4U : 0U);
    return foreground_process == GetCurrentProcessId() && modifiers == settings.key_modifiers;
}

void update_hotkeys(std::array<HotkeyBinding, 4>& bindings) {
    const bool focused = hotkeys_focused();
    const auto old_rate = pending_speed.load();
    auto new_rate = old_rate;
    const auto old_pitch = pending_preserve_pitch.load();
    auto new_pitch = old_pitch;
    for (auto& binding : bindings) {
        const bool down = key_pressed(binding.key);
        if (focused && down && !binding.was_down) {
            switch (binding.action) {
            case HotkeyAction::Increase: {
                new_rate += settings.speed_step;
                break;
            }
            case HotkeyAction::Decrease: {
                new_rate -= settings.speed_step;
                break;
            }
            case HotkeyAction::Reset: {
                new_rate = 1.0;
                break;
            }
            case HotkeyAction::TogglePitch: {
                new_pitch = !new_pitch;
                break;
            }
            }
        }
        // Track keys even out of focus so switching windows does not create a fresh press.
        binding.was_down = down;
    }
    new_rate = std::clamp(std::round(new_rate * 100.0) / 100.0, 0.5, 2.0);
    if (new_rate != old_rate) {
        pending_speed.store(new_rate);
        try {
            const auto message = "Next song rate: " + rate_text(new_rate);
            log_msg(message);
            show_toast(SPICE_SDK_TOAST_LEVEL_INFO, message);
        } catch (...) {
        }
    }
    if (new_pitch != old_pitch) {
        pending_preserve_pitch.store(new_pitch);
        try {
            const std::string message =
                new_pitch ? "Next song: preserve pitch ON" : "Next song: preserve pitch OFF";
            log_msg(message);
            show_toast(SPICE_SDK_TOAST_LEVEL_INFO, message);
        } catch (...) {
        }
    }
}

void poll_hotkeys(std::stop_token stop_token) {
    std::array bindings{HotkeyBinding{settings.increase_key, HotkeyAction::Increase},
                        HotkeyBinding{settings.decrease_key, HotkeyAction::Decrease},
                        HotkeyBinding{settings.reset_key, HotkeyAction::Reset},
                        HotkeyBinding{settings.preserve_pitch_key, HotkeyAction::TogglePitch}};
    std::mutex wait_mutex;
    std::condition_variable_any wake;
    std::unique_lock wait_lock(wait_mutex);
    while (!stop_token.stop_requested()) {
        wake.wait_for(wait_lock, stop_token, std::chrono::milliseconds(20), [] {
            return false;
        });
        if (stop_token.stop_requested()) {
            break;
        }
        update_hotkeys(bindings);
    }
}

}

std::int32_t initialize_runtime(spice_sdk_init_func* init) {
    if (!init) {
        return SPICE_SDK_STATUS_INVALID_ARGUMENT_1;
    }
    sdk.size = sizeof(sdk);
    const auto status = init(0, &shutdown, &sdk);
    if (status != SPICE_SDK_STATUS_SUCCESS) {
        return status;
    }
    if (!sdk.log || !sdk.get_game_info || !sdk.get_avs_info) {
        return SPICE_SDK_STATUS_NOT_SUPPORTED;
    }
    sdk_ready.store(true);
    return SPICE_SDK_STATUS_SUCCESS;
}

Result<std::string> game_model() {
    SPICE_SDK_GAME_INFO game_info{};
    SPICE_SDK_AVS_INFO avs_info{};
    if (sdk.get_game_info(&game_info) != SPICE_SDK_STATUS_SUCCESS ||
        sdk.get_avs_info(&avs_info) != SPICE_SDK_STATUS_SUCCESS) {
        return Error{"Cannot obtain game identity from the Spice SDK."};
    }
    log_msg("Game: " + std::string(game_info.name) + ", model: " + avs_info.model +
        ", version: " + avs_info.ext);
    return std::string(avs_info.model);
}

void configure_runtime(const Config& config) {
    settings = config;
    pending_speed.store(config.speed);
    pending_preserve_pitch.store(config.preserve_pitch);
}

void start_hotkeys() {
    try {
        hotkey_thread = std::jthread(poll_hotkeys);
    } catch (const std::system_error&) {
        log_fatal("Cannot start rate hotkeys.");
    }
}

void log_msg(const std::string& message, SPICE_SDK_LOG_LEVEL level) {
    if (sdk_ready.load()) {
        sdk.log(level, "iidxfreq", message.c_str());
    }
}

void show_toast(SPICE_SDK_TOAST_SEVERITY severity, const std::string& message) {
    if (sdk_ready.load() && sdk.add_toast) {
        const auto text = "[FREQ] " + message;
        sdk.add_toast(severity, text.c_str());
    }
}

[[noreturn]] void log_fatal(const char* message) noexcept {
    if (sdk_ready.load()) {
        sdk.log(SPICE_SDK_LOG_LEVEL_FATAL, "iidxfreq", message);
    }
    std::abort();
}

std::string rate_text(double rate) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << rate << 'x';
    return text.str();
}

}