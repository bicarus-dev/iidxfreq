#include "game_hooks.h"
#include "chart.h"
#include "module.h"
#include "runtime.h"
#include "versions/registry.h"
#include "wave.h"

#include <MinHook.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <exception>
#include <utility>

namespace iidxfreq {
namespace {

enum class GameplayMode : std::uint32_t {
    Standard = 0,
    StepUp = 5,
    PremiumFree = 6,
};

using GetGameplayMode = GameplayMode(__fastcall*)();
using ChartConvert = void(__fastcall*)(void*, std::uint32_t);
using Decode = void*(__fastcall*)(void*, const void*, std::size_t);
using S3pDecode = void*(__fastcall*)(void*, const void*, std::uint32_t);
using LoopVoice = void*(__fastcall*)(void*, void*, void*, std::int32_t, std::int32_t);
using InvalidPlay = char(__fastcall*)(std::uint32_t, char);
using Dispatch = std::int32_t(__fastcall*)(void*, std::int32_t*, std::uint32_t, void*, void*);
using GameplaySetup = char(__fastcall*)(void*, void*, std::int32_t, std::int32_t, std::uint32_t,
                                        void*);
using SoundLoad = std::uint32_t(__fastcall*)(std::uint32_t, const char*);

ChartConvert original_chart{};
Decode original_decode{};
S3pDecode original_s3p_decode{};
LoopVoice original_loop{};
InvalidPlay original_invalid{};
Dispatch original_dispatch{};
GameplaySetup original_setup{};
SoundLoad original_sound_load{};
// Chart setup and the audio worker share these latched values, not pending hotkey edits.
std::atomic<double> song_speed{1.0};
std::atomic<bool> song_preserve_pitch{false};
// Limit retiming to calls made within gameplay setup or its audio-bank loader.
thread_local double chart_speed = 1.0;
thread_local double bank_speed = 1.0;
std::byte* game_base{};
const versions::Profile* game_profile{};

struct BankLoadContext {
    bool preserve_pitch{};
    ProcessedWave last_wave{};
    std::size_t sample_count{};
    double longest_source_seconds{};
    double longest_output_seconds{};

