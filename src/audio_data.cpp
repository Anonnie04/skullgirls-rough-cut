#include "audio_data.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace skullgirls {
namespace {
struct Cursor {
    const SpriteBytes& bytes;
    std::size_t position = 0;
    std::uint32_t word(unsigned count) {
        if (position > bytes.size() || count > bytes.size() - position) throw std::runtime_error("Truncated sound data");
        std::uint32_t value = 0;
        for (unsigned i=0;i<count;++i) value |= std::uint32_t(bytes[position++]) << (8*i);
        return value;
    }
    SpriteBytes take(std::size_t count) {
        if (position > bytes.size() || count > bytes.size()-position) throw std::runtime_error("Truncated sound payload");
        SpriteBytes result(bytes.begin()+position,bytes.begin()+position+count);
        position += count;
        return result;
    }
    float real() { const auto bits=word(4); float value; std::memcpy(&value,&bits,4); if(!std::isfinite(value))throw std::runtime_error("Invalid sound parameter"); return value; }
    unsigned count(unsigned width,unsigned minimum) {
        const auto result=word(width);
        if(result>65536 || result>(bytes.size()-position)/minimum)throw std::runtime_error("Invalid sound table count");
        return result;
    }
};
void expression(Cursor& reader,std::vector<FoitExpression>& output) {
    do {
        const auto a=reader.word(1),b=reader.word(1),c=reader.word(1),flags=reader.word(1);
        const auto opcode=reader.word(1);
        const auto operand=[&](unsigned type){return FoitOperand{std::uint8_t(type),reader.word(type&1?1:4)};};
        const auto left=operand(a),right=operand(b),destination=operand(c);
        output.push_back({std::uint8_t(opcode),bool(flags&1),left,right,destination});
        if(!(flags&2))break;
        if(output.size()>128)throw std::runtime_error("Sound condition is too long");
    } while(true);
}
SoundChoice choice(Cursor& reader,unsigned depth) {
    if(depth>16)throw std::runtime_error("Sound layers are nested too deeply");
    SoundChoice result;
    for(auto& value:result.modifiers){const auto bits=reader.word(4);std::memcpy(&value,&bits,4);}
    result.weight=reader.real();
    if(result.weight<0)throw std::runtime_error("Invalid sound selection weight");
    const auto flags=reader.word(1);
    if(flags&~7u)throw std::runtime_error("Unsupported sound choice flags");
    result.detached=flags&1;
    if(flags&2)result.script_value=reader.word(1);
    if(flags&4)result.gain_percent=reader.word(1);
    result.mode=reader.word(1);
    for(unsigned i=0,n=reader.count(4,2);i<n;++i) result.waves.push_back(reader.word(2));
    for(unsigned i=0,n=reader.count(4,50);i<n;++i) result.children.push_back(choice(reader,depth+1));
    return result;
}
std::string trim(std::string value) {
    const auto first=value.find_first_not_of(" \t\r\n");
    if(first==value.npos)return {};
    return value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
}
}

