#include "audio_data.hpp"
#include "audio_playback.hpp"
#include "foit_effects.hpp"
#include "sprite_asset.hpp"
#include <iostream>
#include <stdexcept>
using namespace skullgirls;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static void word(SpriteBytes& b,unsigned n,unsigned size=4){for(unsigned i=0;i<size;++i)b.push_back(n>>(8*i));}
int main(int argc,char** argv){try{
    SpriteBytes wav{'R','I','F','F'};word(wav,40);wav.insert(wav.end(),{'W','A','V','E','f','m','t',' '});word(wav,16);
    word(wav,1,2);word(wav,1,2);word(wav,48000);word(wav,96000);word(wav,2,2);word(wav,16,2);
    wav.insert(wav.end(),{'d','a','t','a'});word(wav,4);word(wav,0);
    const auto decoded=read_audio_wave(wav);
    check(decoded.frames==2 && decoded.format.size()==18,"PCM sample count and padded WAVEFORMATEX");
    for(unsigned n=0;n<wav.size();++n){auto truncated=wav;truncated.resize(n);bool rejected=false;
        try{read_audio_wave(truncated);}catch(const std::runtime_error&){rejected=true;}
        check(rejected,"Truncated WAV accepted");}
    SpriteBytes bank;word(bank,1);word(bank,42);word(bank,42);word(bank,1);word(bank,0,1);word(bank,1,1);
    for(unsigned i=0;i<10;++i)word(bank,0);
    word(bank,0x3f800000);word(bank,7,1);word(bank,255,1);word(bank,30,1);word(bank,0,1);word(bank,1);word(bank,0,2);word(bank,0);
    word(bank,1);word(bank,wav.size());word(bank,0);word(bank,0);bank.insert(bank.end(),wav.begin(),wav.end());
    auto sounds=read_sound_bank(bank);check(sounds.events.at(42)[0].choices[0].waves[0]==0 && sounds.waves[0]->frames==2,"Bank wave reference");
    const auto& parsed_choice=sounds.events.at(42)[0].choices[0];
    check(parsed_choice.detached && parsed_choice.script_value==255 && parsed_choice.gain_percent==30,
        "Optional sound flag payloads retain their original byte order");
    for(unsigned n=0;n<bank.size();++n){auto truncated=bank;truncated.resize(n);bool rejected=false;
        try{read_sound_bank(truncated);}catch(const std::runtime_error&){rejected=true;}check(rejected,"Truncated bank accepted");}
    FoitMoveTable table;table.animations.resize(1);table.animations[0].resize(2);
    table.animations[0][0].sounds={20};table.animations[0][1].sounds={30};
    FoitVisualState visual;FoitScriptState state;FoitInputHistory inputs;
    visual.enter_records(table,0,0,state,inputs,true);visual.enter_records(table,0,0,state,inputs,true);
    check(visual.pending_sounds.size()==1,"Frame cues fire once on entry");
    visual.enter_records(table,0,1,state,inputs,true);check(visual.pending_sounds.size()==2 && visual.pending_sounds.back().first==30,"Next record cue");
    visual.enter_records(table,0,0,state,inputs,true);check(visual.pending_sounds.size()==3,"Restart replays entry cues");
    SoundGroup group;group.choices.resize(3);
    std::mt19937 random(7);std::optional<std::size_t> previous;
    for(unsigned i=0;i<1000;++i){auto selected=select_sound_choice(group,random,previous);
        check(!previous || selected!=*previous,"Three or more variations must not repeat immediately");previous=selected;}
    group.choices.resize(2);group.choices[0].weight=0;group.choices[1].weight=1;
    check(select_sound_choice(group,random,1)==1,"Two-choice banks allow repeats and honor weights");
    SoundChoice layered;layered.children.resize(2);layered.children[0].weight=0;
    check(sound_layers(layered).size()==3,"All child layers play, including zero selection weights");
    layered.mode=3;check(sound_layers(layered).empty(),"Mode 3 suppresses the whole selected sound");
    check(sound_voice_bus(1) && sound_voice_bus(2) && sound_voice_bus(4) && sound_voice_bus(6) && !sound_voice_bus(0),"Original sound bus routing");
    check(!sound_interrupts(4) && !sound_interrupts(6) && sound_interrupts(0),"Modes 4 and 6 preserve the other actor channel");
    SoundChoice forwarding;
    forwarding.modifiers={42,0,0x1505,1,0x1505,3,0x1505,2,0x1505,4};
    forwarding.script_value=255;
    AudioEventNames names{"filia","cerebella","peacock","msfortune"};
    const auto forwarded=sound_event_result(forwarding,names);
    const auto hash=[](const std::string& name){unsigned value=0x1505;for(unsigned char c:name)value=value*33u^unsigned(std::toupper(c));return value;};
    check(forwarded.forwarded==std::array<std::uint32_t,5>{42,hash("filia"),hash("cerebella"),hash("peacock"),hash("msfortune")},"Sound source operands hash the selected character name");
    check(forwarded.script_value==-1,"Sound script writes use signed bytes");
    names[3].clear();check(sound_event_result(forwarding,names).forwarded[4]==0x1505,"Missing target leaves the literal hash");
    forwarding.mode=3;const auto muted=sound_event_result(forwarding,names);
    check(muted.forwarded==std::array<std::uint32_t,5>{} && muted.script_value==-1,"Mode 3 writes the script value without forwarding");
    struct Target { std::uint64_t identity; };
    std::vector<std::pair<unsigned,unsigned>> delivered;
    dispatch_sound_events(Target{1},10,[&](Target& target,unsigned id){
        delivered.emplace_back(target.identity,id);AudioEventResult result;
        if(id==10){result.forwarded[2]=20;result.forwarded[4]=30;}
        if(id==20)result.forwarded[0]=10;
        return result;
    },[](const Target&,unsigned destination){
        return destination==4 ? std::vector<Target>{{3},{4}} : std::vector<Target>{{1}};
    });
    check(delivered==std::vector<std::pair<unsigned,unsigned>>{{1,10},{1,20},{3,30},{4,30}},"Forwarding preserves order, distinct helpers and cycle protection");
    if(argc>1){
        SpriteArchive archive(std::filesystem::path(argv[1])/"characters-win.gfs");
        auto shared=read_sound_bank(archive.read("characters/hitspark.snd-wav"));
        for(auto name:{"filia","cerebella","peacock","parasoul","msfortune","painwheel","hitspark"}){
            auto original=read_sound_bank(archive.read(std::string("characters/")+name+".snd-wav"));
            check(!original.events.empty() && !original.waves.empty(),"Original sound bank is empty");
            const auto validate_sequence=[&](auto&& self,const SoundChoice& choice)->void {
                if(!choice.waves.empty()) {
                    const auto& format=original.waves.at(choice.waves.back())->format;
                    for(auto index:choice.waves) {
                        const auto& other=original.waves.at(index)->format;
                        check(other.size()==format.size() && std::equal(other.begin(),other.begin()+4,format.begin()) &&
                            std::equal(other.begin()+12,other.end(),format.begin()+12),"Original wave sequence formats must match");
                    }
                }
                for(const auto& child:choice.children)self(self,child);
            };
            for(const auto& event:original.events)for(const auto& g:event.second)for(const auto& c:g.choices)validate_sequence(validate_sequence,c);
            auto animation=CharacterAssets(argv[1],name).read_animations(true);
            unsigned matched=0,total=0;
            const auto visit=[&](auto&& self,const FoitMoveTable& entity)->void{
                const auto cue=[&](auto id){++total;if(original.events.count(id) || shared.events.count(id))++matched;};

                for(const auto& clip:entity.animations)for(const auto& frame:clip){for(auto id:frame.sounds)cue(id);for(const auto& v:frame.variants)for(auto id:v.sounds)cue(id);}
                for(const auto& child:entity.children)self(self,child);
            };visit(visit,animation);
            check(total && matched*4>=total*3,"Most authored cues must resolve in the character/shared banks");
            std::cout<<name<<": "<<matched<<"/"<<total<<" authored sound references resolved\n";
        }
    }
    std::cout<<"Audio parser bounds and authored event timing passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
