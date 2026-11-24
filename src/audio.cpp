#include "audio.hpp"
#include "audio_data.hpp"
#include "audio_playback.hpp"
#include "sprite_asset.hpp"
#include "third_party/FAudio.h"
#include <dlfcn.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <random>
#include <thread>

namespace skullgirls {
namespace {
struct FAudioApi {
    void* library = nullptr;
#define AUDIO_FUNCTION(name) decltype(&::name) name = nullptr
    AUDIO_FUNCTION(FAudioCreate);
    AUDIO_FUNCTION(FAudio_Release);
    AUDIO_FUNCTION(FAudio_CreateMasteringVoice);
    AUDIO_FUNCTION(FAudio_CreateSourceVoice);
    AUDIO_FUNCTION(FAudioVoice_DestroyVoice);
    AUDIO_FUNCTION(FAudioVoice_SetVolume);
    AUDIO_FUNCTION(FAudioVoice_SetOutputMatrix);
    AUDIO_FUNCTION(FAudioSourceVoice_Start);
    AUDIO_FUNCTION(FAudioSourceVoice_Stop);
    AUDIO_FUNCTION(FAudioSourceVoice_SubmitSourceBuffer);
    AUDIO_FUNCTION(FAudioSourceVoice_GetState);
    AUDIO_FUNCTION(FAudio_SetEngineProcedureEXT);
    AUDIO_FUNCTION(FAudio_GetProcessingQuantum);
#undef AUDIO_FUNCTION
    explicit FAudioApi(const std::filesystem::path& data) {
        library=dlopen("libFAudio.so.0",RTLD_NOW|RTLD_LOCAL);
        if(!library) {
            const auto game=std::filesystem::canonical(data).parent_path();
            const auto local=game/"lib/x86_64-pc-linux-gnu/libFAudio.so.0";
            library=dlopen(local.c_str(),RTLD_NOW|RTLD_LOCAL);
        }
        if(!library) throw std::runtime_error(std::string("Cannot load FAudio: ")+dlerror());
        try {
#define LOAD_AUDIO(name) name=reinterpret_cast<decltype(name)>(dlsym(library,#name)); if(!name)throw std::runtime_error("FAudio is missing " #name)
            LOAD_AUDIO(FAudioCreate); LOAD_AUDIO(FAudio_Release);
            LOAD_AUDIO(FAudio_CreateMasteringVoice); LOAD_AUDIO(FAudio_CreateSourceVoice);
            LOAD_AUDIO(FAudioVoice_DestroyVoice); LOAD_AUDIO(FAudioVoice_SetVolume);
            LOAD_AUDIO(FAudioVoice_SetOutputMatrix); LOAD_AUDIO(FAudioSourceVoice_Start);
            LOAD_AUDIO(FAudioSourceVoice_Stop); LOAD_AUDIO(FAudioSourceVoice_SubmitSourceBuffer);
            LOAD_AUDIO(FAudioSourceVoice_GetState);
            // Diagnostic capture is optional during normal playback.
            FAudio_SetEngineProcedureEXT=reinterpret_cast<decltype(FAudio_SetEngineProcedureEXT)>(dlsym(library,"FAudio_SetEngineProcedureEXT"));
            FAudio_GetProcessingQuantum=reinterpret_cast<decltype(FAudio_GetProcessingQuantum)>(dlsym(library,"FAudio_GetProcessingQuantum"));
#undef LOAD_AUDIO
        } catch(...) { dlclose(library); library=nullptr; throw; }
    }
    ~FAudioApi(){if(library)dlclose(library);}
};
void check_audio(std::uint32_t result,const char* operation) {
    if(result)throw std::runtime_error(std::string("FAudio ")+operation+" failed ("+std::to_string(result)+")");
}

class GameAudio final : public AudioOutput {
    enum class Bus { effects, music, voices };
    struct Voice {
        FAudioSourceVoice* handle=nullptr;
        std::vector<std::shared_ptr<AudioWave>> waves;
        Bus bus=Bus::effects;
        bool combat=false, retiring=false;
        unsigned actor=unsigned(-1);
        bool detached=false;
        float gain=1, fade=1;
        std::chrono::steady_clock::time_point changed;
    };
    FAudioApi api;
    FAudio* engine=nullptr;
    FAudioMasteringVoice* master=nullptr;
    SpriteArchive ui_archive, music_archive, character_archive;
    std::vector<MusicTrack> tracks;
    std::map<std::string,std::shared_ptr<AudioWave>> ui_cache;
    std::map<std::string,SoundBank> banks;
    std::map<const SoundGroup*,std::size_t> previous_choices;
    std::vector<Voice> playing;
    std::mt19937 generator{std::random_device{}()};
    std::string music_key;
    std::array<float,3> gains{0.8f,0.65f,0.85f};
    bool combat_paused=false;
    std::atomic<std::uint64_t> rendered_samples{0}, nonzero_samples{0};
    unsigned quantum=0;

