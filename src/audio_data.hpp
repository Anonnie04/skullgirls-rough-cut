#pragma once
#include "foit_script.hpp"
#include <map>
#include <memory>

namespace skullgirls {
struct AudioWave {
    SpriteBytes format, data;
    unsigned sample_rate = 0, channels = 0, frames = 0;
    unsigned loop_begin = 0, loop_length = 0;
};
AudioWave read_audio_wave(const SpriteBytes& bytes);

struct SoundChoice {
    std::array<std::int32_t, 10> modifiers{};
    float weight = 1;
    bool detached = false;
    unsigned script_value = 0, gain_percent = 100, mode = 0;
    std::vector<std::uint16_t> waves;
    std::vector<SoundChoice> children;
};
struct SoundGroup {
    std::vector<FoitExpression> condition;
    bool completion_flag = false;
    std::vector<SoundChoice> choices;
};
struct SoundBank {
    std::map<std::uint32_t, std::vector<SoundGroup>> events;
    std::vector<std::shared_ptr<AudioWave>> waves;
};
SoundBank read_sound_bank(const SpriteBytes& bytes);

struct MusicTrack {
    std::string name, intro, loop;
    std::vector<std::string> stages;
};
std::vector<MusicTrack> read_music_tracks(const SpriteBytes& config);
}
