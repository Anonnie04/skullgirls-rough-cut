#pragma once
#include "combat_scene.hpp"
#include "stage_select.hpp"
#include <sstream>
#include <functional>
#include <cctype>
#include <cstdlib>

namespace skullgirls {
inline std::string story_trim(std::string s) {
    const auto begin=s.find_first_not_of(" \t\r\n");
    return begin==s.npos ? "" : s.substr(begin,s.find_last_not_of(" \t\r\n")-begin+1);
}
inline std::string story_text(const SpriteBytes& b) { return {b.begin(),b.end()}; }
struct StoryCommand { std::string type, value; };
inline std::vector<StoryCommand> read_story_commands(std::string text, bool colon=true) {
    std::vector<StoryCommand> result;
    std::istringstream stream(text);
    for(std::string line;std::getline(stream,line);) {
        line=story_trim(line.substr(0,line.find('#')));
        if(line.empty())continue;
        const auto split=colon?line.find(':'):line.find_first_of(" \t");
        result.push_back({story_trim(line.substr(0,split)),split==line.npos?"":story_trim(line.substr(split+1))});
    }
    return result;
}
struct StoryScene {
    using Node=CombatScene::Node;
    using Animation=CombatScene::Animation;
    struct Event { unsigned frame, message; };
    struct Clip { Animation animation; unsigned loop_start=~0u,loop_end=~0u; std::vector<Event> events; std::map<unsigned,std::vector<std::pair<unsigned,unsigned>>> colors; std::map<unsigned,std::vector<std::pair<unsigned,std::array<float,2>>>> pivots; };
    struct View { std::vector<Node> nodes; std::map<std::string,Clip> clips; };
    struct Symbol {unsigned page,x,y,width,height;};
    std::map<std::string,Symbol> symbols;
    std::map<std::string,View> views;
    std::map<unsigned,std::string> textures;
    std::map<std::string,std::map<unsigned,unsigned>> colors;
    std::map<std::string,std::map<unsigned,std::array<bool,2>>> flips;
};
// CutsceneCommon uses the CSGG view offset at +48 (the +44 table is sounds).
inline StoryScene read_story_scene(const SpriteBytes& data) {
    const std::string_view b(reinterpret_cast<const char*>(data.data()),data.size());
    const auto word=[&](std::size_t at) { if(at>b.size()||b.size()-at<4)throw std::runtime_error("Truncated story scene");
        unsigned v;std::memcpy(&v,b.data()+at,4);return v; };
    const auto real=[&](std::size_t at) { auto v=word(at);float f;std::memcpy(&f,&v,4);if(!std::isfinite(f))throw std::runtime_error("Invalid story transform");return f; };
    const auto name=[&](std::size_t at,std::size_t n) { if(at>b.size()||n>b.size()-at)throw std::runtime_error("Truncated story name");
        auto s=b.substr(at,n);return std::string(s.substr(0,s.find('\0'))); };
    const auto end=[&](std::size_t at,std::size_t minimum,std::size_t limit) { if(at>limit||limit-at<minimum)throw std::runtime_error("Truncated story record");
        auto n=word(at+4);if(n<minimum||n>limit-at)throw std::runtime_error("Invalid story record");return at+n; };
    if(b.size()<56||b.substr(0,4)!="CSGG")throw std::runtime_error("Invalid CutsceneCommon.gbs");
    StoryScene result;
    auto font=std::size_t(56);
    for(unsigned i=0;i<word(16);++i) {
        const auto fe=end(font,100,b.size());
        if(name(font+12,64)=="Dialogue Text") {
            if(word(font+96)>(fe-font-100)/40)throw std::runtime_error("Truncated story font");
            for(unsigned j=0;j<word(font+96);++j){const auto g=font+100+j*40;const auto code=word(g);
                if((code>>16)==0x686e){std::string token;for(int shift=24;shift>=0;shift-=8)token+=char(code>>shift);
                    result.symbols[token]={word(g+4),word(g+8),word(g+12),word(g+16),word(g+20)};}
            }
        }font=fe;
    }
    auto texture=56ull+word(40);
    if(texture>b.size()||word(20)>(b.size()-texture)/284)throw std::runtime_error("Invalid story textures");
    for(unsigned i=0;i<word(20);++i,texture+=284) {
        auto path=name(texture+4,260);
        if(path.size()>4&&path.substr(path.size()-4)==".png")result.textures[word(texture)]="ui/Win/"+path.substr(0,path.size()-4)+".dds";
    }
    const auto objects=[&](auto&& self,std::size_t at,std::size_t limit,unsigned count,unsigned depth)->std::vector<StoryScene::Node> {
        if(depth>32)throw std::runtime_error("Story nesting limit");
        std::vector<StoryScene::Node> nodes;
        for(unsigned i=0;i<count;++i) {
            auto stop=end(at,116,limit);StoryScene::Node n;
            n.id=word(at+8);n.name=name(at+12,64);n.pivot_x=real(at+84);n.pivot_y=real(at+88);
            n.transform={real(at+76),real(at+80),real(at+92),real(at+96),real(at+100),float(word(at+108)>>24)/255};
            if(b.substr(at,4)=="PRGG")n.children=self(self,at+116,stop,word(at+112),depth+1);
            else if(b.substr(at,4)=="RPSG")n.texture=word(at+112);
            nodes.push_back(std::move(n));at=stop;
        }return nodes;
    };
    auto v=56ull+word(48);
    for(unsigned i=0;i<word(28);++i) {
        auto ve=end(v,140,b.size());auto& view=result.views[name(v+12,64)];
        view.nodes=objects(objects,v+140+word(v+132),ve,word(v+120),0);
        const auto read_colors=[&](auto&& self,std::size_t at,std::size_t limit,unsigned count)->void {
            for(unsigned k=0;k<count;++k){auto stop=end(at,116,limit);result.colors[name(v+12,64)][word(at+8)]=word(at+108);
                if(b.substr(at,4)=="PRGG")self(self,at+116,stop,word(at+112));
                else if(b.substr(at,4)=="RPSG"&&stop-at>=132)result.flips[name(v+12,64)][word(at+8)]={word(at+124)!=0,word(at+128)!=0};
                at=stop;}
        };
        read_colors(read_colors,v+140+word(v+132),ve,word(v+120));
        auto table=v+140+word(v+136);auto te=end(table,12,ve);
        for(auto a=table+12;a<te;) {
            auto ae=end(a,96,te);auto& clip=view.clips[name(a+8,64)];clip.animation.frames=word(a+72);clip.loop_start=word(a+76);clip.loop_end=word(a+80);
            auto track=a+96;
            for(unsigned j=0;j<word(a+84);++j) {
                auto tr_end=end(track,16,ae);auto key=track+16;
                for(unsigned k=0;k<word(track+12);++k) {
                    auto ke=end(key,88,tr_end);auto kind=b.substr(key+52,4);
                    if(kind=="TLPS"||kind=="TLPG") {
                        if(kind=="TLPS"&&ke-key<92)throw std::runtime_error("Truncated story color");
                        clip.pivots[word(track+8)].push_back({word(key+8),{real(key+60),real(key+64)}});
                        if(kind=="TLPS")clip.colors[word(track+8)].push_back({word(key+8),word(key+88)});
                        clip.animation.tracks[word(track+8)].push_back({word(key+8),{real(key+68),real(key+72),real(key+76),real(key+80),real(key+84),kind=="TLPS"?float(word(key+88)>>24)/255:1}});
                    }key=ke;
                }track=tr_end;
            }
            auto event=a+96+word(a+92);
            for(unsigned j=0;j<word(a+88);++j) {
                auto ee=end(event,80,ae);auto action=event+80;
                for(unsigned k=0;k<word(event+76);++k) {
                    auto action_end=end(action,12,ee);
                    if(b.substr(action,4)=="AGSM")clip.events.push_back({word(event+72),word(action+8)});
                    action=action_end;
                }event=ee;
            }
            std::stable_sort(clip.events.begin(),clip.events.end(),[](auto a,auto b){return a.frame<b.frame;});a=ae;
        }v=ve;
    }return result;
}

struct StoryCharacter { const char* id; const char* route; const char* name; };
inline constexpr std::array<StoryCharacter,18> story_characters{{
    {"filia","Filia","Filia"},{"cerebella","Cerebella","Cerebella"},{"peacock","Peacock","Peacock"},
    {"parasoul","Parasoul","Parasoul"},{"msfortune","MsFortune","Ms. Fortune"},{"painwheel","Painwheel","Painwheel"},
    {"valentine","Valentine","Valentine"},{"double","Double","Double"},{"squigly","Squigly","Squigly"},
    {"bigband","BigBand","Big Band"},{"fukua","Fukua","Fukua"},{"eliza","Eliza","Eliza"},
    {"beowulf","Beowulf","Beowulf"},{"robofortune","RoboFortune","Robo-Fortune"},{"annie","Annie","Annie"},
    {"umbrella","Umbrella","Umbrella"},{"blackdahlia","BlackDahlia","Black Dahlia"},{"marie","Marie","Marie"}
}};
inline std::string story_fold(std::string s) { for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s; }
inline const StoryCharacter& story_character(const std::string& id) {
    for(const auto& c:story_characters)if(c.id==id)return c;
    throw std::runtime_error("Unknown story character: "+id);
}
inline std::string story_shot_path(const std::string& value) {
    std::istringstream row(value);std::string slot,dir,file;row>>slot>>dir>>file;
    if(dir.empty()||file.empty()||dir=="_EMPTY"||file=="_EMPTY"||dir=="blank")return "";
    return "story_art/Cinematics/"+dir+"/"+file+".dds";
}
inline std::optional<SpriteBytes> read_story_art(SpriteArchive& archive,const std::string& path) {
    if(auto data=archive.read_if_present(path))return data;
    const auto lower=story_fold(path);
    for(const auto& e:archive.entries())if(story_fold(e.name)==lower)return archive.read(e.name);
    // These references are absent from the shipped archive: Dahlia's disabled
    // Shot_02 placeholder and one Marie expression typo. Preserve a blank slot
    // instead of inventing replacement artwork. All other missing art is fatal.
    if(lower=="story_art/cinematics/blackdahlia/blackdahlia_03a.dds"||lower=="story_art/ths/shadow/peacock_neutral.dds")return {};
    throw std::runtime_error("Missing story art: "+path);
}
struct StoryRouteNode { std::string type,file; };
// Narrative preview: follows each native route, without touching original saves/unlocks.
class StoryPlayback {
public:
    enum class Phase { scene, fight, credits, complete };
    StoryScene scene;
    std::string character_id="filia", character_name="Filia";
    std::array<std::vector<std::string>,2> encounter_teams;
    std::array<std::vector<unsigned>,2> encounter_colors;
    bool encounter_modified=false;
    std::vector<StoryRouteNode> route;
    std::size_t route_index=0, command_index=0;
    Phase phase=Phase::complete;
    std::string view, text_key, text, speaker, world;
    std::vector<std::string> enemies;
    std::array<std::string,2> portraits{};
    std::array<bool,2> portrait_xl{};
    std::map<std::string,std::string> shots;
    std::vector<std::vector<std::string>> credits;
    std::vector<std::string> credit_images,credit_layouts;
    std::size_t credit_page=0;
    std::vector<StoryCommand> commands;
    std::map<unsigned,CombatScene::Transform> pose;
    std::map<unsigned,unsigned> pose_colors;
    std::map<unsigned,std::array<float,2>> pose_pivots;
    struct Layer {std::string name;float frame=0,speed=1;unsigned order=0;bool finished=false;};
    std::map<unsigned,Layer> layers;
    std::string animation, persistent;
    float next_speed=1;
    unsigned animation_order=0,next_order=0;
    float frame=0, persistent_frame=0, speed=1, dialogue_loop=-1;
    bool legacy=false, paused=false, waiting=false, dialogue=false, cinematic=false;
    unsigned revision=0;
    std::vector<std::string> music_messages;
    using Read=std::function<SpriteBytes(const std::string&)>;
    Read core_read;
    std::string localization;

