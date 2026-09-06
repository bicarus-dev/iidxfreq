#include "wave.h"
#include "audio.h"

#include <windows.h>
#include <mmreg.h>

#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <string>

namespace iidxfreq {
namespace {

template <class Value> Value read(const void* object, std::size_t offset) {
    Value value;
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
    return value;
}

template <class Value> void write(void* object, std::size_t offset, Value value) {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

void __fastcall free_wave_allocation(void* allocation) {
    HeapFree(GetProcessHeap(), 0, allocation);
}

struct DecodedWave {
    void* object;
    WAVEFORMATEX* format;
    std::size_t format_bytes;
    std::int16_t* pcm;
    std::size_t pcm_bytes;
    std::size_t allocation_bytes;
    std::byte* riff;

    std::size_t header_bytes() const {
        return allocation_bytes - pcm_bytes;
    }
};

bool is_pcm_format(const WAVEFORMATEX* format, std::size_t format_bytes) {
    if (!format || format_bytes < offsetof(WAVEFORMATEX, cbSize)) {
        return false;
    }
    if (format->wFormatTag == WAVE_FORMAT_PCM) {
        return true;
    }
    // S3P decoding can return extensible PCM rather than the ordinary RIFF format tag.
    if (format_bytes >= sizeof(WAVEFORMATEXTENSIBLE) &&
        format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
        constexpr GUID pcm_subtype{1, 0, 0x10, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
        const auto extended = read<WAVEFORMATEXTENSIBLE>(format, 0);
        return extended.Samples.wValidBitsPerSample == 16 &&
               std::memcmp(&extended.SubFormat, &pcm_subtype, sizeof(GUID)) == 0;
    }
    return false;
}

Result<DecodedWave> read_decoded_wave(void* output, const std::byte* module_base,
                                      const versions::WaveLayout& layout) {
    auto* object = read<void*>(output, layout.shared_wave);
    if (!object || read<void*>(object, layout.vtable) != module_base + layout.vtable_rva) {
        return Error{"Unexpected decoded wave object."};
    }
    DecodedWave wave{object,
                     read<WAVEFORMATEX*>(object, layout.format),
                     read<std::size_t>(object, layout.format_size),
                     read<std::int16_t*>(object, layout.pcm),
                     read<std::size_t>(object, layout.pcm_size),
                     read<std::size_t>(object, layout.allocation_size),
                     read<std::byte*>(object, layout.riff)};
    if (!is_pcm_format(wave.format, wave.format_bytes) || !wave.pcm || !wave.riff ||
        wave.format->wBitsPerSample != 16 ||
        wave.format->nBlockAlign != wave.format->nChannels * sizeof(std::int16_t) ||
        wave.pcm_bytes == 0 || wave.pcm_bytes % sizeof(std::int16_t) != 0 ||
        wave.pcm_bytes > 512 * 1024 * 1024 || wave.allocation_bytes < wave.pcm_bytes + 8) {
        return Error{"Unsupported decoded PCM layout."};
    }
    return wave;
}

std::string grow_wave(DecodedWave& wave, const versions::WaveLayout& layout,
                      std::size_t new_bytes) {
    using FreeAllocation = void(__fastcall*)(void*);
    const auto old_free = read<FreeAllocation>(wave.object, layout.free_callback);
    auto* old_allocation = read<void*>(wave.object, layout.allocation);
    const auto riff_address = reinterpret_cast<std::uintptr_t>(wave.riff);
    const auto pcm_address = reinterpret_cast<std::uintptr_t>(wave.pcm);
    const auto format_address = reinterpret_cast<std::uintptr_t>(wave.format);
    const auto header_bytes = wave.header_bytes();
    if (!old_free || !old_allocation || pcm_address < riff_address ||
        pcm_address - riff_address != header_bytes || format_address < riff_address ||
        format_address - riff_address > header_bytes ||
        wave.format_bytes > header_bytes - (format_address - riff_address)) {
        return "Cannot grow an unexpected wave allocation layout.";
    }
    const auto format_offset = format_address - riff_address;
    std::unique_ptr<std::byte, decltype(&free_wave_allocation)> allocation(
        static_cast<std::byte*>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, header_bytes + new_bytes + 15)),
        &free_wave_allocation);
    if (!allocation) {
        return "Cannot allocate expanded song audio.";
    }
    // Align the PCM payload, not the RIFF header, to the game's 16-byte boundary.
    auto* new_riff = allocation.get() + ((16 - header_bytes % 16) % 16);
    std::memcpy(new_riff, wave.riff, header_bytes);
    wave.riff = new_riff;
    wave.pcm = reinterpret_cast<std::int16_t*>(wave.riff + header_bytes);
    wave.format = reinterpret_cast<WAVEFORMATEX*>(wave.riff + format_offset);

    // The game owns the replacement; free the old block with its original allocator.
    write(wave.object, layout.allocation, allocation.release());
    write(wave.object, layout.free_callback, &free_wave_allocation);
    write(wave.object, layout.riff, wave.riff);
    write(wave.object, layout.format, wave.format);
    write(wave.object, layout.pcm, wave.pcm);
    old_free(old_allocation);
    return {};
}

void store_pcm(const DecodedWave& wave, const versions::WaveLayout& layout,
               std::span<const std::int16_t> samples) {
    const auto new_bytes = samples.size_bytes();
    std::memcpy(wave.pcm, samples.data(), new_bytes);
    if (new_bytes < wave.pcm_bytes) {
        std::memset(reinterpret_cast<std::byte*>(wave.pcm) + new_bytes, 0,
                    wave.pcm_bytes - new_bytes);
    }
    const auto total_bytes = wave.header_bytes() + new_bytes;
    // Keep the game's cached lengths and the RIFF/data chunk lengths in agreement.
    write(wave.object, layout.pcm_size, new_bytes);
    write(wave.object, layout.allocation_size, total_bytes);
    write(wave.riff, 4, static_cast<std::uint32_t>(total_bytes - 8));
    write(reinterpret_cast<std::byte*>(wave.pcm) - 4, 0, static_cast<std::uint32_t>(new_bytes));
}

}

Result<ProcessedWave> process_wave(void* output, const std::byte* module_base,
                                   const versions::WaveLayout& layout, double speed,
                                   bool preserve_pitch) {
    auto decoded = read_decoded_wave(output, module_base, layout);
    if (!decoded) {
        return Error{decoded.error};
    }
    auto& wave = decoded.value;
    const auto transformed =
        process_audio({wave.pcm, wave.pcm_bytes / sizeof(std::int16_t)}, wave.format->nChannels,
                      wave.format->nSamplesPerSec, speed, preserve_pitch);
    if (!transformed) {
        return Error{transformed.error};
    }
    const auto new_bytes = transformed.value.size() * sizeof(std::int16_t);
    const auto header_bytes = wave.header_bytes();
    if (new_bytes == 0 || header_bytes > std::numeric_limits<std::uint32_t>::max() ||
        new_bytes > std::numeric_limits<std::uint32_t>::max() - header_bytes) {
        return Error{"Processed PCM exceeds the wave size limit."};
    }
    if (new_bytes > wave.pcm_bytes) {
        const auto error = grow_wave(wave, layout, new_bytes);
        if (!error.empty()) {
            return Error{error};
        }
    }
    store_pcm(wave, layout, transformed.value);
    return ProcessedWave{wave.object, transformed.value.size() / wave.format->nChannels,
                         wave.pcm_bytes / wave.format->nBlockAlign, wave.format->nSamplesPerSec};
}

}