#include "../src/main_menu.cpp"
#include <iostream>

namespace {
void check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
void commands(Menu& menu, bool facing) {
    constexpr unsigned normals[][6] = {{120,123,125,122,124,126}, {127,130,132,129,131,133}, {134,136,141,135,140,142}};
    for (unsigned stance=0; stance<3; ++stance) for (unsigned button=0; button<6; ++button) {
        Menu::PracticeFighter fighter;
        auto& actor=fighter.peacock.emplace(); actor.facing_right=facing;
        fighter.practice_x=800;
        if (stance==2) { actor.start(98); actor.state.variables[0x3b]=2.0f; fighter.jump_height=450; }
        const auto tick=[&](unsigned buttons) {
            fighter.practice_down=stance==1;
            fighter.pressed_punches=buttons&7u; fighter.pressed_kicks=buttons>>3;
            fighter.held_attack_buttons=buttons;
            menu.tick_peacock(fighter,facing?1500.0f:100.0f,0,1700,facing);
        };
        tick(1u<<button);
        check(actor.move==normals[stance][button], "Normal entry stance="+std::to_string(stance)+" button="+std::to_string(button)+" got="+std::to_string(actor.move));
        bool hit=actor.hit!=nullptr;
        for (unsigned i=0;i<240 && actor.attacking();++i) { tick(0); hit|=actor.hit!=nullptr; }
        check(!actor.attacking(), "Normal completes recovery");
        // Check body hit data for ground normals; spawned child combat is a later milestone.
        if (stance!=2 && button!=2) check(hit,"Normal exposes original hit descriptor stance="+std::to_string(stance)+" button="+std::to_string(button));
    }
}
void training(Menu& menu) {
    auto id=[](std::string_view name) { return int(std::find(roster.begin(),roster.end(),name)-roster.begin()); };
    menu.load_movement_frames(); menu.load_cerebella();
    menu.practice_width=1700; menu.practice_height=900; menu.practice_floor=145; menu.practice_starts={760,878};
    for (auto name:{"peacock","filia","cerebella"}) { const int n=id(name); menu.palettes[n].push_back({0,skullgirls::CharacterAssets(menu.game_data,name).read_default_palette()}); }
    for (auto partner:{"peacock","filia","cerebella"}) {
        menu.fighters[0]={id("peacock")}; menu.fighters[1]={id(partner)};
        for(auto& colors:menu.locked_colors) colors={0};
        menu.prepare_practice_bitmaps(); menu.reset_practice();
        menu.start_practice_intro();
        check(menu.practice_presentation.phase == Menu::PracticePresentation::Phase::intro, "Peacock starts intro");
        for (unsigned i=0; i<600 && menu.practice_presentation.phase != Menu::PracticePresentation::Phase::none; ++i) menu.practice_tick();
        check(menu.practice_presentation.phase == Menu::PracticePresentation::Phase::none && menu.practice_players[0].peacock->move == 20, "Intro returns to idle");
        check(menu.practice_slot_playable(0,0),"Peacock is playable");
        Gdiplus::Bitmap canvas(640,480,PixelFormat32bppARGB); Gdiplus::Graphics dc(&canvas);
        dc.Clear(Gdiplus::Color::Transparent);
        menu.draw_practice_frame(dc,0,2,320,410,false,false);
        bool painted=false;
        for(int y=20;y<440;y+=12) for(int x=120;x<520;x+=12) { Gdiplus::Color pixel; canvas.GetPixel(x,y,&pixel); painted|=pixel.GetAlpha()!=0; }
        check(painted,"Peacock renders using her own palette and sprites");
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
        check(menu.hud_stats[0].blocked_hits && !menu.hud_stats[0].damage, "Peacock jab can be guarded");
        menu.training_options.pop_back();
        menu.reset_practice(); menu.practice_key('A',true); menu.practice_tick(); menu.practice_key('A',false);
        const unsigned saved=menu.practice_players[0].peacock->move;
        menu.save_or_restore_practice(false); menu.practice_tick(); menu.reset_practice(); menu.save_or_restore_practice(true);
        check(menu.practice_players[0].peacock && menu.practice_players[0].peacock->move==saved,"Practice state restores Peacock playback");
    }
}
void specials(Menu& menu, bool facing) {
    const auto tick = [&](Menu::PracticeFighter& f, unsigned dir, unsigned buttons=0) {
        constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};
        if (!facing) dir=mirror[dir];
        f.practice_left=dir>=3 && dir<=5; f.practice_right=dir==0 || dir==1 || dir==7;
        f.practice_down=dir>=1 && dir<=3; f.practice_up=dir>=5 && dir<=7;
        f.pressed_punches=buttons&7; f.pressed_kicks=buttons>>3; f.held_attack_buttons=buttons;
        menu.tick_peacock(f,facing?1500:100,0,1700,facing);
    };
    struct Command { std::initializer_list<unsigned> motion; unsigned buttons, move, cost; };
    for (const auto& c : {
        Command{{2,1,0},1,165,0}, {{2,1,0},2,166,0}, {{2,1,0},4,167,0},
        {{2,3,4},1,170,0}, {{2,3,4},2,171,0}, {{2,3,4},4,172,0},
        {{2,3,4},8,182,0}, {{2,3,4},16,184,0}, {{2,3,4},32,185,0},
        {{2,1,0},8,189,0}, {{2,1,0},16,193,0}, {{2,1,0},32,197,0},
        {{2,1,0},3,201,1}, {{2,1,0},24,207,1},
        {{2,1,0},18,219,1}, {{2,1,0},36,219,1}, {{8},9,147,0}}) {
        Menu::PracticeFighter f; f.practice_x=800; f.peacock.emplace().facing_right=facing;
        for(auto i=c.motion.begin();i!=c.motion.end();++i) tick(f,*i,i+1==c.motion.end()?c.buttons:0);
        check(f.peacock->move==c.move || (c.move==170 && f.peacock->move==178) ||
            (c.move==171 && f.peacock->move==178) || (c.move==172 && f.peacock->move==178) ||
            (c.move>=182 && c.move<=185 && f.peacock->move==186) ||
            (c.move==189 && f.peacock->move==190) || (c.move==193 && f.peacock->move==194) ||
            (c.move==197 && f.peacock->move==198),
            "Special command "+menu.peacock_assets.names[c.move]+" got="+menu.peacock_assets.names[f.peacock->move]);
        check(f.tension==5-c.cost,"Command meter cost");
        for(unsigned i=0;i<1000 && f.peacock->attacking();++i) tick(f,8);
        check(!f.peacock->attacking(),"Special recovers "+menu.peacock_assets.names[c.move]+" stuck="+std::to_string(f.peacock->move));
        if(c.cost) {
            Menu::PracticeFighter empty; empty.practice_x=800; empty.tension=0; empty.peacock.emplace().facing_right=facing;
            for(auto i=c.motion.begin();i!=c.motion.end();++i) tick(empty,*i,i+1==c.motion.end()?c.buttons:0);
            check(empty.tension==0 && empty.peacock->move!=c.move,"Super/Snapback requires meter");
        }
    }
    for (unsigned meter : {2u,3u,5u}) {
        Menu::PracticeFighter f; f.practice_x=800; f.tension=meter;
        auto& actor=f.peacock.emplace(); actor.facing_right=facing;
        constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};
        actor.sample(facing?2:mirror[2],0,0,true,meter);
        actor.sample(facing?3:mirror[3],0,0,true,meter);
        actor.sample(facing?4:mirror[4],3,3,true,meter);
        actor.start(149);
        tick(f,8);
        check((actor.move==208)==(meter>=3),"Goodfellas throw cancel requires three bars");
        check(f.tension==(meter>=3?meter-3:meter),"Goodfellas spends meter once");
        for(unsigned i=0;i<1000 && actor.attacking();++i)tick(f,8);
        check(!actor.attacking(),"Goodfellas/throw recovery completes");
    }
    for (unsigned move : {182u,184u,185u}) {
        Menu::PracticeFighter f; f.practice_x=800; auto& actor=f.peacock.emplace(); actor.facing_right=facing;
        actor.start(move);
        for(unsigned i=0;i<150;++i)tick(f,8);
        const float delta=(f.practice_x-800)*(facing?1:-1);
        check(move==182 ? delta < -300 : delta > 300,"Teleport uses strength-specific travel "+std::to_string(move)+" delta="+std::to_string(delta));
    }
}
void movement_and_defense(Menu& menu) {
    for (const bool facing : {false, true}) {
        Menu::PracticeFighter fighter;
        fighter.practice_x = 800;
        auto& actor = fighter.peacock.emplace(); actor.facing_right = facing;
        fighter.practice_right = facing; fighter.practice_left = !facing;
        for (unsigned i=0; i<20; ++i) menu.tick_peacock(fighter, facing ? 1500 : 100, 0, 1700, facing);
        check(facing ? fighter.practice_x > 800 : fighter.practice_x < 800, "Walk moves in facing direction");
        fighter.practice_left = fighter.practice_right = false;
        fighter.practice_up = true;
        bool rose = false;
        for (unsigned i=0; i<120; ++i) {
            menu.tick_peacock(fighter, facing ? 1500 : 100, 0, 1700, facing);
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
        check(fighter.peacock->move==(stance==2?417u:stance==1?411u:402u), "Pushblock selects Peacock stance");
        for (unsigned i=0;i<100;++i) menu.tick_peacock(fighter,1500,0,1700,true);
        check(!fighter.blockstun && fighter.guard_phase==Menu::PracticeFighter::GuardPhase::none,"Pushblock recovers");
    }
    for (bool air : {false,true}) {
        menu.reset_practice(); auto& fighter=menu.practice_players[0];
        fighter.hurt_group=skullgirls::air_mid; fighter.burst_available=true;
        fighter.jump_height=air?450.0f:0.0f; fighter.pressed_punches=1;
        menu.advance_training_bursts();
        check(fighter.peacock->move==429,"Burst uses Peacock move");
        for(unsigned i=0;i<300;++i) menu.tick_peacock(fighter,1500,0,1700,true);
        check(!fighter.burst_ticks && !fighter.burst_fall_tick && fighter.jump_height==0 && fighter.peacock->move==20,"Burst lands and returns to idle air="+std::to_string(air)+" move="+std::to_string(fighter.peacock->move)+" height="+std::to_string(fighter.jump_height)+" burst="+std::to_string(fighter.burst_ticks)+" fall="+std::to_string(fighter.burst_fall_tick));
    }
}
void combat(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("peacock")}; menu.fighters[1]={id("filia")};
    menu.locked_colors[0]=menu.locked_colors[1]={0};
    for(bool facing : {false,true}) for(unsigned move : {125u,165u,166u,167u,170u,171u,172u,189u,193u,197u,201u,207u,147u,157u}) {
        menu.reset_practice();
        auto& f=menu.practice_players[0]; auto& target=menu.practice_players[1];
        f.practice_x=facing?760.0f:940.0f; target.practice_x=facing?878.0f:822.0f;
        f.peacock->facing_right=facing;
        f.peacock->start(move); ++f.attack_instance;
        const bool throwing=move==147 || move==157;
        if (move==170) target.practice_x=f.practice_x+(facing?290:-290);
        // Heavy Shadow tracks beyond its authored minimum distance of 775.
        if (move==172) { f.practice_x=facing?400.0f:1300.0f;target.practice_x=facing?1300.0f:400.0f; }
        if (move==157) { f.jump_height=target.jump_height=450; f.peacock->state.variables[0x3b]=2.0f; }
        bool captured=false;
        unsigned maximum=0;
        for(unsigned i=0;i<1000;++i) {
            menu.practice_tick();
            maximum=std::max(maximum,menu.hud_stats[0].damage);
            captured|=target.throw_phase==Menu::PracticeFighter::ThrowPhase::grabbed;
        }
        check(maximum>0,"Combat damage "+menu.peacock_assets.names[move]+" captured="+std::to_string(captured));
        if(throwing)check(captured && target.throw_phase!=Menu::PracticeFighter::ThrowPhase::grabbed,"Throw releases victim");
        check(!f.peacock->attacking(),"Combat move recovers "+menu.peacock_assets.names[move]);
    }
    for (unsigned hold : {0u,100u,260u}) {
        menu.reset_practice();
        auto& f=menu.practice_players[0];
        menu.practice_players[1].practice_x=f.practice_x+290;
        f.peacock->start(170); ++f.attack_instance;
        if(hold)menu.practice_key('A',true);
        unsigned damage=0;
        for(unsigned i=0;i<700;++i) {
            if(i==hold)menu.practice_key('A',false);
            menu.practice_tick(); damage=std::max(damage,menu.hud_stats[0].damage);
        }
        check(damage>= (hold>=260?1500u:hold>=100?1000u:850u),"Item charge increases damage hold="+std::to_string(hold)+" damage="+std::to_string(damage));
    }
    menu.reset_practice();
    auto& f=menu.practice_players[0]; f.peacock->start(193); ++f.attack_instance;
    for(unsigned i=0;i<25;++i)menu.practice_tick();
    check(!menu.practice_effects.empty(),"Projectile is independently active");
    menu.save_or_restore_practice(false);
    const auto effects=menu.practice_effects;
    menu.practice_effects.clear();menu.save_or_restore_practice(true);
    check(menu.practice_effects.size()==effects.size() && menu.practice_effects.front().x==effects.front().x &&
        menu.practice_effects.front().hit==effects.front().hit,"Save restores projectile position and combat descriptor");
    for(auto& e:menu.practice_effects) {
        if(e.root!=&menu.peacock_assets || !e.hit)continue;
        const auto x=e.x; const auto record=e.record; const auto ticks=e.ticks;
        e.advance(menu.effect_parent_state(f),true,true);
        check(e.x==x && e.record==record && e.ticks==ticks,"Super freeze pauses combat children");
    }
    menu.reset_practice();
    menu.training_options.push_back({L"Block",{},2,"OPT_STR_BLOCKING"});
    menu.practice_players[0].peacock->start(193);
    for(unsigned i=0;i<250;++i)menu.practice_tick();
    check(menu.hud_stats[0].blocked_hits && !menu.hud_stats[0].damage,"Combat projectile can be guarded");
    menu.training_options.pop_back();
    for(bool air : {false,true}) {
        menu.reset_practice();
        menu.training_options.push_back({L"Throw tech",{},1,"OPT_STR_THROWTECH"});
        auto& f=menu.practice_players[0]; f.peacock->start(air?157:147);
        if(air) {f.jump_height=menu.practice_players[1].jump_height=450;f.peacock->state.variables[0x3b]=2.0f;}
        bool teched=false;
        for(unsigned i=0;i<300;++i) {menu.practice_tick();teched|=f.throw_phase==Menu::PracticeFighter::ThrowPhase::tech;}
        check(teched && !menu.hud_stats[0].damage,"Offensive throw can be teched");
        menu.training_options.pop_back();
    }
}
void teams(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("filia"),id("peacock"),id("cerebella")};
    menu.fighters[1]={id("peacock")};
    menu.locked_colors[0]={0,0,0}; menu.locked_colors[1]={0};
    menu.reset_practice();
    for(unsigned ref:{1012u,1013u,1014u,119u,134u,135u,150u,153u,176u,180u,113u}) {
        auto pose=menu.peacock_team_pose(ref);
        check(pose>1 && menu.peacock_frames.at(pose).width,"Peacock team travel visible");
    }
    menu.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
    for(unsigned i=0;i<300 && menu.practice_teams[0].tag;++i) menu.practice_tick();
    check(menu.practice_teams[0].active_slot==1 && menu.practice_players[0].peacock,"Tag activates Peacock playback");
    menu.begin_practice_tag(0,2,Menu::PracticeTag::Kind::regular);
    for(unsigned i=0;i<300 && menu.practice_teams[0].tag;++i) menu.practice_tick();
    check(menu.practice_teams[0].active_slot==2 && menu.practice_players[0].cerebella,"Peacock tags out into Cerebella");
    menu.reset_practice();
    menu.practice_key('A',true); menu.practice_key('X',true); menu.practice_tick();
    menu.practice_key('A',false); menu.practice_key('X',false);
    auto& team=menu.practice_teams[0];
    check(team.assist && team.assist->fighter.peacock,"Peacock assist has own playback");
    bool acted=false;
    for(unsigned i=0;i<400 && team.assist;++i) { menu.practice_tick(); acted |= team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting; }
    check(acted && !team.assist,"Peacock assist acts and exits");
}
void throw_victim(Menu& menu) {
    auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("filia")}; menu.fighters[1]={id("peacock")};
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
            check(defender.sprite() < menu.peacock_frames.size(), "Throw victim uses Peacock sprite references");
        }
        check(tech ? teched && !menu.hud_stats[0].damage : captured, "Peacock throw victim/tech outcome tech="+std::to_string(tech)+" captured="+std::to_string(captured)+" teched="+std::to_string(teched)+" damage="+std::to_string(menu.hud_stats[0].damage));
        check(menu.practice_players[1].throw_phase==Menu::PracticeFighter::ThrowPhase::none, "Peacock throw victim recovers");
        menu.training_options.pop_back();
    }
}
void projectile_regressions(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("peacock")};menu.fighters[1]={id("filia")};menu.locked_colors[0]=menu.locked_colors[1]={0};
    const auto visible = [&](const skullgirls::FoitEffectPlayback& effect) {
        return effect.alive && effect.pose>1 && effect.pose<menu.peacock_frames.size() &&
            menu.peacock_frames[effect.pose].width && effect.value(0xa4)>0 && effect.value(0xa5)>0 && effect.value(0xa6)>0;
    };
    const auto rendered = [&](const skullgirls::FoitEffectPlayback& effect) {
        Gdiplus::Bitmap canvas(1024,1024,PixelFormat32bppARGB);Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color::Transparent);
        dc.TranslateTransform(512,512);dc.ScaleTransform(effect.value(0xa4),effect.value(0xa5));
        menu.draw_practice_frame(dc,0,effect.pose,0,0,!effect.facing_right,false,{},effect.value(0xa6));
        for(int y=0;y<1024;y+=8)for(int x=0;x<1024;x+=8){Gdiplus::Color pixel;canvas.GetPixel(x,y,&pixel);if(pixel.GetAlpha())return true;}
        return false;
    };
    for(bool facing:{false,true})for(const auto& spec : {std::pair{167u,22u},{189u,28u},{193u,29u},{197u,30u},{170u,33u},{171u,33u},{172u,33u},{201u,48u},{207u,52u}}) {
        menu.reset_practice();auto& f=menu.practice_players[0];f.practice_x=facing?300.0f:1400.0f;
        menu.practice_players[1].practice_x=facing?1550.0f:150.0f;
        f.peacock->facing_right=facing;f.peacock->start(spec.first);++f.attack_instance;
        bool painted=false;unsigned first=0;
        for(unsigned t=0;t<200 && !painted;++t){menu.practice_tick();for(const auto& effect:menu.practice_effects)
            if(effect.root==&menu.peacock_assets && effect.table==&menu.peacock_assets.children[spec.second] && visible(effect)){
                painted=rendered(effect);first=t;break;
            }}
        check(painted,"Visible projectile artwork "+menu.peacock_assets.names[spec.first]+" facing="+std::to_string(facing));
        check(first>0 && first<100,"Projectile respects the move's startup");
    }

    for(bool facing:{false,true}) {
        menu.reset_practice();auto& f=menu.practice_players[0];f.practice_x=facing?300.0f:1400.0f;
        menu.practice_players[1].practice_x=facing?1600.0f:100.0f;f.peacock->facing_right=facing;f.peacock->start(193);++f.attack_instance;
        std::uint64_t kart=0;float start=0;unsigned spawn=0;bool travelled=false;
        for(unsigned t=0;t<180 && !travelled;++t){menu.practice_tick();for(const auto& e:menu.practice_effects)if(e.table==&menu.peacock_assets.children[29] && e.move==1){
            if(!kart){kart=e.entity_id;start=e.x;spawn=t;}
            if(e.entity_id==kart && t==spawn+10){check(std::abs((e.x-start)*(facing?1:-1)-190)<0.1f,"George's kart travels at its original 19 units/tick");
                check(e.y==0 && std::abs(e.value(0x9f))<2,"Grounded kart does not accumulate falling velocity");travelled=true;}
        }}
        check(travelled,"George kart reaches sustained flight");
        for(unsigned t=0;t<400;++t)menu.practice_tick();
        check(std::none_of(menu.practice_effects.begin(),menu.practice_effects.end(),[&](const auto&e){return e.entity_id==kart;}),"George despawns after contact or leaving the stage");
        check(f.peacock->state.bits[26].has_value() && !*f.peacock->state.bits[26],"George releases its owner presence flag");
    }

}

}
int wmain(int argc,wchar_t** argv) {
    if(argc!=2) return 2;
    Gdiplus::GdiplusStartupInput input; ULONG_PTR graphics=0;
    if(Gdiplus::GdiplusStartup(&graphics,&input,nullptr)!=Gdiplus::Ok) return 2;
    int result=0;
    try { Menu menu; menu.game_data=argv[1]; menu.load_peacock();
        check(menu.peacock_assets.animations.size()==548 && menu.peacock_frames[2].width,"Peacock assets load");
        commands(menu,true); commands(menu,false); specials(menu,true); specials(menu,false); training(menu); movement_and_defense(menu); teams(menu); throw_victim(menu); combat(menu); projectile_regressions(menu);
        std::cout<<"Peacock moves, projectiles, throws, and Training integration passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; result=1; }
    Gdiplus::GdiplusShutdown(graphics); return result;
}
