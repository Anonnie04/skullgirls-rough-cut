#pragma once

#include "sprite_asset.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <string>
#include <string_view>

namespace skullgirls {

// The original HUD's filmstrip, vignette and combo panel retain their authored
// transforms. Only the character portrait is substituted at draw time.
struct CombatScene {
    struct Transform {
        float x = 0, y = 0, width = 0, height = 0, rotation = 0, opacity = 1;
    };
    struct Node {
        unsigned id = 0, texture = 0;
        std::string name;
        float pivot_x = 0, pivot_y = 0;
        Transform transform;
        std::vector<Node> children;
    };
    struct Tile {
        unsigned page = 0;
        std::array<float, 4> uv{};
    };
    struct Key { unsigned frame; Transform transform; };
    struct Animation {
        unsigned frames = 0;
        std::map<unsigned, std::vector<Key>> tracks;
        Transform sample(const Node& node, float frame) const {
            const auto found = tracks.find(node.id);
            if (found == tracks.end() || found->second.empty()) return node.transform;
            const auto& keys = found->second;
            const auto next = std::upper_bound(keys.begin(), keys.end(), frame,
                [](float tick, const Key& key) { return tick < key.frame; });
            if (next == keys.begin()) return next->transform;
            const auto& previous = *(next - 1);
            if (next == keys.end()) return previous.transform;
            const float t = (frame - previous.frame) / float(next->frame - previous.frame);
            const auto mix = [t](float a, float b) { return a + (b - a) * t; };
            const auto& a = previous.transform;
            const auto& b = next->transform;
            return {mix(a.x,b.x), mix(a.y,b.y), mix(a.width,b.width), mix(a.height,b.height),
                mix(a.rotation,b.rotation), mix(a.opacity,b.opacity)};
        }
    };
    std::vector<Node> nodes;
    std::map<std::string, unsigned> texture_names;
    std::map<unsigned, Tile> tiles;
    std::map<std::string, Animation> animations;