AudioWave read_audio_wave(const SpriteBytes& bytes) {
    if(bytes.size()<12 || std::memcmp(bytes.data(),"RIFF",4) || std::memcmp(bytes.data()+8,"WAVE",4))
        throw std::runtime_error("Expected RIFF/WAVE audio");
    Cursor reader{bytes,4};
    const std::uint64_t end=std::uint64_t(reader.word(4))+8;
    if(end>bytes.size() || end<12)throw std::runtime_error("Truncated RIFF audio");
    AudioWave result;
    unsigned fact=0;
    for(std::size_t at=12;at<end;) {
        if(end-at<8)throw std::runtime_error("Truncated RIFF chunk");
        reader.position=at+4;const auto length=reader.word(4);
        if(length>end-at-8)throw std::runtime_error("Truncated RIFF chunk payload");
        if(!std::memcmp(bytes.data()+at,"fmt ",4))result.format=reader.take(length);
        else if(!std::memcmp(bytes.data()+at,"data",4))result.data=reader.take(length);
        else if(!std::memcmp(bytes.data()+at,"fact",4) && length>=4)fact=reader.word(4);
        at+=8+std::size_t(length)+(length&1);
    }
    if(result.format.size()<16 || result.format.size()>256 || result.data.empty())throw std::runtime_error("Missing audio format or samples");
    if(result.format.size()==16)result.format.resize(18);
    Cursor format{result.format};
    const auto tag=format.word(2);result.channels=format.word(2);result.sample_rate=format.word(4);
    format.word(4);const auto block=format.word(2),bits=format.word(2),extra=format.word(2);
    if(!result.channels || result.channels>2 || result.sample_rate<1000 || result.sample_rate>200000 ||
       !block || extra>result.format.size()-18 || result.data.size()%block)
        throw std::runtime_error("Invalid audio format or block alignment");
    std::uint64_t frames=0;
    if(tag==2) {
        if(bits!=4 || extra<32 || result.format.size()<50)throw std::runtime_error("Invalid MS ADPCM format");
        const auto samples=format.word(2),coefficients=format.word(2);
        if(coefficients!=7 || block<7*result.channels || samples!=2+(block-7*result.channels)*2/result.channels)
            throw std::runtime_error("Invalid MS ADPCM block format");
        frames=std::uint64_t(result.data.size()/block)*samples;
    } else if((tag==1 && (bits==8 || bits==16 || bits==24 || bits==32)) || (tag==3 && bits==32)) {
        if(block!=result.channels*bits/8)throw std::runtime_error("Invalid PCM block format");
        frames=result.data.size()/block;
    } else throw std::runtime_error("Unsupported game audio encoding");
    if(frames>std::numeric_limits<unsigned>::max())throw std::runtime_error("Audio clip is too long");
    result.frames=unsigned(frames);
    if(fact && fact<=result.frames && tag!=2)result.frames=fact;
    return result;
}

SoundBank read_sound_bank(const SpriteBytes& bytes) {
    Cursor reader{bytes};SoundBank result;
    for(unsigned i=0,n=reader.count(4,14);i<n;++i) {
        const auto id=reader.word(4);reader.word(4); // Stored event name/hash, also used as the map key.
        auto& groups=result.events[id];
        for(unsigned j=0,count=reader.count(4,2);j<count;++j) {
            SoundGroup group;
            const auto flags=reader.word(1);
            if(flags&~3u)throw std::runtime_error("Unsupported sound group flags");
            group.completion_flag=flags&2;
            if(flags&1)expression(reader,group.condition);
            for(unsigned k=0,choices=reader.count(1,50);k<choices;++k)group.choices.push_back(choice(reader,0));
            groups.push_back(std::move(group));
        }
    }
    for(unsigned i=0,n=reader.count(4,24);i<n;++i) {
        const auto size=reader.word(4),loop_begin=reader.word(4),loop_length=reader.word(4);
        auto wave=std::make_shared<AudioWave>(read_audio_wave(reader.take(size)));
        if(loop_begin>wave->frames || loop_length>wave->frames-loop_begin)throw std::runtime_error("Invalid sound loop region");
        wave->loop_begin=loop_begin;wave->loop_length=loop_length;
        result.waves.push_back(std::move(wave));
    }
    const auto validate=[&](auto&& self,const SoundChoice& entry)->void {
        for(auto index:entry.waves)if(index>=result.waves.size())throw std::runtime_error("Sound references a missing wave");
        for(const auto& child:entry.children)self(self,child);
    };
    for(const auto& event:result.events)for(const auto& group:event.second)for(const auto& entry:group.choices)validate(validate,entry);
    if(reader.position!=bytes.size())throw std::runtime_error("Unparsed character sound data");
    return result;
}

std::vector<MusicTrack> read_music_tracks(const SpriteBytes& config) {
    std::istringstream input(std::string(config.begin(),config.end()));
    std::vector<MusicTrack> tracks;
    for(std::string line;std::getline(input,line);) {
        line=trim(line.substr(0,line.find('#')));
        if(line.rfind("Track:",0)==0)tracks.push_back({trim(line.substr(6)),{},{},{}});
        else if(!tracks.empty() && line.rfind("Stage:",0)==0) {
            std::istringstream stages(line.substr(6));
            for(std::string stage;std::getline(stages,stage,',');)tracks.back().stages.push_back(trim(stage));
        } else if(!tracks.empty() && line.rfind("Files:",0)==0) {
            const auto comma=line.find(',');
            if(comma==line.npos)throw std::runtime_error("Music track needs intro and loop files");
            tracks.back().intro=trim(line.substr(6,comma-6));tracks.back().loop=trim(line.substr(comma+1));
        }
    }
    for(const auto& track:tracks)if(track.intro.empty() || track.loop.empty())throw std::runtime_error("Incomplete music track");
    return tracks;
}
}
