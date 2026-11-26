#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <deque>
#include <iostream>
#include <sstream>

namespace {
void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
void prepare(Menu& m,const std::filesystem::path& data){
    m.game_data=data;m.load_movement_frames();m.load_robofortune();m.load_practice_effects();
    const auto id=[](std::string_view n){return int(std::find(roster.begin(),roster.end(),n)-roster.begin());};
    m.fighters[0]={id("filia")};m.fighters[1]={id("robofortune")};
    m.locked_colors[0]=m.locked_colors[1]={0};
    m.practice_width=1700;m.practice_height=2000;m.practice_floor=64;m.practice_starts={400,1250};
    m.begin_netplay_match(1234);
    m.practice_presentation={}; // Begin at the same post-intro simulation boundary.
}
// Test observation of logical values, never padding, heap addresses or resource pointers.
// This is not a production cross-process desync checksum.
std::string observe(const Menu::NetplayState& state){
    const auto& s=state.simulation;std::ostringstream o;o<<std::hexfloat;
    const auto add=[&](auto v){o<<v<<',';};
    add(s.stage_tick);add(s.camera_center);add(s.camera_bottom);add(s.camera_zoom);
    add(s.training_rng);o<<s.cpu_rng<<',';add(state.next_entity);add(state.paused);
    add(int(s.presentation.phase));add(s.presentation.tick);add(s.combat.tick);add(s.flash_amount);
    for(unsigned side=0;side<2;++side){
        const auto& f=s.fighters[side];const auto& h=s.stats[side];
        add(f.practice_x);add(f.jump_height);add(f.jump_velocity);add(f.active_normal);add(f.movement_sprite);
        add(f.tension);add(f.hurt_group);add(f.fenrir_captured);add(f.visual.freeze);add(f.visual.next_record);
        add(f.script_move_age);add(f.next_script_record);add(h.life);add(h.hits);add(h.damage);add(h.combo);
        add(state.held[side]);add(state.previous[side]);add(state.edges[side]);
        for(auto v:s.inputs[side].entries)add(v);
        for(auto v:f.script_state.variables){add(v.has_value());if(v)add(*v);}
        if(f.blockbuster_playback){add(f.blockbuster_playback->record);add(f.blockbuster_playback->ticks);}
        if(const auto* scripted=f.scripted()){add(scripted->move);add(scripted->record);add(scripted->ticks);}
        add(s.teams[side].active_slot);for(auto& life:s.teams[side].health)add(life.life);
    }
    add(s.effects.size());for(const auto& e:s.effects){add(e.entity_id);add(e.record);add(e.ticks);add(e.x);add(e.y);}
    return o.str();
}
std::uint32_t checksum(const Menu::NetplayState& s){std::uint32_t h=2166136261u;for(unsigned char b:observe(s))h=(h^b)*16777619u;return h;}
// Fixture-only encoding. The original record fields remain opaque until traced;
// this test mapping must not become the shipping game's wire input layout.
skullgirls::NetInput fixture(unsigned mask){skullgirls::NetInput n;n.auxiliary=std::uint16_t(mask);return n;}
std::array<Menu::PracticeInput,2> decode(const std::array<skullgirls::NetInput,2>& in){return {{{in[0].auxiliary,0},{in[1].auxiliary,0}}};}
struct Pipe:skullgirls::NetplayTransport {
    struct Packet{unsigned due;skullgirls::NetFrame frame;};
    unsigned clock=0;Pipe* other=nullptr;std::deque<Packet> pending;std::deque<skullgirls::NetFrame> arrived;
    void send_input(const skullgirls::NetFrame& f)override{other->pending.push_back({clock+unsigned(f.frame%7+2),f});}
    bool receive_input(skullgirls::NetFrame& out)override{
        for(auto i=pending.begin();i!=pending.end();)if(i->due<=clock){arrived.push_back(i->frame);i=pending.erase(i);}else ++i;
        if(arrived.empty())return false;out=arrived.front();arrived.pop_front();return true;
    }
};
void run(const std::filesystem::path& data){
    auto baseline=std::make_unique<Menu>();prepare(*baseline,data);
    std::vector<std::array<unsigned,2>> inputs;
    const auto tick=[&](unsigned p1,unsigned p2=0){inputs.push_back({p1,p2});baseline->advance_netplay_frame(decode({fixture(p1),fixture(p2)}),false);};
    tick(4);tick(5);tick(1|128|256);
    require(baseline->practice_players[0].active_normal==skullgirls::gregor_start,"Netplay fixture starts Gregor");
    bool second=false;unsigned max_hits=0;
    while(inputs.size()<700){
        auto& f=baseline->practice_players[0];
        if(!second && f.active_normal<0 && baseline->hud_stats[0].hits){
            const auto forward=f.practice_x<baseline->practice_players[1].practice_x?2u:1u;
            tick(forward);tick(4);tick(4|forward|16|32);second=true;
        }
        tick(0);max_hits=std::max(max_hits,baseline->hud_stats[0].hits);
    }
    require(second && max_hits==15,"Confirmed-input baseline completes the 15-hit double Blockbuster combo");
    require(baseline->hud_stats[1].life<15500,"Network match applies damage without Training invulnerability");
    // Exercise both human sides after the combo, including jumps and Robo's beam.
    for(unsigned f=0;f<300;++f){
        const unsigned phase=f%80;
        unsigned robo=phase<10?1u:phase==12?4u:phase==13?5u:phase==14?1u|16u:phase==40?8u:0u;
        tick(f%90<12?2u:0u,robo);
    }
    auto a=std::make_unique<Menu>(),b=std::make_unique<Menu>();prepare(*a,data);prepare(*b,data);
    auto p0=std::make_shared<Pipe>(),p1=std::make_shared<Pipe>();p0->other=p1.get();p1->other=p0.get();
    skullgirls::NetplaySession s0(0,a->netplay_callbacks(decode,checksum),p0),s1(1,b->netplay_callbacks(decode,checksum),p1);
    s0.notify(skullgirls::NetplaySession::Event::running);s1.notify(skullgirls::NetplaySession::Event::running);
    for(unsigned t=0;t<2000 && (s0.stats().frame<int(inputs.size()) || s1.stats().frame<int(inputs.size()));++t){
        p0->clock=p1->clock=t;s0.idle();s1.idle();
        if(s0.stats().frame<int(inputs.size()))s0.step(fixture(inputs[s0.stats().frame][0]));
        if(s1.stats().frame<int(inputs.size()))s1.step(fixture(inputs[s1.stats().frame][1]));
    }
    for(unsigned i=0;i<20;++i){++p0->clock;++p1->clock;s0.idle();s1.idle();}
    require(s0.stats().frame==int(inputs.size()) && s1.stats().frame==int(inputs.size()),"Both combat peers advance all frames");
    const auto expected=observe(baseline->capture_netplay_state());
    require(observe(a->capture_netplay_state())==expected,"Peer one combat state matches confirmed-input execution");
    require(observe(b->capture_netplay_state())==expected,"Peer two combat state matches confirmed-input execution");
    require(s0.stats().rollbacks && s1.stats().rollbacks,"Both combat peers actually roll back");
    std::cout<<inputs.size()<<" combat frames, 15-hit Gregor/Fenrir, damage, two-sided input, presentation and entity replay converged; rollbacks "<<s0.stats().rollbacks<<'/'<<s1.stats().rollbacks<<'\n';
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
#else
int main(int argc,char** argv)
#endif
{if(argc!=2)return 2;Gdiplus::GdiplusStartupInput startup;ULONG_PTR graphics=0;
if(Gdiplus::GdiplusStartup(&graphics,&startup,nullptr)!=Gdiplus::Ok)return 2;
int result=0;try{run(argv[1]);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}
Gdiplus::GdiplusShutdown(graphics);return result;}