    void load(Read core,Read ui, const SpriteBytes& gbs,const std::string& character="filia") {
        const auto& selected=story_character(character);character_id=selected.id;character_name=selected.name;
        core_read=std::move(core);scene=read_story_scene(gbs);
        localization=story_text(core_read("core/localization/en_loc.json"));
        route.clear();
        for(const auto& c:read_story_commands(story_text(core_read("core/arcade/story/"+std::string(selected.route)+".stm")),false)) {
            if(c.type=="ArcadeNode"&&c.value!="Complete")route.push_back({});
            else if(c.type=="NodeType")route.back().type=c.value;
            else if(c.type=="FileName")route.back().file="core/"+c.value;
        }
        // Preserve the installed credits' page boundaries and literal staff names.
        credits.clear();credit_images.clear();credit_layouts.clear();
        for(auto c:read_story_commands(story_text(ui("ui/config/Credits/credits_2021.ini")),false)) {
            if(c.type=="CreditsPage:"){credits.push_back({});credit_images.push_back("");credit_layouts.push_back(c.value);}
            else if(c.type=="Credit:"||c.type=="Header:"||c.type=="Image:"||c.type=="Anim:"||c.type=="Text:") {
                if(c.type=="Image:"&&!credits.empty()) {
                    auto path=c.value;if(path.size()>4&&path.substr(path.size()-4)==".png")path=path.substr(0,path.size()-4)+".dds";
                    credit_images.back()="ui/Win/"+path;
                }else if(c.value=="Blank"&&!credits.empty())credits.back().push_back("");
            }else if(!credits.empty())credits.back().push_back(c.type+(c.value.empty()?"":" "+c.value));
        }
        route_index=0;enter();
    }
    std::string localized(const std::string& key) const {
        if(key.rfind("_RAW_ ",0)==0)return key.substr(6);
        if(!key.empty()&&key.find_first_not_of(".?! \t")==key.npos)return key;
        if(key.rfind("{hn",0)==0)return key; // Rendered with the native dialogue symbol atlas.
        auto value=stage_localized_name(localization,key);
        if(!value){std::istringstream row(key);std::string word,normalized;while(row>>word){if(!normalized.empty())normalized+=' ';normalized+=word;}value=stage_localized_name(localization,normalized);}
        if(!value)throw std::runtime_error("Missing story text: "+key);
        return *value;
    }
    void enter() {
        ++revision;text.clear();text_key.clear();speaker.clear();music_messages.clear();
        animation.clear();persistent.clear();layers.clear();next_speed=1;animation_order=next_order=0;pose.clear();pose_pivots.clear();portraits={};portrait_xl={};shots.clear();commands.clear();
        paused=waiting=dialogue=cinematic=legacy=false;speed=1;frame=0;dialogue_loop=-1;command_index=0;
        if(route_index>=route.size()){phase=Phase::complete;return;}
        const auto& node=route[route_index];
        if(node.type=="Credits"){phase=Phase::credits;credit_page=0;return;}
        if(node.type=="Fight") {
            phase=Phase::fight;enemies.clear();world.clear();encounter_teams={};encounter_colors={};encounter_modified=false;
            unsigned side=0;
            for(const auto& c:read_story_commands(story_text(core_read(node.file)),false)) {
                if(c.type=="Player")side=c.value=="2"?1:0;
                else if(c.type=="Fighter"){encounter_teams[side].push_back(story_fold(c.value));encounter_colors[side].push_back(1);}
                else if(c.type=="Color"&&!encounter_colors[side].empty())encounter_colors[side].back()=std::stoul(c.value);
                else if(c.type=="World")world=c.value;
                else if(c.type=="StartingHealthByPercent"||c.type=="StartingSuper"||c.type=="AIDifficulty"||c.type=="MatchLength")encounter_modified=true;
            }
            if(encounter_teams[0].empty()){encounter_teams[0]={character_id};encounter_colors[0]={1};}
            enemies=encounter_teams[1];return;
        }
        phase=Phase::scene;commands=read_story_commands(story_text(core_read(node.file)));view.clear();
        for(const auto& c:commands)if(c.type=="View")view=c.value;
        if(!scene.views.count(view))throw std::runtime_error("Missing story view: "+view);
        // Establish authored default state before starting the first command.
        const auto establish=[&](auto&& self,const auto& nodes)->void {for(const auto& n:nodes){pose[n.id]=n.transform;pose_pivots[n.id]={n.pivot_x,n.pivot_y};self(self,n.children);}};
        establish(establish,scene.views.at(view).nodes);pose_colors=scene.colors.at(view);
        if(scene.views.at(view).clips.count("OnActivate"))apply(scene.views.at(view).clips.at("OnActivate"),0);
        pump();
    }
    void next() {++route_index;enter();}
    const StoryScene::Clip& clip(const std::string& name) const {
        const auto& clips=scene.views.at(view).clips;
        const auto found=clips.find(name);if(found==clips.end())throw std::runtime_error("Missing story animation: "+view+"/"+name);return found->second;
    }
    void apply(const StoryScene::Clip& source,float tick) {
        const auto& anim=source.animation;
        const auto visit=[&](auto&& self,const auto& nodes)->void {for(const auto& n:nodes){if(anim.tracks.count(n.id))pose[n.id]=anim.sample(n,tick);self(self,n.children);}};
        visit(visit,scene.views.at(view).nodes);
        for(const auto& [id,keys]:source.pivots) {
            auto next=std::upper_bound(keys.begin(),keys.end(),tick,[](float t,const auto& k){return t<k.first;});
            if(next==keys.begin()){pose_pivots[id]=next->second;continue;}
            auto prev=next-1;if(next==keys.end()){pose_pivots[id]=prev->second;continue;}
            const float t=(tick-prev->first)/float(next->first-prev->first);
            pose_pivots[id]={prev->second[0]+(next->second[0]-prev->second[0])*t,prev->second[1]+(next->second[1]-prev->second[1])*t};
        }
        for(const auto& [id,keys]:source.colors) {
            auto next=std::upper_bound(keys.begin(),keys.end(),tick,[](float t,const auto& k){return t<k.first;});
            if(next==keys.begin()){pose_colors[id]=next->second;continue;}
            auto prev=next-1;if(next==keys.end()){pose_colors[id]=prev->second;continue;}
            const float t=(tick-prev->first)/float(next->first-prev->first);unsigned color=0;
            for(unsigned shift=0;shift<32;shift+=8)color|=unsigned(std::lround(((prev->second>>shift)&255)*(1-t)+((next->second>>shift)&255)*t))<<shift;
            pose_colors[id]=color;
        }
    }
    void start(const std::string& name,bool old) {
        speed=next_speed;next_speed=1;animation_order=++next_order;animation=name;legacy=old;frame=0;dialogue_loop=-1;paused=false;waiting=true;
        apply(clip(name),0);events(-1,0);
    }
    void events(float previous,float now) {
        for(const auto& e:clip(animation).events)if(e.frame>previous&&e.frame<=now) {
            if(e.message>=0xeb030002&&e.message<=0xeb030014)music_messages.push_back(std::to_string(e.message));
            if(!legacy)continue;
            // DIALOGUE_ON pauses; CONT_ANIM plays to STOP_ANIM; LOOP_ANIM
            // repeats that interval while the reader is on the dialogue.
            // A queued LEGACY_PLAY consumes the stop and continues to the
            // next dialogue cue (or the end), rather than treating STOP as
            // a new dialogue page and truncating the remaining footage.
            if(e.message==0xeb030000){waiting=false;paused=true;dialogue_loop=-1;}
            else if(e.message==0xeb030015){waiting=false;paused=false;dialogue_loop=-1;}
            else if(e.message==0xeb030017){waiting=false;paused=false;dialogue_loop=float(e.frame);}
            else if(e.message==0xeb030016&&!waiting) {
                if(dialogue_loop>=0){frame=dialogue_loop;break;}
                paused=true;
            }
        }
    }
    void pump() {
        for(unsigned guard=0;guard<10000&&!waiting&&text.empty()&&phase==Phase::scene;++guard) {
            if(command_index>=commands.size()){next();return;}
            auto c=commands[command_index++];
            if(c.type=="ShotTex") {
                std::istringstream row(c.value);std::string slot,dir,file;row>>slot>>dir>>file;
                shots[slot]=story_shot_path(c.value);
                if(slot=="Character_Right")portraits[1].clear();
                else if(slot=="Character_Left")portraits[0].clear();
                ++revision;
            }else if(c.type=="Portrait") {
                std::istringstream row(c.value);std::string side,dir,file;row>>side>>dir;
                const auto id=side=="Right"?1:0;portrait_xl[id]=dir=="_XL_";
                if(portrait_xl[id])row>>dir;
                if(dir=="_EMPTY")portraits[id].clear();
                else {row>>file;portraits[id]="story_art/THs/"+dir+"/"+file+".dds";}
                ++revision;
            }else if(c.type=="Text") {
                text_key=c.value;text=localized(text_key);speaker.clear();
                if(command_index<commands.size()&&commands[command_index].type=="Header") {
                    auto header=commands[command_index++].value;auto split=header.find(' ');speaker=header.substr(split+1);
                    if(auto label=stage_localized_name(localization,speaker))speaker=*label;
                }
            }else if(c.type=="AnimSpeed") {
                // Native scripts also contain suffixes such as .25f. One shipped
                // Umbrella command has an animation name here; ignore that typo.
                char* end=nullptr;const auto v=std::strtof(c.value.c_str(),&end);
                if(end!=c.value.c_str()&&std::isfinite(v)&&v>0)next_speed=v;
            }
            else if(c.type=="Anim") {
                if(c.value.substr(0,9)=="_PERSIST ") {
                    std::istringstream row(c.value.substr(9));std::string token,name;unsigned slot=0;row>>token;
                    const bool finish=token=="_END";
                    if(finish)row>>slot;else slot=std::stoul(token);
                    row>>name;clip(name);layers.erase(slot);
                    if(finish)start(name,false);
                    else {layers[slot]={name,0,next_speed,++next_order,false};next_speed=1;apply(clip(name),0);}
                    persistent=layers.empty()?"":layers.begin()->second.name;
                }
                else if(c.value.substr(0,8)=="_LEGACY ")start(c.value.substr(8),true);
                else start(c.value,false);
            }else if(c.type=="Msg") {
                if(c.value=="DIALOGUE_LEGACY_PLAY") {
                    dialogue_loop=-1;
                    waiting=!animation.empty()&&frame<clip(animation).animation.frames;
                    paused=!waiting;
                }
                else if(c.value=="DIALOGUE_ON"||c.value=="DIALOGUE_CINE_ON"){dialogue=true;cinematic=c.value=="DIALOGUE_CINE_ON";}
                else if(c.value=="DIALOGUE_CLEAR"||c.value=="DIALOGUE_FADE_ALL"||c.value=="DIALOGUE_CINE_OFF"||c.value=="DIALOGUE_OFF")dialogue=false;
                else if(c.value.substr(0,5)=="P_END") {
                    const auto slot=std::stoul(c.value.substr(5));layers.erase(slot);
                    persistent=layers.empty()?"":layers.begin()->second.name;
                }
                else if(c.value.substr(0,6)=="MUSIC_")music_messages.push_back(c.value);
            }
        }
    }
    void tick() {
        if(phase!=Phase::scene)return;
        struct Update {unsigned order;std::string clip;float frame;};
        std::vector<Update> updates;
        if(!animation.empty()&&!paused) {
            const auto old=frame;frame=std::min(frame+speed,float(clip(animation).animation.frames));
            events(old,frame);updates.push_back({animation_order,animation,frame});
            if(frame>=clip(animation).animation.frames){paused=true;waiting=false;}
        }
        for(auto& [slot,layer]:layers) {
            if(layer.finished)continue;
            const auto& c=clip(layer.name);const auto& a=c.animation;layer.frame+=layer.speed;
            if(c.loop_start<c.loop_end&&c.loop_end<a.frames) {
                if(layer.frame>c.loop_end)layer.frame=c.loop_start+std::fmod(layer.frame-c.loop_start,float(c.loop_end-c.loop_start+1));
            }else if(layer.frame>=a.frames){layer.frame=float(a.frames);layer.finished=true;}
            updates.push_back({layer.order,layer.name,layer.frame});
        }
        // Native animation instances write in creation order. Finished one-shot
        // layers retain their final pose without repeatedly overwriting new fades.
        std::sort(updates.begin(),updates.end(),[](const auto& a,const auto& b){return a.order<b.order;});
        for(const auto& u:updates)apply(clip(u.clip),u.frame);
        pump();
    }
    // Finish only the current scene, using the same native route handoff as
    // normal cutscene completion. The following fight/scene is not skipped.
    void skip_scene() { if(phase==Phase::scene)next(); }
    void advance() {
        if(phase==Phase::fight){next();return;}
        if(phase==Phase::credits){++revision;if(++credit_page>=credits.size())next();return;}
        if(phase==Phase::scene&&!text.empty()){text.clear();text_key.clear();pump();}
        // Animation waits intentionally run at their authored 60 Hz rate.
    }
};
using FiliaStory=StoryPlayback; // Existing Filia regression fixtures.
} // namespace skullgirls
