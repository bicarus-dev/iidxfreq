#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace iidxfreq::versions {

// Native request IDs: song registration; app, best, and course aggregate results.
inline constexpr std::array<std::uint32_t, 1> song_score_requests{29};
inline constexpr std::array<std::uint32_t, 3> aggregate_score_requests{14, 26, 38};

struct HookPoint {
    std::size_t rva;
    // Expected entry bytes guard against hooking a different DLL build at the same RVA.
    std::array<std::uint8_t, 15> bytes;
};

struct HookPoints {
    HookPoint chart_convert;
    HookPoint decode;
    HookPoint s3p_decode;
    HookPoint loop_voice;
    HookPoint invalid_play;
    HookPoint dispatch;
    HookPoint gameplay_setup;
    HookPoint sound_load;
};

// Byte offsets in the game's decoded wave object, except shared_wave in its owning wrapper.
// vtable_rva is module-relative; allocation_size counts RIFF bytes, not allocator padding.
struct WaveLayout {
    std::size_t vtable_rva;
    std::size_t shared_wave = 0;
    std::size_t vtable = 0;
    std::size_t allocation_size = 8;
    std::size_t free_callback = 16;
    std::size_t allocation = 24;
    std::size_t riff = 32;
    std::size_t format = 40;
    std::size_t format_size = 48;
    std::size_t pcm = 56;
    std::size_t pcm_size = 64;
};

struct Profile {
    const wchar_t* module_name = L"bm2dx.dll";
    std::string_view pe_identifier;
    HookPoints hooks;
    HookPoint gameplay_mode{};
    WaveLayout wave;
    std::size_t max_chart_events = 0x3000;
    std::uint32_t gameplay_sound_group = 2;
    std::span<const std::uint32_t> song_score_requests = versions::song_score_requests;
    std::span<const std::uint32_t> aggregate_score_requests = versions::aggregate_score_requests;
    std::int32_t denied_request_result = 2;
};

}