#include "config.h"
#include "game_hooks.h"
#include "module.h"
#include "runtime.h"

#include <exception>

namespace {

using namespace iidxfreq;

std::int32_t disable(const char* message) {
    // Once hooks are active, continuing after a partial failure could desync chart and audio.
    if (active.load()) {
        log_fatal(message);
    }
    try {
        log_msg(message, SPICE_SDK_LOG_LEVEL_WARNING);
        show_toast(SPICE_SDK_TOAST_LEVEL_ERROR, std::string("Rate mode disabled: ") + message);
    } catch (...) {
    }
    return 1;
}

std::int32_t initialize(spice_sdk_init_func* init) {
    const auto status = initialize_runtime(init);
    if (status != SPICE_SDK_STATUS_SUCCESS) {
        return status;
    }
    HMODULE self{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&initialize), &self)) {
        log_msg("Cannot resolve the hook DLL module.", SPICE_SDK_LOG_LEVEL_WARNING);
        return 1;
    }
    try {
        auto path = module_path(self);
        if (!path) {
            return disable(path.error.c_str());
        }
        path.value.replace_extension(".ini");
        const auto loaded = load_config(path.value);
        if (!loaded) {
            return disable(loaded.error.c_str());
        }
        const auto& config = loaded.value;
        if (!config.enabled) {
            log_msg("Disabled. Set enabled=1 in iidxfreq.ini to activate rate mode.");
            return 0;
        }
        configure_runtime(config);
        const auto model = game_model();
        if (!model) {
            return disable(model.error.c_str());
        }
        install_game_hooks(model.value);
        start_hotkeys();

        log_msg("ACTIVE speed=" + std::to_string(config.speed) +
                " preserve_pitch=" + std::to_string(pending_preserve_pitch.load()));

        show_toast(SPICE_SDK_TOAST_LEVEL_WARNING,
                   "Next play rate: " + rate_text(config.speed) + ".");

        return 0;
    } catch (const std::exception& error) {
        return disable(error.what());
    }
}

}

SPICE_SDK_ENTRY_POINT spice_sdk_entry_point(spice_sdk_init_func* init) {
    // Repeated SDK entry calls must not install another set of hooks or polling threads.
    static const std::int32_t result = initialize(init);
    return result;
}