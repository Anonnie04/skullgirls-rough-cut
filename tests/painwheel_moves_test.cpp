#include "../src/main_menu.cpp"
#include <iostream>

namespace {
void check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
void commands(Menu& menu, bool facing) {
    constexpr unsigned normals[][6] = {{112,114,117,113,115,119}, {125,127,130,126,128,132}, {133,136,139,135,138,141}};
    for (unsigned stance=0; stance<3; ++stance) for (unsigned button=0; button<6; ++button) {
        Menu::PracticeFighter fighter;
        auto& actor=fighter.painwheel.emplace(); actor.facing_right=facing;
        fighter.practice_x=800;
        if (stance==2) { actor.start(99); actor.state.variables[0x3b]=2.0f; fighter.jump_height=450; }
        const auto tick=[&](unsigned buttons) {
            fighter.practice_down=stance==1;
            fighter.pressed_punches=buttons&7u; fighter.pressed_kicks=buttons>>3;
            fighter.held_attack_buttons=buttons;
            menu.tick_painwheel(fighter,facing?1500.0f:100.0f,0,1700,facing);
        };
        tick(1u<<button);
        check(actor.move==normals[stance][button], "Normal entry stance="+std::to_string(stance)+" button="+std::to_string(button)+" got="+std::to_string(actor.move));
        bool hit=actor.hit!=nullptr;
        for (unsigned i=0;i<240 && actor.attacking();++i) { tick(0); hit|=actor.hit!=nullptr; }
        check(!actor.attacking(), "Normal completes recovery");
        // Check body hit data for ground normals; spawned child combat is a later milestone.
        if (stance!=2 && !(stance==1 && button==1)) check(hit,"Normal exposes original hit descriptor stance="+std::to_string(stance)+" button="+std::to_string(button));
    }
}
void training(Menu& menu) {
    auto id=[](std::string_view name) { return int(std::find(roster.begin(),roster.end(),name)-roster.begin()); };
    menu.load_movement_frames(); menu.load_cerebella(); menu.load_peacock();
    menu.practice_width=1700; menu.practice_height=900; menu.practice_floor=145; menu.practice_starts={760,878};
    for (auto name:{"painwheel","filia","cerebella","peacock"}) { const int n=id(name); menu.palettes[n].push_back({0,skullgirls::CharacterAssets(menu.game_data,name).read_default_palette()}); }
    for (auto partner:{"painwheel","filia","cerebella","peacock"}) {
        menu.fighters[0]={id("painwheel")}; menu.fighters[1]={id(partner)};
        for(auto& colors:menu.locked_colors) colors={0};
        menu.prepare_practice_bitmaps(); menu.reset_practice();
        menu.start_practice_intro();
        check(menu.practice_presentation.phase == Menu::PracticePresentation::Phase::intro, "Painwheel starts intro");
        for (unsigned i=0; i<600 && menu.practice_presentation.phase != Menu::PracticePresentation::Phase::none; ++i) menu.practice_tick();
        check(menu.practice_presentation.phase == Menu::PracticePresentation::Phase::none && menu.practice_players[0].painwheel->move == 21, "Intro returns to idle");
        check(menu.practice_slot_playable(0,0),"Painwheel is playable");
        Gdiplus::Bitmap canvas(640,480,PixelFormat32bppARGB); Gdiplus::Graphics dc(&canvas);
        dc.Clear(Gdiplus::Color::Transparent);
        menu.draw_practice_frame(dc,0,2,320,410,false,false);
        bool painted=false;
        for(int y=20;y<440;y+=12) for(int x=120;x<520;x+=12) { Gdiplus::Color pixel; canvas.GetPixel(x,y,&pixel); painted|=pixel.GetAlpha()!=0; }
        check(painted,"Painwheel renders using her own palette and sprites");
        for(unsigned side:{0u,1u}) {
            menu.reset_practice(); const WPARAM jab=side?'V':'A';
            menu.practice_key(jab,true); menu.practice_tick(); menu.practice_key(jab,false);
            for(unsigned i=0;i<50 && !menu.hud_stats[side].damage;++i) menu.practice_tick();
            check(menu.hud_stats[side].damage>0,"Jab hits mixed-character hurtboxes side="+std::to_string(side)+" partner="+partner);
        }
        menu.reset_practice();
        menu.training_options.push_back({L"Block", {}, 2, "OPT_STR_BLOCKING"});
        menu.practice_key('A',true); menu.practice_tick(); menu.practice_key('A',false);
        for (unsigned i=0;i<50 && !menu.hud_stats[0].blocked_hits;++i) menu.practice_tick();
        check(menu.hud_stats[0].blocked_hits && !menu.hud_stats[0].damage, "Painwheel jab can be guarded");
        menu.training_options.pop_back();
        menu.reset_practice(); menu.practice_key('A',true); menu.practice_tick(); menu.practice_key('A',false);
        const unsigned saved=menu.practice_players[0].painwheel->move;
        menu.save_or_restore_practice(false); menu.practice_tick(); menu.reset_practice(); menu.save_or_restore_practice(true);
        check(menu.practice_players[0].painwheel && menu.practice_players[0].painwheel->move==saved,"Practice state restores Painwheel playback");
    }
}

void movement_and_defense(Menu& menu) {
    for (const bool facing : {false, true}) {
        Menu::PracticeFighter fighter;
        fighter.practice_x = 800;
        auto& actor = fighter.painwheel.emplace(); actor.facing_right = facing;
        fighter.practice_right = facing; fighter.practice_left = !facing;
        for (unsigned i=0; i<20; ++i) menu.tick_painwheel(fighter, facing ? 1500 : 100, 0, 1700, facing);
        check(facing ? fighter.practice_x > 800 : fighter.practice_x < 800, "Walk moves in facing direction");
        fighter.practice_left = fighter.practice_right = false;
        fighter.practice_up = true;
        bool rose = false;
        for (unsigned i=0; i<120; ++i) {
            menu.tick_painwheel(fighter, facing ? 1500 : 100, 0, 1700, facing);
            if (i>8) fighter.practice_up = false;
            rose |= fighter.jump_height > 100;
        }
        check(rose && fighter.jump_height == 0, "Jump rises and lands in both facings");
    }
    for (unsigned stance=0; stance<3; ++stance) {
        menu.reset_practice();
        auto& fighter=menu.practice_players[0];
        fighter.blockstun=30; fighter.crouching=stance==1; fighter.jump_height=stance==2?450.0f:0.0f;
        fighter.pressed_punches=fighter.held_attack_buttons=3;
        menu.advance_training_pushblock(0,1700);
        check(fighter.painwheel->move==(stance==2?445u:stance==1?439u:430u), "Pushblock selects Painwheel stance");
        for (unsigned i=0;i<100;++i) menu.tick_painwheel(fighter,1500,0,1700,true);
        check(!fighter.blockstun && fighter.guard_phase==Menu::PracticeFighter::GuardPhase::none,"Pushblock recovers");
    }
    for (bool air : {false,true}) {
        menu.reset_practice(); auto& fighter=menu.practice_players[0];
        fighter.hurt_group=skullgirls::air_mid; fighter.burst_available=true;
        fighter.jump_height=air?450.0f:0.0f; fighter.pressed_punches=1;
        menu.advance_training_bursts();
        check(fighter.painwheel->move==457,"Burst uses Painwheel move");
        for(unsigned i=0;i<300;++i) menu.tick_painwheel(fighter,1500,0,1700,true);
        check(!fighter.burst_ticks && !fighter.burst_fall_tick && fighter.jump_height==0 && fighter.painwheel->move==21,"Burst lands and returns to idle air="+std::to_string(air)+" move="+std::to_string(fighter.painwheel->move)+" height="+std::to_string(fighter.jump_height)+" burst="+std::to_string(fighter.burst_ticks)+" fall="+std::to_string(fighter.burst_fall_tick));
    }
}
void teams(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("filia"),id("painwheel"),id("cerebella")};
    menu.fighters[1]={id("painwheel")};
    menu.locked_colors[0]={0,0,0}; menu.locked_colors[1]={0};
    menu.reset_practice();
    for(unsigned ref:{1012u,1013u,1014u,119u,134u,135u,150u,153u,176u,180u,113u}) {
        auto pose=menu.painwheel_team_pose(ref);
        check(pose>1 && menu.painwheel_frames.at(pose).width,"Painwheel team travel visible");
    }
    menu.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
    for(unsigned i=0;i<300 && menu.practice_teams[0].tag;++i) menu.practice_tick();
    check(menu.practice_teams[0].active_slot==1 && menu.practice_players[0].painwheel,"Tag activates Painwheel playback");
    menu.begin_practice_tag(0,2,Menu::PracticeTag::Kind::regular);
    for(unsigned i=0;i<300 && menu.practice_teams[0].tag;++i) menu.practice_tick();
    check(menu.practice_teams[0].active_slot==2 && menu.practice_players[0].cerebella,"Painwheel tags out into Cerebella");
    menu.reset_practice();
    menu.practice_key('A',true); menu.practice_key('X',true); menu.practice_tick();
    menu.practice_key('A',false); menu.practice_key('X',false);
    auto& team=menu.practice_teams[0];
    check(team.assist && team.assist->fighter.painwheel,"Painwheel assist has own playback");
    bool acted=false;
    for(unsigned i=0;i<400 && team.assist;++i) { menu.practice_tick(); acted |= team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting; }
    check(acted && !team.assist,"Painwheel assist acts and exits move="+std::to_string(team.assist?team.assist->fighter.painwheel->move:0));
}
void specials(Menu& menu,bool facing) {
    const auto tick=[&](Menu::PracticeFighter& f,unsigned dir=8,unsigned buttons=0) {
        constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
        f.practice_left=dir>=3 && dir<=5;f.practice_right=dir==0 || dir==1 || dir==7;
        f.practice_down=dir>=1 && dir<=3;f.practice_up=dir>=5 && dir<=7;
        f.pressed_punches=buttons&7;f.pressed_kicks=buttons>>3;f.held_attack_buttons=buttons;
        menu.tick_painwheel(f,facing?1500:100,0,1700,facing);
    };
    struct Command { std::initializer_list<unsigned> motion;unsigned buttons,move,cost;bool air=false; };
    for(const auto& c : {Command{{2,1,0},1,182,0},{{2,1,0},2,183,0},{{2,1,0},4,184,0},
        {{2,1,0},8,185,0},{{2,1,0},16,187,0},{{2,1,0},32,189,0},
        {{2,1,0},8,191,0,true},{{2,1,0},16,194,0,true},{{2,1,0},32,197,0,true},
        {{2,1,0},3,222,1},{{2,3,4},24,231,2},{{2,3,4},3,236,3},{{2,1,0},24,225,1,true},
        {{2,1,0},18,247,1},{{2,1,0},36,247,1},{{8},9,146,0},{{8},9,159,0,true},{{2,3,4},8,200,0},{{2,3,4},8,201,0,true}}) {
        Menu::PracticeFighter f; f.practice_x=800;auto& actor=f.painwheel.emplace();actor.facing_right=facing;
        if(c.air){f.jump_height=1000;actor.start(99);actor.state.variables[0x3b]=2.0f;}
        for(auto i=c.motion.begin();i!=c.motion.end();++i)tick(f,*i,i+1==c.motion.end()?c.buttons:0);
        check(actor.move==c.move,"Command "+menu.painwheel_assets.names[c.move]+" got="+menu.painwheel_assets.names[actor.move]);
        check(f.tension==5-c.cost,"Command meter cost "+std::to_string(c.move));
        if(c.move==200 || c.move==201) {
            for(unsigned i=0;i<20;++i)tick(f,7);
            check(actor.flying() && f.jump_height>0,"Flight activates and climbs");
            float x=f.practice_x,y=f.jump_height;for(unsigned i=0;i<10;++i)tick(f,4);
            check((f.practice_x-x)*(facing?1:-1)<0 && std::abs(f.jump_height-y)<1,"Flight reverses and hovers");
            tick(f,8,1);check(actor.move==134 || actor.move==133,"Flight normals dispatch");
        } else {
            for(unsigned i=0;i<900 && actor.attacking();++i)tick(f);
            check(!actor.attacking(),"Special recovers "+menu.painwheel_assets.names[c.move]+" stuck="+std::to_string(actor.move));
        }
    }
}
void charge_and_armor(Menu& menu) {
    unsigned lengths[2]{};
    for(unsigned charged=0;charged<2;++charged) {
        Menu::PracticeFighter f;f.practice_x=800;auto& a=f.painwheel.emplace();a.start(117);
        bool armor=false;
        for(unsigned t=0;t<300;++t) {
            f.held_attack_buttons=charged && t<40?4:0;
            menu.tick_painwheel(f,1500,0,1700,true);
            armor|=a.value(0x7b)>0;
            ++lengths[charged];if(!a.attacking())break;
        }
        if(charged)check(armor,"Charged heavy punch has authored armor window");
        check(!a.attacking(),"Charged heavy punch recovers after release");
    }
    check(lengths[1]>lengths[0],"Holding heavy punch extends its charge");
    Menu::PracticeFighter f;f.practice_x=800;auto& a=f.painwheel.emplace();a.start(117);
    f.held_attack_buttons=4;
    for(unsigned t=0;t<20 && a.value(0x7b)<=0;++t)menu.tick_painwheel(f,1500,0,1700,true);
    const float budget=a.value(0x7b);check(budget>0,"Armor activates");
    f.take_hit({0,20,3,3},false,-1);
    check(f.hurt_group<0 && a.move==117 && a.value(0x7b)==budget-1,"Armor absorbs a strike without interrupting the move");
    for(unsigned i=1;i<unsigned(budget);++i)f.take_hit({0,20,3,3},false,-1);
    f.take_hit({0,20,3,3},false,-1);check(f.hurt_group>=0,"Exhausted armor takes normal hit stun");
}
void combat(Menu& menu) {
    auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("painwheel")};menu.fighters[1]={id("filia")};menu.locked_colors[0]=menu.locked_colors[1]={0};
    for(bool facing : {false,true})for(unsigned move : {112u,114u,117u,127u,135u,182u,183u,184u,185u,187u,189u,222u,225u,236u,146u,159u}) {
        menu.reset_practice();auto& f=menu.practice_players[0];auto& target=menu.practice_players[1];
        f.practice_x=facing?760.0f:940.0f;target.practice_x=facing?878.0f:822.0f;
        f.painwheel->facing_right=facing;f.painwheel->start(move);++f.attack_instance;
        if(move==159 || move==135 || move==225){f.jump_height=target.jump_height=450;f.painwheel->state.variables[0x3b]=2.0f;}
        unsigned damage=0;bool captured=false;
        for(unsigned i=0;i<900;++i){menu.practice_tick();damage=std::max(damage,menu.hud_stats[0].damage);captured|=target.throw_phase==Menu::PracticeFighter::ThrowPhase::grabbed;}
        check(damage>0,"Combat "+menu.painwheel_assets.names[move]+" facing="+std::to_string(facing)+" captured="+std::to_string(captured));
        if(move==146 || move==159)check(captured && target.throw_phase!=Menu::PracticeFighter::ThrowPhase::grabbed,"Throw captures and releases");
        check(!f.painwheel->attacking(),"Combat recovery "+menu.painwheel_assets.names[move]+" stuck="+std::to_string(f.painwheel->move));
    }
}

