#include "audio.h"
#include "rate.h"

#include <SoundTouch.h>
#include <algorithm>
#include <array>

namespace iidxfreq {
namespace {

constexpr std::uint32_t block_frames = 4096;
constexpr std::uint32_t maximum_channels = 6;
constexpr float pcm16_scale = 32768.0f;

float to_float(std::int16_t sample) {
    return sample / pcm16_scale;
}

std::int16_t to_pcm16(float sample) {
    return static_cast<std::int16_t>(
        std::clamp(std::lround(sample * pcm16_scale), -32768L, 32767L));
}

void receive_processed_samples(soundtouch::SoundTouch& processor, std::uint32_t channels,
                               std::span<float> buffer, std::span<std::int16_t>& remaining_output) {
    while (true) {
        const auto received_frames = processor.receiveSamples(buffer.data(), block_frames);
        if (received_frames == 0) {
            return;
        }

        const std::size_t received_samples = received_frames * channels;
        const auto samples_to_copy = std::min(received_samples, remaining_output.size());
        std::ranges::transform(buffer.first(samples_to_copy), remaining_output.begin(), to_pcm16);
        remaining_output = remaining_output.subspan(samples_to_copy);
        // Discard excess output, but keep draining until SoundTouch has no samples ready.
    }
}

}

Result<std::vector<std::int16_t>> process_audio(std::span<const std::int16_t> samples,
                                                std::uint32_t channels, std::uint32_t sample_rate,
                                                double rate, bool preserve_pitch) {

    if (!valid_rate(rate) || (channels != 1 && channels != 2 && channels != 6) ||
        sample_rate < 8000 || sample_rate > 192000 || samples.size() % channels != 0) {
        return Error{"Unsupported audio format or speed."};
    }
    if (rate == 1.0 || samples.empty()) {
        return std::vector<std::int16_t>(samples.begin(), samples.end());
    }

    // SoundTouch counts frames: one frame contains one sample for each channel.
    const auto input_frames = samples.size() / channels;
    const auto output_frames = static_cast<std::size_t>(std::llround(input_frames / rate));
    std::vector<std::int16_t> output(output_frames * channels);
    soundtouch::SoundTouch processor;
    processor.setChannels(channels);
    processor.setSampleRate(sample_rate);

    // Tempo changes retain pitch; rate changes resample speed and pitch together.
    if (preserve_pitch) {
        processor.setTempo(rate);
    } else {
        processor.setRate(rate);
    }

    // Feed PCM16 input in float blocks, draining processed audio after each block so
    // SoundTouch does not buffer the entire sample before returning output.
    std::array<float, block_frames * maximum_channels> buffer{};
    std::span<std::int16_t> remaining_output(output);
    for (std::size_t frame_offset = 0; frame_offset < input_frames;) {
        const auto frames_to_send = static_cast<std::uint32_t>(
            std::min<std::size_t>(block_frames, input_frames - frame_offset));
        const auto input_block =
            samples.subspan(frame_offset * channels, frames_to_send * channels);
        std::ranges::transform(input_block, buffer.begin(), to_float);
        processor.putSamples(buffer.data(), frames_to_send);

        receive_processed_samples(processor, channels, buffer, remaining_output);
        frame_offset += frames_to_send;
    }

    // Release the processor's buffered tail after the last input block.
    processor.flush();
    receive_processed_samples(processor, channels, buffer, remaining_output);

    // Allow at most one zero-filled frame for rounding differences at the end.
    if (remaining_output.size() > channels) {
        return Error{"Audio processor returned too few frames."};
    }

    return output;
}

}