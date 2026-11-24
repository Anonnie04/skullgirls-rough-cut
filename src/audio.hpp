#pragma once
#include "foit_script.hpp"
#include <filesystem>
#include <memory>
#include <string>

namespace skullgirls {
struct AudioEventResult {
    std::array<std::uint32_t, 5> forwarded{};
    std::optional<float> script_value;
};
// Character names for source operands: self, opponent, opponent's opponent,
// and the current target. Empty entries retain the literal hash.
using AudioEventNames = std::array<std::string, 4>;
// The shared preview only depends on this interface; tests and the Windows
// preview can leave it unset. Native Linux supplies the FAudio implementation.
class AudioOutput {
public:
    virtual ~AudioOutput() = default;
    virtual void ui(const std::string& name) = 0;
    virtual void music(const std::string& intro, const std::string& loop) = 0;
    virtual void stage_music(const std::string& stage, const std::string& parent) = 0;
    virtual void prepare_character(const std::string& name) = 0;
    virtual AudioEventResult event(const std::string& bank, std::uint32_t id, const FoitScriptState& context, float pan, unsigned actor, const AudioEventNames& names = {}) = 0;
    virtual void volumes(float effects, float music, float voices) = 0;
    virtual void pause_combat(bool paused) = 0;
    virtual void stop_combat() = 0;
    virtual void tick() = 0;
};
std::shared_ptr<AudioOutput> create_linux_audio(const std::filesystem::path& data);
void run_audio_smoke_test(const std::filesystem::path& data);
}