    const Node* find(std::string_view name) const {
        const auto search = [&](auto&& self, const auto& list) -> const Node* {
            for (const auto& node : list) {
                if (node.name == name) return &node;
                if (const auto* child = self(self, node.children)) return child;
            }
            return nullptr;
        };
        return search(search, nodes);
    }
};

inline CombatScene read_combat_scene(const SpriteBytes& data) {
    const std::string_view bytes(reinterpret_cast<const char*>(data.data()), data.size());
    const auto word = [&](std::size_t at) {
        if (at > bytes.size() || bytes.size() - at < 4) throw std::runtime_error("Truncated combat scene");
        std::uint32_t value; std::memcpy(&value, bytes.data() + at, 4); return value;
    };
    const auto real = [&](std::size_t at) {
        const auto bits = word(at); float value; std::memcpy(&value, &bits, 4); return value;
    };
    const auto name = [&](std::size_t at, std::size_t length) {
        if (at > bytes.size() || length > bytes.size() - at) throw std::runtime_error("Truncated combat scene name");
        const auto value = bytes.substr(at, length); return std::string(value.substr(0, value.find('\0')));
    };
    const auto end_record = [&](std::size_t at, std::size_t minimum, std::size_t limit) {
        if (at > limit || limit - at < minimum) throw std::runtime_error("Truncated combat scene record");
        const auto size = word(at + 4);
        if (size < minimum || size > limit - at) throw std::runtime_error("Invalid combat scene record");
        return at + size;
    };
    if (bytes.size() < 56 || bytes.substr(0,4) != "CSGG") throw std::runtime_error("Invalid InGame.gbs");
    CombatScene scene;
    const auto textures = 56ull + word(40);
    if (textures > bytes.size() || word(20) > (bytes.size() - textures) / 284) throw std::runtime_error("Invalid HUD texture table");
    for (unsigned i = 0; i < word(20); ++i) {
        const auto at = textures + i * 284;
        const auto page = word(at + 264);
        if (page != 0xe90203e8u && page != 0xe90203e9u) continue;
        scene.texture_names.emplace(name(at + 4,260), word(at));
        scene.tiles.emplace(word(at), CombatScene::Tile{page - 0xe90203e8u,
            {real(at+268),real(at+272),real(at+276),real(at+280)}});
    }
    const auto objects = [&](auto&& self, std::size_t at, std::size_t limit, unsigned count,
                             unsigned depth) -> std::vector<CombatScene::Node> {
        if (depth > 32) throw std::runtime_error("Invalid combat scene nesting");
        std::vector<CombatScene::Node> result;
        for (unsigned i=0;i<count;++i) {
            const auto end = end_record(at,116,limit);
            const auto tag = bytes.substr(at,4);
            CombatScene::Node node;
            node.id = word(at+8); node.name = name(at+12,64);
            node.pivot_x = real(at+84); node.pivot_y = real(at+88);
            node.transform = {real(at+76),real(at+80),real(at+92),real(at+96),real(at+100),float(word(at+108)>>24)/255};
            if (tag == "PRGG") node.children = self(self,at+116,end,word(at+112),depth+1);
            else if (tag == "RPSG") node.texture = word(at+112);
            result.push_back(std::move(node));
            at = end;
        }
        return result;
    };
    auto view = 56ull + word(44);
    for (unsigned i=0;i<word(28);++i) {
        const auto end = end_record(view,140,bytes.size());
        if (name(view+12,64) == "HUD") {
            const auto object_table = view + 140 + word(view+132);
            scene.nodes = objects(objects,object_table,end,word(view+120),0);
            const auto table = view + 140 + word(view+136);
            const auto table_end = end_record(table,12,end);
            for (auto at=table+12;at<table_end;) {
                const auto animation_end = end_record(at,96,table_end);
                const auto title = name(at+8,64);
                if (title.find("Super_Anim_") != std::string::npos || title.find("Combo_") != std::string::npos) {
                    CombatScene::Animation animation;
                    animation.frames = word(at+72);
                    auto track = at+96;
                    for (unsigned j=0;j<word(at+84);++j) {
                        const auto track_end = end_record(track,16,animation_end);
                        auto key = track+16;
                        for (unsigned k=0;k<word(track+12);++k) {
                            const auto key_end = end_record(key,88,track_end);
                            const auto kind = bytes.substr(key+52,4);
                            if (kind=="TLPG" || kind=="TLPS") {
                                if (kind=="TLPS" && key_end-key<92) throw std::runtime_error("Truncated HUD color key");
                                const float opacity = kind=="TLPS" ? float(word(key+88)>>24)/255 : 1;
                                animation.tracks[word(track+8)].push_back({word(key+8),
                                    {real(key+68),real(key+72),real(key+76),real(key+80),real(key+84),opacity}});
                            }
                            key=key_end;
                        }
                        track=track_end;
                    }
                    scene.animations.emplace(title,std::move(animation));
                }
                at=animation_end;
            }
            break;
        }
        view=end;
    }
    if (!scene.find("P1_Filmstrip") || !scene.animations.count("P1_Super_Anim_In")) throw std::runtime_error("Missing original super presentation");
    return scene;
}

struct CombatPresentation {
    struct Cutin {
        bool frozen = false;
        unsigned frame = 0, exit_frame = 11;
        void advance(bool freeze) {
            if (freeze) {
                frame = frozen ? std::min(frame+1,79u) : 0;
                exit_frame = 0;
            } else if (frozen) exit_frame=0;
            else exit_frame=std::min(exit_frame+1,11u);
            frozen=freeze;
        }
        bool visible() const { return frozen || exit_frame<11; }
    };
    struct Ghost {
        unsigned sprite, age;
        std::size_t slot;
        float x,y;
        bool mirror;
    };
    struct Combo {
        unsigned hits = 0, age = 65, pop = 0;
        bool running = false;
        void advance(unsigned count, bool active, bool frozen) {
            if (active && count!=hits) { hits=count; age=0; pop=6; }
            if (running && !active) age=0;
            running=active;
            if (!frozen) { if (!running) age=std::min(age+1,65u); if (pop) --pop; }
        }
        bool visible() const { return hits>1 && (running || age<65); }
    };
    std::array<Cutin,2> cutins{};
    std::array<Combo,2> combos{};
    std::array<std::vector<Ghost>,2> ghosts;
    unsigned tick = 0;
};

}
