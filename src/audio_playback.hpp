#pragma once
#include "audio_data.hpp"
#include "audio.hpp"
#include <set>
#include <cctype>
#include <random>
#include <algorithm>
#include <stdexcept>

namespace skullgirls {
// Original Linux selectors: 0x482ff0 (choices), 0x489a20 (layers),
// 0x48e94e (wave queue). Wave references are an ordered sequence.
inline std::size_t select_sound_choice(const SoundGroup& group, std::mt19937& random,
                                      std::optional<std::size_t> previous) {
    if(group.choices.empty()) throw std::runtime_error("Cannot select from an empty sound group");
    std::vector<double> weights;
    for (std::size_t i=0;i<group.choices.size();++i)
        weights.push_back(group.choices.size()>2 && previous==i ? 0 : group.choices[i].weight);
    // Malformed all-zero weights must not cause an endless retry loop.
    if (std::none_of(weights.begin(),weights.end(),[](double v){return v>0;}))
        for (std::size_t i=0;i<weights.size();++i) weights[i]=group.choices.size()>2 && previous==i ? 0 : 1;
    return std::discrete_distribution<std::size_t>(weights.begin(),weights.end())(random);
}
inline AudioEventResult sound_event_result(const SoundChoice& choice,const AudioEventNames& names) {
    AudioEventResult result;
    if(choice.script_value) result.script_value=float(static_cast<std::int8_t>(choice.script_value));
    if(choice.mode==3) return result;
    for(unsigned i=0;i<5;++i) {
        auto hash=std::uint32_t(choice.modifiers[i*2]);
        const auto source=choice.modifiers[i*2+1];
        const int index=source==1?0:source==2?2:source==3?1:source==4?3:-1;
        if(index>=0) for(unsigned char ch:names[index]) hash=hash*33u ^ unsigned(std::toupper(ch));
        result.forwarded[i]=hash;
    }
    return result;
}
// Forwarding is synchronous in the original. Bound malformed cycles and keep
// distinct helper identities separate even when their event IDs are equal.
template<class Target,class Emit,class Route>
void dispatch_sound_events(Target target,std::uint32_t id,Emit emit,Route route) {
    std::vector<std::pair<Target,std::uint32_t>> pending{{std::move(target),id}};
    std::set<std::pair<std::uint64_t,std::uint32_t>> visited;
    for(std::size_t at=0;at<pending.size() && at<128;++at) {
        auto current=std::move(pending[at]);
        if(!current.second || !visited.emplace(current.first.identity,current.second).second) continue;
        const auto result=emit(current.first,current.second);
        // Preserve original depth-first ordering: fully process an earlier
        // destination before advancing to the next forwarding slot.
        std::vector<std::pair<Target,std::uint32_t>> next;
        for(unsigned i=0;i<result.forwarded.size();++i) if(result.forwarded[i])
            for(auto& destination:route(current.first,i)) {
                if(next.size()+pending.size()>=128) break;
                next.emplace_back(std::move(destination),result.forwarded[i]);
            }
        pending.insert(pending.begin()+at+1,std::make_move_iterator(next.begin()),std::make_move_iterator(next.end()));
    }
}
inline bool sound_voice_bus(unsigned mode) { return mode==1 || mode==2 || mode==4 || mode==6; }
inline bool sound_interrupts(unsigned mode) { return mode!=3 && mode!=4 && mode!=6; }
inline std::vector<const SoundChoice*> sound_layers(const SoundChoice& selected) {
    if (selected.mode==3) return {};
    std::vector<const SoundChoice*> layers{&selected};
    for (const auto& child : selected.children) if(child.mode!=3) layers.push_back(&child);
    return layers;
}
}