void projectile_regressions(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("painwheel")};menu.fighters[1]={id("filia")};menu.locked_colors[0]=menu.locked_colors[1]={0};
    const auto visible = [&](const skullgirls::FoitEffectPlayback& effect) {
        return effect.alive && effect.pose>1 && effect.pose<menu.painwheel_frames.size() &&
            menu.painwheel_frames[effect.pose].width && effect.value(0xa4)>0 && effect.value(0xa5)>0 && effect.value(0xa6)>0;
    };
    const auto rendered = [&](const skullgirls::FoitEffectPlayback& effect) {
        Gdiplus::Bitmap canvas(1024,1024,PixelFormat32bppARGB);Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color::Transparent);
        dc.TranslateTransform(512,512);dc.ScaleTransform(effect.value(0xa4),effect.value(0xa5));
        menu.draw_practice_frame(dc,0,effect.pose,0,0,!effect.facing_right,false,{},effect.value(0xa6));
        for(int y=0;y<1024;y+=8)for(int x=0;x<1024;x+=8){Gdiplus::Color pixel;canvas.GetPixel(x,y,&pixel);if(pixel.GetAlpha())return true;}
        return false;
    };
    for(bool facing:{false,true})for(const auto& spec : {std::pair{183u,27u},{184u,28u}}) {
        menu.reset_practice();auto& f=menu.practice_players[0];f.practice_x=facing?300.0f:1400.0f;
        menu.practice_players[1].practice_x=facing?1550.0f:150.0f;
        f.painwheel->facing_right=facing;f.painwheel->start(spec.first);++f.attack_instance;
        bool painted=false;unsigned first=0;
        for(unsigned t=0;t<200 && !painted;++t){menu.practice_tick();for(const auto& effect:menu.practice_effects)
            if(effect.root==&menu.painwheel_assets && effect.table==&menu.painwheel_assets.children[spec.second] && visible(effect)){
                painted=rendered(effect);first=t;break;
            }}
        check(painted,"Visible projectile artwork "+menu.painwheel_assets.names[spec.first]+" facing="+std::to_string(facing));
        check(first>0 && first<100,"Projectile respects the move's startup");
    }

    for(bool facing:{false,true}) {
        menu.reset_practice();auto& f=menu.practice_players[0];f.practice_x=facing?300.0f:1400.0f;
        menu.practice_players[1].practice_x=facing?1600.0f:100.0f;f.painwheel->facing_right=facing;f.painwheel->start(183);
        std::uint64_t nail=0;float x=0;unsigned launch=0;bool travelled=false;
        for(unsigned tick=0;tick<120;++tick) {menu.practice_tick();for(const auto& e:menu.practice_effects)
            if(e.table==&menu.painwheel_assets.children[27] && e.alive && visible(e)) {
                if(!nail){nail=e.entity_id;x=e.x;launch=tick;}
                if(nail==e.entity_id && tick==launch+5)travelled=(e.x-x)*(facing?1:-1)>90;
            }
        }
        check(travelled,"Stinger nail travels independently in the facing direction");
    }
}

}
int wmain(int argc,wchar_t** argv) {
    if(argc!=2) return 2;
    Gdiplus::GdiplusStartupInput input; ULONG_PTR graphics=0;
    if(Gdiplus::GdiplusStartup(&graphics,&input,nullptr)!=Gdiplus::Ok) return 2;
    int result=0;
    try { Menu menu; menu.game_data=argv[1]; menu.load_painwheel();
        check(menu.painwheel_assets.animations.size()==578 && menu.painwheel_frames[2].width,"Painwheel assets load");
        commands(menu,true); commands(menu,false); specials(menu,true); specials(menu,false); training(menu); movement_and_defense(menu); teams(menu); charge_and_armor(menu); combat(menu); projectile_regressions(menu);
        std::cout<<"Painwheel moves, projectiles, throws, and Training integration passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; result=1; }
    Gdiplus::GdiplusShutdown(graphics); return result;
}
