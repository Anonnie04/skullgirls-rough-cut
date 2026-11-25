#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>

namespace {
void check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
void commands(Menu& menu, bool facing) {
    constexpr unsigned normals[][6] = {{112,117,122,115,119,125}, {126,128,130,127,129,131}, {132,135,159,133,136,160}};
    for (unsigned stance=0; stance<3; ++stance) for (unsigned button=0; button<6; ++button) {
        Menu::PracticeFighter fighter;
        auto& actor=fighter.parasoul.emplace(); actor.facing_right=facing;
        fighter.practice_x=800;
        if (stance==2) { actor.start(97); actor.state.variables[0x3b]=2.0f; fighter.jump_height=450; }
        const auto tick=[&](unsigned buttons) {
            fighter.practice_down=stance==1;
            fighter.pressed_punches=buttons&7u; fighter.pressed_kicks=buttons>>3;
            fighter.held_attack_buttons=buttons;
            menu.tick_parasoul(fighter,facing?1500.0f:100.0f,0,1700,facing);
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
    for (auto name:{"parasoul","filia","cerebella","peacock"}) { const int n=id(name); menu.palettes[n].push_back({0,skullgirls::CharacterAssets(menu.game_data,name).read_default_palette()}); }
    for (auto partner:{"parasoul","filia","cerebella","peacock"}) {
        menu.fighters[0]={id("parasoul")}; menu.fighters[1]={id(partner)};
        for(auto& colors:menu.locked_colors) colors={0};
        menu.prepare_practice_bitmaps(); menu.reset_practice();
        menu.start_practice_intro();
        check(menu.practice_presentation.phase == Menu::PracticePresentation::Phase::intro, "Parasoul starts intro");
        for (unsigned i=0; i<600 && menu.practice_presentation.phase != Menu::PracticePresentation::Phase::none; ++i) menu.practice_tick();
        check(menu.practice_presentation.phase == Menu::PracticePresentation::Phase::none && menu.practice_players[0].parasoul->move == 20, "Intro returns to idle");
        check(menu.practice_slot_playable(0,0),"Parasoul is playable");
        Gdiplus::Bitmap canvas(640,480,PixelFormat32bppARGB); Gdiplus::Graphics dc(&canvas);
        dc.Clear(Gdiplus::Color::Transparent);
        menu.draw_practice_frame(dc,0,2,320,410,false,false);
        bool painted=false;
        for(int y=20;y<440;y+=12) for(int x=120;x<520;x+=12) { Gdiplus::Color pixel; canvas.GetPixel(x,y,&pixel); painted|=pixel.GetAlpha()!=0; }
        check(painted,"Parasoul renders using her own palette and sprites");
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
        check(menu.hud_stats[0].blocked_hits && !menu.hud_stats[0].damage, "Parasoul jab can be guarded");
        menu.training_options.pop_back();
        menu.reset_practice(); menu.practice_key('A',true); menu.practice_tick(); menu.practice_key('A',false);
        const unsigned saved=menu.practice_players[0].parasoul->move;
        menu.save_or_restore_practice(false); menu.practice_tick(); menu.reset_practice(); menu.save_or_restore_practice(true);
        check(menu.practice_players[0].parasoul && menu.practice_players[0].parasoul->move==saved,"Practice state restores Parasoul playback");
    }
}
void movement_and_defense(Menu& menu) {
    for (const bool facing : {false, true}) {
        Menu::PracticeFighter fighter;
        fighter.practice_x = 800;
        auto& actor = fighter.parasoul.emplace(); actor.facing_right = facing;
        fighter.practice_right = facing; fighter.practice_left = !facing;
        for (unsigned i=0; i<20; ++i) menu.tick_parasoul(fighter, facing ? 1500 : 100, 0, 1700, facing);
        check(facing ? fighter.practice_x > 800 : fighter.practice_x < 800, "Walk moves in facing direction");
        fighter.practice_left = fighter.practice_right = false;
        fighter.practice_up = true;
        bool rose = false;
        for (unsigned i=0; i<120; ++i) {
            menu.tick_parasoul(fighter, facing ? 1500 : 100, 0, 1700, facing);
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
        check(fighter.parasoul->move==(stance==2?427u:stance==1?421u:412u), "Pushblock selects Parasoul stance");
        for (unsigned i=0;i<100;++i) menu.tick_parasoul(fighter,1500,0,1700,true);
        check(!fighter.blockstun && fighter.guard_phase==Menu::PracticeFighter::GuardPhase::none,"Pushblock recovers");
    }
    for (bool air : {false,true}) {
        menu.reset_practice(); auto& fighter=menu.practice_players[0];
        fighter.hurt_group=skullgirls::air_mid; fighter.burst_available=true;
        fighter.jump_height=air?450.0f:0.0f; fighter.pressed_punches=1;
        menu.advance_training_bursts();
        check(fighter.parasoul->move==439,"Burst uses Parasoul move");
        for(unsigned i=0;i<300;++i) menu.tick_parasoul(fighter,1500,0,1700,true);
        check(!fighter.burst_ticks && !fighter.burst_fall_tick && fighter.jump_height==0 && fighter.parasoul->move==20,"Burst lands and returns to idle air="+std::to_string(air)+" move="+std::to_string(fighter.parasoul->move)+" height="+std::to_string(fighter.jump_height)+" burst="+std::to_string(fighter.burst_ticks)+" fall="+std::to_string(fighter.burst_fall_tick));
    }
}
void specials(Menu& menu, bool facing) {
    const auto ready=[&](bool air=false) {
        Menu::PracticeFighter f; f.practice_x=800;f.parasoul.emplace().facing_right=facing;
        if(air){f.jump_height=450;f.parasoul->start(97);f.parasoul->state.variables[0x3b]=2.0f;}
        return f;
    };
    const auto tick=[&](Menu::PracticeFighter& f,unsigned dir,unsigned buttons=0) {
        constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
        f.practice_left=dir>=3 && dir<=5;f.practice_right=dir==0 || dir==1 || dir==7;
        f.practice_down=dir>=1 && dir<=3;f.practice_up=dir>=5 && dir<=7;
        f.pressed_punches=buttons&7;f.pressed_kicks=buttons>>3;f.held_attack_buttons=buttons;
        menu.tick_parasoul(f,facing?1500:100,0,1700,facing);
    };
    const auto finishes=[&](Menu::PracticeFighter& f,unsigned entry) {
        for(unsigned i=0;i<700 && f.parasoul->attacking();++i)tick(f,8);
        check(!f.parasoul->attacking(),"Recovery "+menu.parasoul_assets.names[entry]+" stuck="+std::to_string(f.parasoul->move));
    };
    struct Command { std::initializer_list<unsigned> motion;unsigned buttons,move,cost;bool air=false; };
    for(const auto& c : {Command{{2,3,4},8,189,0},{{2,3,4},16,189,0},{{2,3,4},32,189,0},
        {{2,3,4},8,201,0,true},{{2,3,4},16,201,0,true},{{2,3,4},32,201,0,true},
        {{2,1,0},3,220,1},{{2,1,0},24,224,1},{{2,3,4},3,221,3},
        {{2,1,0},18,229,1},{{2,1,0},36,229,1},{{8},9,165,0},{{8},9,174,0,true}}) {
        auto f=ready(c.air);
        for(auto i=c.motion.begin();i!=c.motion.end();++i)tick(f,*i,i+1==c.motion.end()?c.buttons:0);
        check(f.parasoul->move==c.move,"Command "+menu.parasoul_assets.names[c.move]+" got="+menu.parasoul_assets.names[f.parasoul->move]);
        check(f.tension==5-c.cost,"Command meter cost");finishes(f,c.move);
        if(c.cost){auto empty=ready(c.air);empty.tension=c.cost-1;for(auto i=c.motion.begin();i!=c.motion.end();++i)tick(empty,*i,i+1==c.motion.end()?c.buttons:0);check(empty.parasoul->move!=c.move && empty.tension==c.cost-1,"Insufficient meter rejects command");}
    }
    for(unsigned charge : {10u,40u}) for(unsigned strength=0;strength<3;++strength) for(unsigned kind=0;kind<3;++kind) {
        auto f=ready();
        for(unsigned i=0;i<charge;++i)tick(f,kind==1?2:4);
        tick(f,kind==1?6:0,(kind?8u:1u)<<strength);
        const unsigned expected=kind==0?210+strength:kind==1?183+strength:215+strength;
        check((f.parasoul->move==expected || (!kind && expected==210 && f.parasoul->move==213))==(charge==40),
            "Charge command "+menu.parasoul_assets.names[expected]+" duration="+std::to_string(charge)+" got="+menu.parasoul_assets.names[f.parasoul->move]);
        finishes(f,expected);
    }
}
void teams(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("filia"),id("parasoul"),id("cerebella")};
    menu.fighters[1]={id("parasoul")};
    menu.locked_colors[0]={0,0,0}; menu.locked_colors[1]={0};
    menu.reset_practice();
    for(unsigned ref:{1012u,1013u,1014u,119u,134u,135u,150u,153u,176u,180u,113u}) {
        auto pose=menu.parasoul_team_pose(ref);
        check(pose>1 && menu.parasoul_frames.at(pose).width,"Parasoul team travel visible");
    }
    menu.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
    for(unsigned i=0;i<300 && menu.practice_teams[0].tag;++i) menu.practice_tick();
    check(menu.practice_teams[0].active_slot==1 && menu.practice_players[0].parasoul,"Tag activates Parasoul playback");
    menu.begin_practice_tag(0,2,Menu::PracticeTag::Kind::regular);
    for(unsigned i=0;i<300 && menu.practice_teams[0].tag;++i) menu.practice_tick();
    check(menu.practice_teams[0].active_slot==2 && menu.practice_players[0].cerebella,"Parasoul tags out into Cerebella");
    menu.reset_practice();
    menu.practice_key('A',true); menu.practice_key('X',true); menu.practice_tick();
    menu.practice_key('A',false); menu.practice_key('X',false);
    auto& team=menu.practice_teams[0];
    check(team.assist && team.assist->fighter.parasoul,"Parasoul assist has own playback");
    bool acted=false;
    for(unsigned i=0;i<400 && team.assist;++i) { menu.practice_tick(); acted |= team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting; }
    check(acted && !team.assist,"Parasoul assist acts and exits");
}
void combat(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("parasoul")};menu.fighters[1]={id("filia")};menu.locked_colors[0]=menu.locked_colors[1]={0};
    for(bool facing : {false,true})for(unsigned move : {112u,117u,128u,160u,184u,185u,210u,211u,212u,217u,220u,221u,224u,165u,174u}) {
        menu.reset_practice();auto& f=menu.practice_players[0];auto& target=menu.practice_players[1];
        f.practice_x=facing?760.0f:940.0f;target.practice_x=facing?878.0f:822.0f;
        f.parasoul->facing_right=facing;f.parasoul->start(move);++f.attack_instance;
        const bool air=move==174 || move==160;
        if(air){f.jump_height=target.jump_height=450;f.parasoul->state.variables[0x3b]=2.0f;}
        unsigned damage=0;bool captured=false;
        for(unsigned i=0;i<800;++i){menu.practice_tick();damage=std::max(damage,menu.hud_stats[0].damage);captured|=target.throw_phase==Menu::PracticeFighter::ThrowPhase::grabbed;}
        check(damage>0,"Combat "+menu.parasoul_assets.names[move]+" facing="+std::to_string(facing)+" captured="+std::to_string(captured));
        if(move==165 || move==174 || move==217)check(captured && target.throw_phase!=Menu::PracticeFighter::ThrowPhase::grabbed,"Throw captures and releases");
        check(!f.parasoul->attacking(),"Combat recovery "+menu.parasoul_assets.names[move]);
    }
    for(bool facing : {false,true}) {
        menu.reset_practice();auto& f=menu.practice_players[0];auto& target=menu.practice_players[1];
        f.practice_x=facing?600.0f:1100.0f;target.practice_x=facing?1000.0f:700.0f;
        f.parasoul->facing_right=facing;f.parasoul->start(210);++f.attack_instance;
        bool tear=false;float tear_x=0,tear_y=0;
        for(unsigned i=0;i<180 && !tear;++i) {
            menu.practice_tick();
            for(const auto&e:menu.practice_effects)if(e.table==&menu.parasoul_assets.children[34] && e.alive && (e.move==2 || e.move==9)) {tear=true;tear_x=e.x;tear_y=e.y;}
        }
        check(tear,"Shot leaves a set tear after contact");
        for(unsigned i=0;i<30;++i)menu.practice_tick();
        check(std::any_of(menu.practice_effects.begin(),menu.practice_effects.end(),[&](const auto&e){return e.table==&menu.parasoul_assets.children[34] && (e.move==2 || e.move==9);}),"Unignited tear persists independently");
        menu.save_or_restore_practice(false);const auto count=menu.practice_effects.size();
        menu.practice_effects.clear();menu.save_or_restore_practice(true);
        check(menu.practice_effects.size()==count,"Save restores set tears");
        target.practice_x=tear_x;target.jump_height=std::max(0.0f,tear_y-120);target.jump_velocity=0;
        target.hurt_group=-1;target.hurt_hold=target.hit_stop=0;
        f.parasoul->start(183);++f.attack_instance;
        menu.hud_stats[0].hits=menu.hud_stats[0].damage=0;bool ignited=false;
        for(unsigned i=0;i<120;++i){menu.practice_tick();for(const auto&e:menu.practice_effects)if(e.table==&menu.parasoul_assets.children[34] && e.hit)ignited=true;}
        check(ignited && menu.hud_stats[0].hits>0,"Napalm Quake ignites tear: ignited="+std::to_string(ignited)+" hits="+std::to_string(menu.hud_stats[0].hits));
    }
    menu.reset_practice();menu.practice_players[0].parasoul->start(186);
    bool tossed=false,set=false;
    for(unsigned i=0;i<200;++i){menu.practice_tick();for(const auto&e:menu.practice_effects){tossed|=e.table==&menu.parasoul_assets.children[33];set|=e.table==&menu.parasoul_assets.children[34];}}
    check(tossed && set,"Napalm Toss arcs into a persistent tear");
    menu.fighters[1]={id("parasoul")};menu.reset_practice();
    menu.practice_players[0].practice_x=600;menu.practice_players[1].practice_x=1100;
    menu.practice_players[0].parasoul->start(216);
    for(unsigned i=0;i<15;++i)menu.practice_tick();
    menu.practice_players[1].parasoul->facing_right=false;menu.practice_players[1].parasoul->start(210);
    bool intercepted=false;
    for(unsigned i=0;i<180;++i){menu.practice_tick();for(const auto&e:menu.practice_effects)if(e.table==&menu.parasoul_assets.children[44])intercepted|=e.value(6)!=0;}
    check(intercepted && !menu.hud_stats[1].damage,"Egret Dive intercepts an incoming Napalm Shot");
}
void throw_victim(Menu& menu) {
    auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("filia")}; menu.fighters[1]={id("parasoul")};
    menu.locked_colors[0]=menu.locked_colors[1]={0};
    for (bool tech : {false,true}) {
        menu.reset_practice();
        menu.training_options.push_back({L"Throw tech", {}, tech?1u:0u, "OPT_STR_THROWTECH"});
        menu.practice_key('A',true); menu.practice_key('Z',true); menu.practice_tick();
        menu.practice_key('A',false); menu.practice_key('Z',false);
        bool captured=false, teched=false;
        for(unsigned i=0;i<300;++i) {
            menu.practice_tick();
            const auto& defender=menu.practice_players[1];
            captured |= defender.throw_phase==Menu::PracticeFighter::ThrowPhase::grabbed;
            teched |= defender.throw_phase==Menu::PracticeFighter::ThrowPhase::tech;
            check(defender.sprite() < menu.parasoul_frames.size(), "Throw victim uses Parasoul sprite references");
        }
        check(tech ? teched && !menu.hud_stats[0].damage : captured, "Parasoul throw victim/tech outcome tech="+std::to_string(tech)+" captured="+std::to_string(captured)+" teched="+std::to_string(teched)+" damage="+std::to_string(menu.hud_stats[0].damage));
        check(menu.practice_players[1].throw_phase==Menu::PracticeFighter::ThrowPhase::none, "Parasoul throw victim recovers");
        menu.training_options.pop_back();
    }
}
void egret_camera(Menu& menu) {
    const float width=menu.practice_width;
    menu.practice_width=4000;
    for (bool facing:{false,true}) {
        menu.reset_practice();
        auto& fighter=menu.practice_players[0];
        fighter.practice_x=facing?2200.0f:2300.0f;
        menu.practice_players[1].practice_x=facing?2320.0f:2180.0f;
        menu.update_practice_camera(true);
        fighter.parasoul->facing_right=facing;
        fighter.parasoul->start(215);++fighter.attack_instance;
        for(unsigned i=0;i<12;++i)menu.practice_tick();
        auto egret=std::find_if(menu.practice_effects.begin(),menu.practice_effects.end(),[&](const auto& e){
            return e.root==&menu.parasoul_assets && e.table==&menu.parasoul_assets.children[42];
        });
        check(egret!=menu.practice_effects.end(),"Egret Call spawns its soldier");
        auto view=menu.practice_viewport();
        check(egret->x>=view.left && egret->x<=view.left+view.width,"Egret enters the visible camera in both facings");
        check(egret->pose>1 && menu.parasoul_frames.at(egret->pose).width,"Egret has visible soldier artwork");
        menu.practice_camera_center-=300;
        menu.practice_camera_zoom=0.5f;
        view=menu.practice_viewport();
        menu.advance_practice_effects(false);
        check(std::abs(egret->value(0x0f)-view.left)<0.01f &&
              std::abs(egret->value(0x10)-(view.left+view.width))<0.01f,"Egret camera bounds refresh during pan and zoom");
    }
    menu.practice_width=width;menu.reset_practice();
}
void portrait_transparency(Menu& menu) {
    skullgirls::SpriteArchive ui(menu.game_data/"ui.gfs"), foits(menu.game_data/"characters-foits.gfs");
    const auto layer=[&](const char* suffix) {
        return skullgirls::read_hud_portrait_layer(ui.read(std::string("ui/palettized_hud_art/character_parasoul_")+suffix+".pcx"));
    };
    const auto colors=layer("colors"), shade=layer("shade"), lines=layer("lines");
    const auto map=skullgirls::read_hud_palette_map(foits.read("characters/parasoul.foit"));
    const auto palette=skullgirls::CharacterAssets(menu.game_data,"parasoul").read_default_palette();
    const auto pixels=skullgirls::colorize_hud_portrait(colors,shade,lines,palette,map);
    const auto hair=pixels.at(370*colors.width+150);
    check(map[4]==2 && (hair>>24)>=250 && ((hair>>16)&255)>(hair&255),
        "Parasoul red hair stays opaque instead of exposing the portrait background");
    check(pixels.front()==0,"Parasoul portrait background stays transparent");
}
void projectile_regressions(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("parasoul")};menu.fighters[1]={id("filia")};menu.locked_colors[0]=menu.locked_colors[1]={0};
    const auto visible = [&](const skullgirls::FoitEffectPlayback& effect) {
        return effect.alive && effect.pose>1 && effect.pose<menu.parasoul_frames.size() &&
            menu.parasoul_frames[effect.pose].width && effect.value(0xa4)>0 && effect.value(0xa5)>0 && effect.value(0xa6)>0;
    };
    const auto rendered = [&](const skullgirls::FoitEffectPlayback& effect) {
        Gdiplus::Bitmap canvas(1024,1024,PixelFormat32bppARGB);Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color::Transparent);
        dc.TranslateTransform(512,512);dc.ScaleTransform(effect.value(0xa4),effect.value(0xa5));
        menu.draw_practice_frame(dc,0,effect.pose,0,0,!effect.facing_right,false,{},effect.value(0xa6));
        for(int y=0;y<1024;y+=8)for(int x=0;x<1024;x+=8){Gdiplus::Color pixel;canvas.GetPixel(x,y,&pixel);if(pixel.GetAlpha())return true;}
        return false;
    };
    for(bool facing:{false,true})for(const auto& spec : {std::pair{210u,32u},{211u,32u},{212u,32u},{186u,33u},{187u,33u},{188u,33u}}) {
        menu.reset_practice();auto& f=menu.practice_players[0];f.practice_x=facing?300.0f:1400.0f;
        menu.practice_players[1].practice_x=facing?1550.0f:150.0f;
        f.parasoul->facing_right=facing;f.parasoul->start(spec.first);++f.attack_instance;
        bool painted=false;unsigned first=0;
        for(unsigned t=0;t<200 && !painted;++t){menu.practice_tick();for(const auto& effect:menu.practice_effects)
            if(effect.root==&menu.parasoul_assets && effect.table==&menu.parasoul_assets.children[spec.second] && visible(effect)){
                painted=rendered(effect);first=t;break;
            }}
        check(painted,"Visible projectile artwork "+menu.parasoul_assets.names[spec.first]+" facing="+std::to_string(facing));
        check(first>0 && first<100,"Projectile respects the move's startup");
    }

    for(bool facing:{false,true}) {
        menu.reset_practice();auto& f=menu.practice_players[0];f.practice_x=facing?300.0f:1400.0f;
        menu.practice_players[1].practice_x=facing?1600.0f:100.0f;f.parasoul->facing_right=facing;f.parasoul->start(210);++f.attack_instance;
        std::uint64_t shot=0;float start=0,height=0;unsigned launch=0;bool travelled=false,independent=false;
        for(unsigned t=0;t<60;++t){menu.practice_tick();for(const auto& e:menu.practice_effects)if(e.table==&menu.parasoul_assets.children[32] && e.move==5){
            if(!shot){shot=e.entity_id;start=e.x;height=e.y;launch=t;}
            if(e.entity_id==shot && t==launch+10){check(std::abs((e.x-start)*(facing?1:-1)-240)<0.1f && e.y==height,"Light Napalm Shot travels straight at 24 units/tick");travelled=true;}
            independent|=!f.parasoul->attacking();
        }}
        check(travelled && independent,"Napalm Shot stays active after owner recovery");
        menu.reset_practice();f.parasoul->facing_right=facing;f.practice_x=facing?300.0f:1400.0f;menu.practice_players[1].practice_x=facing?1600.0f:100.0f;
        f.parasoul->start(211);++f.attack_instance;bool down=false,bounced=false;float previous=10000;
        for(unsigned t=0;t<80;++t){menu.practice_tick();for(const auto&e:menu.practice_effects)if(e.table==&menu.parasoul_assets.children[32] && e.move==6){
            down|=e.value(0x9f)<0 && e.y<previous;bounced|=down && e.value(0x9f)>0;previous=e.y;
        }}
        check(down && bounced,"Medium Napalm Shot uses its downward trajectory and floor bounce");
    }
    // A released toss sets in the air instead of skating along the floor;
    // its finite tear timer proceeds even while the owner stands idle.
    menu.reset_practice();menu.practice_players[0].parasoul->start(186);bool set=false,expired=false;
    for(unsigned t=0;t<350;++t){menu.practice_tick();for(const auto&e:menu.practice_effects)if(e.table==&menu.parasoul_assets.children[34]){
        set|=e.move==2;expired|=e.move>=3;
    }}
    check(set && expired,"Set tears expire/ignite on their authored timer");

}

}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    if(argc!=2) return 2;
    Gdiplus::GdiplusStartupInput input; ULONG_PTR graphics=0;
    if(Gdiplus::GdiplusStartup(&graphics,&input,nullptr)!=Gdiplus::Ok) return 2;
    int result=0;
    try { Menu menu; menu.game_data=argv[1]; menu.load_parasoul();
        check(menu.parasoul_assets.animations.size()==568 && menu.parasoul_frames[2].width,"Parasoul assets load");
        commands(menu,true); commands(menu,false); specials(menu,true);specials(menu,false); training(menu); movement_and_defense(menu); teams(menu); throw_victim(menu);combat(menu);projectile_regressions(menu);egret_camera(menu);portrait_transparency(menu);
        std::cout<<"Parasoul moves, projectiles, throws, and Training integration passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; result=1; }
    Gdiplus::GdiplusShutdown(graphics); return result;
}