    void record_wave(const ProcessedWave& processed) {
        // The following loop-voice call must refer to the most recently decoded sample.
        last_wave = processed;
        ++sample_count;
        const auto source_seconds =
            static_cast<double>(processed.original_frames) / processed.sample_rate;
        if (source_seconds > longest_source_seconds) {
            longest_source_seconds = source_seconds;
            longest_output_seconds = static_cast<double>(processed.frames) / processed.sample_rate;
        }
    }
};
thread_local BankLoadContext* bank_load{};

template <class Value> struct ScopedValue {
    Value& target;
    Value previous;

    ScopedValue(Value& target, Value value)
        : target(target), previous(std::exchange(target, value)) {}
    ScopedValue(const ScopedValue&) = delete;
    ScopedValue& operator=(const ScopedValue&) = delete;

    ~ScopedValue() {
        target = previous;
    }
};

char __fastcall setup_hook(void* gameplay, void* music, std::int32_t first_chart,
                           std::int32_t second_chart, std::uint32_t style, void* options) {
    const auto mode =
        reinterpret_cast<GetGameplayMode>(game_base + game_profile->gameplay_mode.rva)();
    const auto rate_allowed = mode == GameplayMode::Standard || mode == GameplayMode::StepUp ||
                              mode == GameplayMode::PremiumFree;
    const auto requested_rate = active.load() ? pending_speed.load() : 1.0;
    const auto rate = rate_allowed ? requested_rate : 1.0;
    const auto pitch = pending_preserve_pitch.load();
    song_preserve_pitch.store(pitch);
    song_speed.store(rate);
    ScopedValue scope(chart_speed, rate);
    if (active.load()) {
        try {
            const auto message =
                "Play rate: " + rate_text(rate) +
                (!rate_allowed && requested_rate != 1.0 ? ". FREQ unavailable in this mode" : "") +
                (pitch ? ". Preserve pitch ON" : ". Preserve pitch OFF") +
                (rate == 1.0 ? ". Normal song scoring." : ". No score saving.");
            log_msg(message);
            show_toast(SPICE_SDK_TOAST_LEVEL_INFO, message);
        } catch (...) {
        }
    }
    return original_setup(gameplay, music, first_chart, second_chart, style, options);
}

bool is_gameplay_bank(const char* path) {
    if (!path) {
        return false;
    }
    const std::string_view filename(path);
    return filename.ends_with(".2dx") || filename.ends_with(".s3p");
}

std::uint32_t __fastcall sound_load_hook(std::uint32_t group, const char* path) {
    const auto rate =
        active.load() && group == game_profile->gameplay_sound_group ? song_speed.load() : 1.0;
    ScopedValue scope(bank_speed, rate);
    BankLoadContext context;
    context.preserve_pitch = song_preserve_pitch.load();
    ScopedValue loading_scope(bank_load, rate != 1.0 ? &context : nullptr);
    // Previews and menu audio must also clear any outer gameplay loading context.
    if (rate == 1.0) {
        return original_sound_load(group, path);
    }
    if (!is_gameplay_bank(path)) {
        log_fatal("Rate mode requires a 2dx or s3p gameplay audio bank.");
    }
    const auto result = original_sound_load(group, path);
    if (result != 0 || !context.last_wave.wave) {
        log_fatal("Gameplay audio failed to decode. Rate mode cannot continue with missing audio.");
    }
    try {
        log_msg("Gameplay audio bank: " + std::string(path) + " rate=" + rate_text(rate) +
                " samples=" + std::to_string(context.sample_count) +
                " longest=" + std::to_string(context.longest_source_seconds) + "s -> " +
                std::to_string(context.longest_output_seconds) + "s");
    } catch (...) {
    }
    return result;
}

void __fastcall chart_hook(void* buffer, std::uint32_t player) {
    if (active.load() && chart_speed != 1.0) {
        try {
            const auto chart = transform_chart(
                {static_cast<const ChartEvent*>(buffer), game_profile->max_chart_events},
                chart_speed);
            if (!chart) {
                log_fatal(chart.error.c_str());
            }
            std::memcpy(buffer, chart.value.data(), chart.value.size() * sizeof(ChartEvent));
            log_msg("Transformed chart for player " + std::to_string(player));
        } catch (const std::exception& error) {
            log_fatal(error.what());
        }
    }
    original_chart(buffer, player);
}

void process_loaded_wave(void* output) {
    if (!active.load() || !bank_load) {
        return;
    }
    try {
        const auto result = process_wave(output, game_base, game_profile->wave, bank_speed,
                                         bank_load->preserve_pitch);
        if (!result) {
            log_fatal(result.error.c_str());
        }
        bank_load->record_wave(result.value);
    } catch (const std::exception& error) {
        log_fatal(error.what());
    }
}

void* __fastcall decode_hook(void* output, const void* input, std::size_t size) {
    auto* result = original_decode(output, input, size);
    process_loaded_wave(output);
    return result;
}

void* __fastcall s3p_decode_hook(void* output, const void* input, std::uint32_t size) {
    auto* result = original_s3p_decode(output, input, size);
    process_loaded_wave(output);
    return result;
}

struct LoopBounds {
    std::int32_t begin;
    std::int32_t end;
};

LoopBounds transform_loop_bounds(LoopBounds bounds, const ProcessedWave& wave, double rate) {
    if ((bounds.begin >= 0 && static_cast<std::size_t>(bounds.begin) > wave.original_frames) ||
        (bounds.end >= 0 && static_cast<std::size_t>(bounds.end) > wave.original_frames)) {
        log_fatal("Unsupported loop boundaries in a gameplay sample.");
    }
    // Negative endpoints are native sentinels for the start/end of the whole sample.
    if (bounds.begin >= 0) {
        bounds.begin = static_cast<std::int32_t>(std::llround(bounds.begin / rate));
    }
    if (bounds.end >= 0) {
        bounds.end = static_cast<std::int32_t>(std::llround(bounds.end / rate));
    }
    const auto effective_begin = bounds.begin < 0 ? 0U : static_cast<std::size_t>(bounds.begin);
    const auto effective_end = bounds.end < 0 ? wave.frames : static_cast<std::size_t>(bounds.end);
    if (effective_end < effective_begin || effective_end > wave.frames) {
        log_fatal("Scaled loop boundaries do not fit the decoded sample.");
    }
    return bounds;
}

void* __fastcall loop_hook(void* device, void* output, void* wave_shared, std::int32_t begin,
                           std::int32_t end) {
    if (active.load() && bank_load) {
        void* wave{};
        std::memcpy(&wave,
                    static_cast<const std::byte*>(wave_shared) + game_profile->wave.shared_wave,
                    sizeof(wave));
        if (wave != bank_load->last_wave.wave) {
            log_fatal("Unsupported loop boundaries in a gameplay sample.");
        }
        const auto bounds = transform_loop_bounds({begin, end}, bank_load->last_wave, bank_speed);
        begin = bounds.begin;
        end = bounds.end;
    }
    return original_loop(device, output, wave_shared, begin, end);
}

char __fastcall invalid_hook(std::uint32_t player, char flag) {
    // Use the game's native no-save result path; this still transmits play history.
    return active.load() && song_speed.load() != 1.0 ? 1 : original_invalid(player, flag);
}

bool allow_request(std::uint32_t request) {
    const auto song_scores = game_profile->song_score_requests;
    const auto aggregate_scores = game_profile->aggregate_score_requests;

    // Block normal score registration for the current song unless it was loaded at 1x.
    if (song_speed.load() != 1.0 && std::ranges::find(song_scores, request) != song_scores.end()) {
        return false;
    }
    if (std::ranges::find(aggregate_scores, request) != aggregate_scores.end()) {
        return false;
    }
    // Native no-save history and profile saves remain allowed.
    return true;
}

std::int32_t __fastcall dispatch_hook(void* context, std::int32_t* ticket, std::uint32_t request,
                                      void* payload, void* extra) {
    if (active.load() && !allow_request(request)) {
        try {
            log_msg("Blocked request ID " + std::to_string(request));
        } catch (...) {
        }
        return game_profile->denied_request_result;
    }
    return original_dispatch(context, ticket, request, payload, extra);
}

struct Hook {
    const char* name;
    const versions::HookPoint& point;
    void* detour;
    void** original;
};

std::string select_game_profile(std::string_view model) {
    HMODULE identified_module{};
    std::string identifier;
    for (const auto* profile : versions::profiles) {
        const auto module = GetModuleHandleW(profile->module_name);
        if (!module) {
            continue;
        }
        if (module != identified_module) {
            const auto path = module_path(module);
            if (!path) {
                return path.error;
            }
            const auto identity = pe_identifier(path.value, model);
            if (!identity) {
                return identity.error;
            }
            identifier = identity.value;
            identified_module = module;
            log_msg("DLL patch identifier: " + identifier);
        }
        if (identifier != profile->pe_identifier) {
            continue;
        }
        game_profile = profile;
        game_base = reinterpret_cast<std::byte*>(module);
        break;
    }
    if (!game_profile) {
        return "No supported game DLL profile matched. No rate hooks were installed.";
    }
    log_msg("Selected profile: " + std::string(game_profile->pe_identifier));
    return {};
}

}

void install_game_hooks(std::string_view model) noexcept try {
    const auto error = select_game_profile(model);
    if (!error.empty()) {
        log_fatal(error.c_str());
    }
    const auto& mode = game_profile->gameplay_mode;
    if (mode.rva == 0 ||
        std::memcmp(game_base + mode.rva, mode.bytes.data(), mode.bytes.size()) != 0) {
        log_fatal("Mode-getter bytes differ. No rate hooks installed.");
    }
    const auto& points = game_profile->hooks;
    const std::array hooks{
        Hook{"chart_convert", points.chart_convert, reinterpret_cast<void*>(&chart_hook),
             reinterpret_cast<void**>(&original_chart)},
        Hook{"decode", points.decode, reinterpret_cast<void*>(&decode_hook),
             reinterpret_cast<void**>(&original_decode)},
        Hook{"s3p_decode", points.s3p_decode, reinterpret_cast<void*>(&s3p_decode_hook),
             reinterpret_cast<void**>(&original_s3p_decode)},
        Hook{"loop_voice", points.loop_voice, reinterpret_cast<void*>(&loop_hook),
             reinterpret_cast<void**>(&original_loop)},
        Hook{"invalid_play", points.invalid_play, reinterpret_cast<void*>(&invalid_hook),
             reinterpret_cast<void**>(&original_invalid)},
        Hook{"dispatch", points.dispatch, reinterpret_cast<void*>(&dispatch_hook),
             reinterpret_cast<void**>(&original_dispatch)},
        Hook{"gameplay_setup", points.gameplay_setup, reinterpret_cast<void*>(&setup_hook),
             reinterpret_cast<void**>(&original_setup)},
        Hook{"sound_load", points.sound_load, reinterpret_cast<void*>(&sound_load_hook),
             reinterpret_cast<void**>(&original_sound_load)}};
    // Verify every entry point before modifying any game code.
    for (const auto& hook : hooks) {
        if (std::memcmp(game_base + hook.point.rva, hook.point.bytes.data(),
                        hook.point.bytes.size()) != 0) {
            const auto message =
                std::string("Hook-site bytes differ for ") + hook.name + ". No hooks installed.";
            log_fatal(message.c_str());
        }
    }
    if (MH_Initialize() != MH_OK) {
        log_fatal("MinHook initialization failed.");
    }
    for (const auto& hook : hooks) {
        if (MH_CreateHook(game_base + hook.point.rva, hook.detour, hook.original) != MH_OK) {
            MH_Uninitialize();
            log_fatal("Cannot create all required hooks.");
        }
    }
    HMODULE self{};
    // Detours and the replacement wave allocator must outlive all game-owned objects.
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(&install_game_hooks), &self)) {
        MH_Uninitialize();
        log_fatal("Cannot pin the hook DLL for the lifetime of the game.");
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        if (MH_Uninitialize() != MH_OK) {
            log_fatal("Failed to roll back incomplete hook installation.");
        }
        log_fatal("Cannot enable all required hooks.");
    }
    active.store(true);
} catch (const std::exception& error) {
    log_fatal(error.what());
} catch (...) {
    log_fatal("Unexpected failure while installing game hooks.");
}

}