    float volume(const Voice& voice)const {return voice.gain*voice.fade*gains[unsigned(voice.bus)];}
    void destroy(Voice& voice) {
        if(voice.handle)api.FAudioVoice_DestroyVoice(voice.handle);
        voice.handle=nullptr; voice.waves.clear();
    }
    void cleanup() {
        for(auto& voice:playing)destroy(voice);
        playing.clear();
        if(master)api.FAudioVoice_DestroyVoice(master);
        master=nullptr;
        if(engine)api.FAudio_Release(engine);
        engine=nullptr;
    }
    FAudioSourceVoice* submit(std::vector<std::shared_ptr<AudioWave>> waves,Bus bus,bool combat,
                              float gain,float pan,unsigned actor,bool loop) {
        tick();
        if(playing.size()>=80) {
            const auto oldest=std::find_if(playing.begin(),playing.end(),[](const Voice& v){return v.bus!=Bus::music;});
            if(oldest==playing.end())return nullptr;
            destroy(*oldest);playing.erase(oldest);
        }
        Voice voice;
        voice.waves=std::move(waves);voice.bus=bus;voice.combat=combat;voice.gain=gain;voice.actor=actor;
        const auto& format=voice.waves.back()->format;
        check_audio(api.FAudio_CreateSourceVoice(engine,&voice.handle,
            reinterpret_cast<const FAudioWaveFormatEx*>(format.data()),0,2,nullptr,nullptr,nullptr),"create source voice");
        try {
            for(std::size_t i=0;i<voice.waves.size();++i) {
                const auto& wave=*voice.waves[i];
                if(wave.channels!=voice.waves.back()->channels || wave.format.size()!=format.size() ||
                   !std::equal(wave.format.begin(),wave.format.begin()+4,format.begin()) ||
                   !std::equal(wave.format.begin()+12,wave.format.end(),format.begin()+12))
                    throw std::runtime_error("Intro and loop audio formats do not match");
                FAudioBuffer buffer{};
                buffer.AudioBytes=wave.data.size();buffer.pAudioData=wave.data.data();
                buffer.PlayLength=wave.frames;
                if(loop && i+1==voice.waves.size()) {
                    buffer.LoopBegin=wave.loop_begin;buffer.LoopLength=wave.loop_length;
                    buffer.LoopCount=FAUDIO_LOOP_INFINITE;
                } else if(i+1==voice.waves.size())buffer.Flags=FAUDIO_END_OF_STREAM;
                check_audio(api.FAudioSourceVoice_SubmitSourceBuffer(voice.handle,&buffer,nullptr),"submit sound buffer");
            }
            if(bus==Bus::music) { voice.fade=0;voice.changed=std::chrono::steady_clock::now(); }
            check_audio(api.FAudioVoice_SetVolume(voice.handle,volume(voice),0),"set voice volume");
            if(combat && voice.waves.front()->channels==1) {
                pan=std::clamp(pan,-1.0f,1.0f);
                const float matrix[]={std::sqrt((1-pan)*0.5f),std::sqrt((1+pan)*0.5f)};
                check_audio(api.FAudioVoice_SetOutputMatrix(voice.handle,master,1,2,matrix,0),"pan sound");
            }
            if(!combat || !combat_paused)check_audio(api.FAudioSourceVoice_Start(voice.handle,0,0),"start voice");
            const auto handle=voice.handle;
            playing.push_back(std::move(voice));return handle;
        } catch(...) {destroy(voice);throw;}
    }
    void play_choice(const SoundChoice& selected,const SoundBank& bank,float pan,unsigned actor) {
        const auto layers=sound_layers(selected);
        for (std::size_t i=0;i<layers.size();++i) {
            const auto& choice=*layers[i];
            const auto bus=sound_voice_bus(choice.mode)?Bus::voices:Bus::effects;
            const bool detached=i!=0 || choice.detached;
            if (!detached)
                for (auto& voice:playing)
                    if(voice.combat && !voice.detached && voice.actor==actor && (voice.bus==bus || sound_interrupts(choice.mode))) destroy(voice);
            std::vector<std::shared_ptr<AudioWave>> waves;
            for(auto index:choice.waves) waves.push_back(bank.waves.at(index));
            if(waves.empty()) continue;
            if(submit(std::move(waves),bus,true,std::min(choice.gain_percent,100u)/100.f,pan,actor,false))
                playing.back().detached=detached;
        }
    }
    static void capture(FAudioEngineCallEXT render,FAudio* audio,float* output,void* user) {
        render(audio,output);
        auto& self=*static_cast<GameAudio*>(user);
        std::uint64_t audible=0;
        for(unsigned i=0;i<self.quantum*2;++i)if(std::abs(output[i])>0.00001f)++audible;
        self.rendered_samples.fetch_add(self.quantum*2,std::memory_order_relaxed);
        self.nonzero_samples.fetch_add(audible,std::memory_order_relaxed);
    }
public:
    explicit GameAudio(const std::filesystem::path& data,bool diagnostic=false)
        : api(data),ui_archive(data/"ui-win.gfs"),music_archive(data/"music-win.gfs"),character_archive(data/"characters-win.gfs") {
        try {
            tracks=read_music_tracks(SpriteArchive(data/"core.gfs").read("core/music.ini"));
            // Newer installed stages carry their track override in their .lvl file.
            check_audio(api.FAudioCreate(&engine,0,FAUDIO_DEFAULT_PROCESSOR),"create engine");
            check_audio(api.FAudio_CreateMasteringVoice(engine,&master,2,48000,0,0,nullptr),"open audio device");
            if(diagnostic) {
                if(!api.FAudio_SetEngineProcedureEXT || !api.FAudio_GetProcessingQuantum)throw std::runtime_error("FAudio diagnostic capture is unavailable");
                unsigned rate=0;api.FAudio_GetProcessingQuantum(engine,&quantum,&rate);
                api.FAudio_SetEngineProcedureEXT(engine,capture,this);
            }
            std::fprintf(stderr,"Audio: native FAudio, stereo 48000 Hz\n");
        }catch(...){cleanup();throw;}
    }
    ~GameAudio() override {cleanup();}
    void ui(const std::string& name) override {
        auto& wave=ui_cache[name];
        if(!wave)wave=std::make_shared<AudioWave>(read_audio_wave(ui_archive.read("ui/Win/UISound/"+name)));
        const bool voice=name.rfind("charconfirm.",0)==0 || name.rfind("selectconfirm.",0)==0;
        submit({wave},voice?Bus::voices:Bus::effects,false,1,0,0,false);
    }
    void music(const std::string& intro,const std::string& loop) override {
        const auto key=intro+"|"+loop;
        if(key==music_key)return;
        auto first=std::make_shared<AudioWave>(read_audio_wave(music_archive.read("music/"+intro+".wav")));
        auto repeat=std::make_shared<AudioWave>(read_audio_wave(music_archive.read("music/"+loop+".wav")));
        const auto now=std::chrono::steady_clock::now();
        for(auto& voice:playing)if(voice.bus==Bus::music){voice.retiring=true;voice.changed=now;}
        submit({first,repeat},Bus::music,false,1,0,0,true);
        music_key=key;
    }
    void stage_music(const std::string& stage,const std::string& parent) override {
        for (const auto& id : {stage, parent}) for(const auto& track:tracks)
            if(!id.empty() && std::find(track.stages.begin(),track.stages.end(),id)!=track.stages.end()) {
                music(track.intro,track.loop);return;
            }
        // The caller prefers stage-authored Music_Intro / Music_Loop overrides.
        music("blankmusic","blankmusic");
    }
    void prepare_character(const std::string& name) override {
        if(!banks.count(name))banks.emplace(name,read_sound_bank(character_archive.read("characters/"+name+".snd-wav")));
    }
    AudioEventResult event(const std::string& name,std::uint32_t id,const FoitScriptState& context,float pan,unsigned actor,const AudioEventNames& names = {}) override {
        const auto bank=banks.find(name);
        if(bank==banks.end())return {};
        const auto event=bank->second.events.find(id);
        if(event==bank->second.events.end()) {
            if(name!="hitspark") return this->event("hitspark",id,context,pan,actor,names);
            return {};
        }
        for(const auto& group:event->second) {
            if(group.choices.empty() || !test_foit_expressions(group.condition,context))continue;
            const auto previous=previous_choices.find(&group);
            const auto index=select_sound_choice(group,generator,previous==previous_choices.end() ?
                std::nullopt : std::optional<std::size_t>(previous->second));
            previous_choices[&group]=index;
            play_choice(group.choices[index],bank->second,pan,actor);
            auto sources=names;
            if(sources[0].empty()) sources[0]=name;
            return sound_event_result(group.choices[index],sources);
        }
        return {};
    }
    void volumes(float effects,float music,float voices) override {
        gains={std::clamp(effects,0.f,1.f),std::clamp(music,0.f,1.f),std::clamp(voices,0.f,1.f)};
        for(const auto& voice:playing)if(voice.handle)api.FAudioVoice_SetVolume(voice.handle,volume(voice),0);
    }
    void pause_combat(bool paused) override {
        if(paused==combat_paused)return;
        combat_paused=paused;
        for(auto& voice:playing)if(voice.handle && voice.combat) {
            if(paused)api.FAudioSourceVoice_Stop(voice.handle,0,0);
            else api.FAudioSourceVoice_Start(voice.handle,0,0);
        }
    }
    void stop_combat() override {for(auto& voice:playing)if(voice.combat)destroy(voice);combat_paused=false;}
    void tick() override {
        const auto now=std::chrono::steady_clock::now();
        for(auto& voice:playing) {
            if(!voice.handle)continue;
            if(voice.bus==Bus::music) {
                const float t=std::clamp(std::chrono::duration<float>(now-voice.changed).count()/0.25f,0.f,1.f);
                voice.fade=voice.retiring?1-t:t;
                if(voice.retiring && t>=1){destroy(voice);continue;}
                api.FAudioVoice_SetVolume(voice.handle,volume(voice),0);
            }
            FAudioVoiceState state{};api.FAudioSourceVoice_GetState(voice.handle,&state,0);
            if(!state.BuffersQueued)destroy(voice);
        }
        playing.erase(std::remove_if(playing.begin(),playing.end(),[](const Voice& v){return !v.handle;}),playing.end());
    }
    void verify() {
        for(const auto* name:{"filia","cerebella","peacock","parasoul","msfortune","painwheel","hitspark"})prepare_character(name);
        ui("menu.select.wav");
        for (const auto& pair : std::vector<std::pair<std::string,std::string>>{
                {"menu_intro","menu_loop"}, {"select_intro","select_loop"},
                {"sg_train_intro","sg_train_loop"}, {"skull02_1","skull02_2"}}) {
            music(pair.first,pair.second);
            for (auto& voice : playing) if (voice.bus==Bus::music) destroy(voice);
        }
        music_key.clear(); music("menu_intro","menu_loop");
        FoitScriptState context;for(auto& value:context.variables)value=0;for(auto& bit:context.bits)bit=false;
        event("filia",banks.at("filia").events.begin()->first,context,-0.5f,0);
        // Exercise the same queue path with a short intro followed by an endless
        // loop, so the test can observe multiple wraps without waiting for a song.
        auto tone=std::make_shared<AudioWave>();tone->channels=1;tone->sample_rate=48000;tone->frames=2400;
        const std::uint8_t format[]={1,0,1,0,0x80,0xbb,0,0,0,0x77,1,0,2,0,16,0,0,0};tone->format.assign(format,format+18);
        for(unsigned i=0;i<tone->frames;++i){const auto sample=std::int16_t(std::sin(i*0.0576)*1000);tone->data.push_back(sample&255);tone->data.push_back(unsigned(std::uint16_t(sample))>>8);}
        const auto loop=submit({tone,tone},Bus::effects,false,0.1f,0,0,true);
        for(unsigned i=0;i<30;++i){std::this_thread::sleep_for(std::chrono::milliseconds(20));tick();}
        FAudioVoiceState state{};api.FAudioSourceVoice_GetState(loop,&state,0);
        if(state.SamplesPlayed<tone->frames*3 || state.BuffersQueued!=1 || !nonzero_samples.load())
            throw std::runtime_error("Audio smoke check did not render samples or sustain the loop");
        const auto combat=submit({tone},Bus::effects,true,0.1f,0,1,true);
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        pause_combat(true);
        FAudioVoiceState paused{};api.FAudioSourceVoice_GetState(combat,&paused,0);
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        FAudioVoiceState still{};api.FAudioSourceVoice_GetState(combat,&still,0);
        if(still.SamplesPlayed!=paused.SamplesPlayed)throw std::runtime_error("Paused combat audio advanced");
        pause_combat(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        api.FAudioSourceVoice_GetState(combat,&still,0);
        if(still.SamplesPlayed<=paused.SamplesPlayed)throw std::runtime_error("Combat audio did not resume");
        stop_combat();
        for(const auto& voice:playing)if(voice.combat && voice.handle)throw std::runtime_error("Combat audio survived reset");
        SoundBank layered_bank; layered_bank.waves={tone};
        SoundChoice layered; layered.waves={0,0}; layered.children.resize(2);
        for(auto& child:layered.children) child.waves={0};
        play_choice(layered,layered_bank,0,2);
        unsigned layers=0;
        for(const auto& voice:playing) if(voice.combat && voice.handle) {
            ++layers; FAudioVoiceState queued{};api.FAudioSourceVoice_GetState(voice.handle,&queued,0);
            if(!voice.detached && queued.BuffersQueued!=2)throw std::runtime_error("Sound wave sequence was not queued in order");
        }
        if(layers!=3)throw std::runtime_error("Sound child layers did not all play");
        stop_combat();
        SoundChoice effect;effect.waves={0};play_choice(effect,layered_bank,0,2);
        SoundChoice voice=effect;voice.mode=4;play_choice(voice,layered_bank,0,2);
        const auto attached=[&] { return std::count_if(playing.begin(),playing.end(),[](const Voice& v){return v.combat && v.handle && !v.detached;}); };
        if(attached()!=2)throw std::runtime_error("Mode 4 interrupted the other sound channel");
        play_choice(effect,layered_bank,0,2);
        if(attached()!=1)throw std::runtime_error("Replacement sound did not interrupt the actor channels");
        stop_combat();
        SoundBank forwarding_bank;forwarding_bank.waves={tone};
        SoundChoice forwarding;forwarding.waves={0};forwarding.script_value=255;
        forwarding.modifiers[4]=1234;
        SoundGroup forwarding_group;forwarding_group.choices={forwarding};
        forwarding_bank.events[123]={forwarding_group};
        banks.emplace("diagnostic",std::move(forwarding_bank));
        const auto forwarded=event("diagnostic",123,context,0,2);
        if(forwarded.forwarded[2]!=1234 || forwarded.script_value!=-1)
            throw std::runtime_error("Sound forwarding or script write was lost during playback");
        stop_combat();
        std::fprintf(stderr,"Audio smoke passed: seven original sound banks, PCM/ADPCM, intro/loop queue, pause/resume/reset, layered sequences, actor channels and forwarding, %llu nonzero output samples\n",
            static_cast<unsigned long long>(nonzero_samples.load()));
    }
};
}

std::shared_ptr<AudioOutput> create_linux_audio(const std::filesystem::path& data) {
    return std::make_shared<GameAudio>(data);
}
void run_audio_smoke_test(const std::filesystem::path& data) {GameAudio audio(data,true);audio.verify();}
}
