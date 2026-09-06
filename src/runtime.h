#pragma once

#include "config.h"

#include <windows.h>
#include <spicesdk.h>

#include <atomic>
#include <string>

namespace iidxfreq {

extern std::atomic<bool> active;
extern std::atomic<double> pending_speed;
extern std::atomic<bool> pending_preserve_pitch;

std::int32_t initialize_runtime(spice_sdk_init_func* init);
Result<std::string> game_model();
void configure_runtime(const Config& config);
void start_hotkeys();
void log_msg(const std::string& message, SPICE_SDK_LOG_LEVEL level = SPICE_SDK_LOG_LEVEL_INFO);
void show_toast(SPICE_SDK_TOAST_SEVERITY severity, const std::string& message);
[[noreturn]] void log_fatal(const char* message) noexcept;
std::string rate_text(double rate);

}