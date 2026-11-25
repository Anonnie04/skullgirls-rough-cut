#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>

namespace {
void check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }

void load_with_retry(Menu& menu) {
    menu.load_scripted_character("msfortune", menu.msfortune_assets, menu.msfortune_frames, menu.msfortune_overlays);
    const auto directory = menu.game_data;
    menu.game_data = directory / "characters-art.gfs";
    bool rejected = false;
    try {
        menu.load_msfortune();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    menu.game_data = directory;
    check(rejected, "Headless asset loading reports an unavailable archive");

    menu.load_msfortune();
    bool loaded_headless_frame = false;
    for (std::size_t ref = 2; ref < menu.msfortune_headless_frames.size(); ++ref) {
        const auto& layer = menu.msfortune_assets.sprites[ref - 1].layers[4];
        if (layer && layer->used_tiles && menu.msfortune_headless_frames[ref].pixels != menu.msfortune_frames[ref].pixels) {
            loaded_headless_frame = true;
            break;
        }
    }
    check(loaded_headless_frame, "Retry loads headless frames after a failed archive read");
}

void commands(Menu& menu, bool facing) {
    constexpr unsigned normals[][6] = {{107,112,114,109,113,118}, {119,122,124,121,123,126}, {127,129,131,128,130,132}};
    for (unsigned stance=0; stance<3; ++stance) for (unsigned button=0; button<6; ++button) {
        Menu::PracticeFighter fighter;
        auto& actor=fighter.msfortune.emplace(); actor.facing_right=facing;
        fighter.practice_x=800;
        if (stance==2) { actor.start(88); actor.state.variables[0x3b]=2.0f; fighter.jump_height=450; }
        const auto tick=[&](unsigned buttons) {
            fighter.practice_down=stance==1;
            fighter.pressed_punches=buttons&7u; fighter.pressed_kicks=buttons>>3;
            fighter.held_attack_buttons=buttons;
            menu.tick_msfortune(fighter,facing?1500.0f:100.0f,0,1700,facing);
        };
        tick(1u<<button);
        check(actor.move==normals[stance][button], "Normal entry stance="+std::to_string(stance)+" button="+std::to_string(button)+" got="+std::to_string(actor.move));
        bool hit=actor.hit!=nullptr;
        for (unsigned i=0;i<240 && actor.attacking();++i) { tick(0); hit|=actor.hit!=nullptr; }
        check(!actor.attacking(), "Normal completes recovery");
        // Check body hit data for ground normals; spawned child combat is a later milestone.
        if (stance!=2 && !(stance==0 && button==1)) check(hit,"Normal exposes original hit descriptor stance="+std::to_string(stance)+" button="+std::to_string(button));
    }
}
void training(Menu& menu) {
    auto id=[](std::string_view name) { return int(std::find(roster.begin(),roster.end(),name)-roster.begin()); };
    menu.load_movement_frames(); menu.load_cerebella(); menu.load_peacock();menu.load_parasoul();
    menu.practice_width=1700; menu.practice_height=900; menu.practice_floor=145; menu.practice_starts={760,878};
    for (auto name:{"msfortune","filia","cerebella","peacock","parasoul"}) { const int n=id(name); menu.palettes[n].push_back({0,skullgirls::CharacterAssets(menu.game_data,name).read_default_palette()}); }
    for (auto partner:{"msfortune","filia","cerebella","peacock","parasoul"}) {
        menu.fighters[0]={id("msfortune")}; menu.fighters[1]={id(partner)};
        for(auto& colors:menu.locked_colors) colors={0};
        menu.prepare_practice_bitmaps(); menu.reset_practice();
        menu.start_practice_intro();
        check(menu.practice_presentation.phase == Menu::PracticePresentation::Phase::intro, "Ms. Fortune starts intro");
        for (unsigned i=0; i<600 && menu.practice_presentation.phase != Menu::PracticePresentation::Phase::none; ++i) menu.practice_tick();
        check(menu.practice_presentation.phase == Menu::PracticePresentation::Phase::none && menu.practice_players[0].msfortune->move == 21, "Intro returns to idle");
        check(menu.practice_slot_playable(0,0),"Ms. Fortune is playable");
        Gdiplus::Bitmap canvas(640,480,PixelFormat32bppARGB); Gdiplus::Graphics dc(&canvas);
        dc.Clear(Gdiplus::Color::Transparent);
        menu.draw_practice_frame(dc,0,2,320,410,false,false);
        bool painted=false;
        for(int y=20;y<440;y+=12) for(int x=120;x<520;x+=12) { Gdiplus::Color pixel; canvas.GetPixel(x,y,&pixel); painted|=pixel.GetAlpha()!=0; }
        check(painted,"Ms. Fortune renders using her own palette and sprites");
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
        check(menu.hud_stats[0].blocked_hits && !menu.hud_stats[0].damage, "Ms. Fortune jab can be guarded");
        menu.training_options.pop_back();
        menu.reset_practice(); menu.practice_key('A',true); menu.practice_tick(); menu.practice_key('A',false);
        const unsigned saved=menu.practice_players[0].msfortune->move;
        menu.save_or_restore_practice(false); menu.practice_tick(); menu.reset_practice(); menu.save_or_restore_practice(true);
        check(menu.practice_players[0].msfortune && menu.practice_players[0].msfortune->move==saved,"Practice state restores Ms. Fortune playback");
    }
}
void movement_and_defense(Menu& menu) {
    for (const bool facing : {false, true}) {
        Menu::PracticeFighter fighter;
        fighter.practice_x = 800;
        auto& actor = fighter.msfortune.emplace(); actor.facing_right = facing;
        fighter.practice_right = facing; fighter.practice_left = !facing;
        for (unsigned i=0; i<20; ++i) menu.tick_msfortune(fighter, facing ? 1500 : 100, 0, 1700, facing);
        check(facing ? fighter.practice_x > 800 : fighter.practice_x < 800, "Walk moves in facing direction");
        fighter.practice_left = fighter.practice_right = false;
        fighter.practice_up = true;
        bool rose = false;
        for (unsigned i=0; i<120; ++i) {
            menu.tick_msfortune(fighter, facing ? 1500 : 100, 0, 1700, facing);
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
        check(fighter.msfortune->move==(stance==2?416u:stance==1?410u:401u), "Pushblock selects Ms. Fortune stance");
        for (unsigned i=0;i<100;++i) menu.tick_msfortune(fighter,1500,0,1700,true);
        check(!fighter.blockstun && fighter.guard_phase==Menu::PracticeFighter::GuardPhase::none,"Pushblock recovers");
    }
    for (bool air : {false,true}) {
        menu.reset_practice(); auto& fighter=menu.practice_players[0];
        fighter.hurt_group=skullgirls::air_mid; fighter.burst_available=true;
        fighter.jump_height=air?450.0f:0.0f; fighter.pressed_punches=1;
        menu.advance_training_bursts();
        check(fighter.msfortune->move==428,"Burst uses Ms. Fortune move");
        for(unsigned i=0;i<300;++i) menu.tick_msfortune(fighter,1500,0,1700,true);
        check(!fighter.burst_ticks && !fighter.burst_fall_tick && fighter.jump_height==0 && fighter.msfortune->move==21,"Burst lands and returns to idle air="+std::to_string(air)+" move="+std::to_string(fighter.msfortune->move)+" height="+std::to_string(fighter.jump_height)+" burst="+std::to_string(fighter.burst_ticks)+" fall="+std::to_string(fighter.burst_fall_tick));
    }
}
void specials(Menu& menu, bool facing) {
    const auto ready=[&](bool air=false) {
        Menu::PracticeFighter f; f.practice_x=800;f.msfortune.emplace().facing_right=facing;
        if(air){f.jump_height=450;f.msfortune->start(88);f.msfortune->state.variables[0x3b]=2.0f;}
        return f;
    };
    const auto tick=[&](Menu::PracticeFighter& f,unsigned dir,unsigned buttons=0) {
        constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
        f.practice_left=dir>=3 && dir<=5;f.practice_right=dir==0 || dir==1 || dir==7;
        f.practice_down=dir>=1 && dir<=3;f.practice_up=dir>=5 && dir<=7;
        f.pressed_punches=buttons&7;f.pressed_kicks=buttons>>3;f.held_attack_buttons=buttons;
        menu.tick_msfortune(f,facing?1500:100,0,1700,facing);
    };
    const auto finishes=[&](Menu::PracticeFighter& f,unsigned entry) {
        for(unsigned i=0;i<700 && f.msfortune->attacking();++i)tick(f,8);
        check(!f.msfortune->attacking(),"Recovery "+menu.msfortune_assets.names[entry]+" stuck="+std::to_string(f.msfortune->move));
    };
    struct Command { std::initializer_list<unsigned> motion;unsigned buttons,move,cost;bool air=false; };
    for(const auto& c : {Command{{2,1,0},1,182,0},{{2,1,0},2,183,0},{{2,1,0},4,184,0},
        {{0,2,1},8,171,0},{{0,2,1},16,173,0},{{0,2,1},32,175,0},
        {{2,3,4},4,164,0},{{2,3,4},4,167,0,true},
        {{2,3,4},8,168,0,true},{{2,3,4},16,169,0,true},{{2,3,4},32,170,0,true},
        {{2,1,0},3,205,1},{{2,3,4},24,201,1,true},{{2,3,4},3,209,3},
        {{2,1,0},18,218,1},{{2,1,0},36,218,1},{{8},9,139,0},{{8},9,148,0,true}}) {
        auto f=ready(c.air);
        for(auto i=c.motion.begin();i!=c.motion.end();++i)tick(f,*i,i+1==c.motion.end()?c.buttons:0);
        check(f.msfortune->move==c.move,"Command "+menu.msfortune_assets.names[c.move]+" got="+menu.msfortune_assets.names[f.msfortune->move]);
        check(f.tension==5-c.cost,"Command meter cost");if(c.move!=209)finishes(f,c.move);
        if(c.cost){auto empty=ready(c.air);empty.tension=c.cost-1;for(auto i=c.motion.begin();i!=c.motion.end();++i)tick(empty,*i,i+1==c.motion.end()?c.buttons:0);check(empty.msfortune->move!=c.move && empty.tension==c.cost-1,"Insufficient meter rejects command");}
    }
}
void teams(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("filia"),id("msfortune"),id("cerebella")};
    menu.fighters[1]={id("msfortune")};
    menu.locked_colors[0]={0,0,0}; menu.locked_colors[1]={0};
    menu.reset_practice();
    for(unsigned ref:{1012u,1013u,1014u,119u,134u,135u,150u,153u,176u,180u,113u}) {
        auto pose=menu.msfortune_team_pose(ref);
        check(pose>1 && menu.msfortune_frames.at(pose).width,"Ms. Fortune team travel visible");
    }
    menu.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
    for(unsigned i=0;i<300 && menu.practice_teams[0].tag;++i) menu.practice_tick();
    check(menu.practice_teams[0].active_slot==1 && menu.practice_players[0].msfortune,"Tag activates Ms. Fortune playback");
    menu.begin_practice_tag(0,2,Menu::PracticeTag::Kind::regular);
    for(unsigned i=0;i<300 && menu.practice_teams[0].tag;++i) menu.practice_tick();
    check(menu.practice_teams[0].active_slot==2 && menu.practice_players[0].cerebella,"Ms. Fortune tags out into Cerebella");
    menu.reset_practice();
    menu.practice_key('A',true); menu.practice_key('X',true); menu.practice_tick();
    menu.practice_key('A',false); menu.practice_key('X',false);
    auto& team=menu.practice_teams[0];
    check(team.assist && team.assist->fighter.msfortune,"Ms. Fortune assist has own playback");
    bool acted=false;
    for(unsigned i=0;i<400 && team.assist;++i) { menu.practice_tick(); acted |= team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting; }
    check(acted && !team.assist,"Ms. Fortune assist acts and exits");
}

void head_lifecycle(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0] = {id("msfortune")};
    menu.fighters[1] = {id("filia")};
    menu.locked_colors[0] = menu.locked_colors[1] = {0};
    const auto head = [&]() -> skullgirls::FoitEffectPlayback& {
        const auto belongs_to_fortune = [](const auto& effect) { return effect.persistent_owner && effect.side == 0; };
        check(std::count_if(menu.practice_effects.begin(), menu.practice_effects.end(), belongs_to_fortune) == 1,
            "Fortune has exactly one head entity");
        return *std::find_if(menu.practice_effects.begin(), menu.practice_effects.end(), belongs_to_fortune);
    };

    for (bool facing : {true, false}) {
        menu.reset_practice();
        auto& fighter = menu.practice_players[0];
        fighter.practice_x = facing ? 500.0f : 1200.0f;
        menu.practice_players[1].practice_x = facing ? 1250.0f : 450.0f;
        menu.practice_tick();
        check(head().move == 0, "Reset creates a single attached head");

        auto& actor = *fighter.msfortune;
        actor.facing_right = facing;
        actor.start(167);
        actor.state.variables[0x3b] = 2.0f;
        fighter.jump_height = 450;
        bool kicked = false, landed = false;
        for (unsigned i = 0; i < 190; ++i) {
            menu.practice_tick();
            kicked |= head().move == 5 && head().hit && head().value(0x9f) < 0;
            landed |= head().move == 23;
            check(head().x >= 0 && head().x <= menu.practice_width && head().y >= 0, "Head Spike stays inside the playfield");
        }
        check(kicked && landed && actor.value(0x8b) != 0 && head().y == 0,
            "Head Spike gives the detached head a downward hit and lands it");

        const WPARAM back = facing ? VK_LEFT : VK_RIGHT;
        const auto call = [&] {
            menu.practice_key(VK_DOWN, true);
            menu.practice_tick();
            menu.practice_key(back, true);
            menu.practice_tick();
            menu.practice_key(VK_DOWN, false);
            menu.practice_key('A', true);
            menu.practice_tick();
            menu.practice_key(back, false);
        };
        head().start(16);
        head().x = facing ? 1100.0f : 600.0f;
        head().state.variables[0x9e] = 0.0f;
        call();
        check(actor.move == 160 || actor.move == 161, "Quarter-circle back and LP calls a detached head");
        for (unsigned i = 0; i < 32 && head().move != 30; ++i) menu.practice_tick();
        check(head().move == 30, "The head runs toward Fortune during Cat Call, body=" + std::to_string(actor.move) +
            " head=" + std::to_string(head().move) + " y=" + std::to_string(head().y));
        const float frozen_x = head().x;
        const unsigned frozen_record = head().record;
        menu.advance_practice_effects(true);
        check(head().x == frozen_x && head().record == frozen_record, "Super-freeze pauses the returning head");

        menu.practice_key('A', false);
        for (unsigned i = 0; i < 60; ++i) menu.practice_tick();
        check(!actor.attacking() && actor.value(0x8b) != 0 && head().move != 30,
            "Releasing Cat Call stops the head from attaching");

        call();
        for (unsigned i = 0; i < 200 && actor.value(0x8b) != 0; ++i) menu.practice_tick();
        menu.practice_key('A', false);
        for (unsigned i = 0; i < 40; ++i) menu.practice_tick();
        check(head().move == 0 && actor.value(0x8b) == 0 && !actor.attacking(),
            "Holding Cat Call reattaches the head and restores headed idle");

        actor.start(164);
        for (unsigned i = 0; i < 180; ++i) menu.practice_tick();
        auto& rival = menu.practice_players[1];
        head().start(16);
        head().x = rival.practice_x + (facing ? -120.0f : 120.0f);
        head().state.variables[0x9e] = 0.0f;
        menu.practice_tick();
        menu.practice_key('K', true);
        menu.practice_key('F', true);
        menu.practice_tick();
        menu.practice_key('F', false);
        bool struck = false;
        for (unsigned i = 0; i < 75; ++i) {
            menu.practice_tick();
            struck |= head().move >= 61 && head().move <= 74;
            check(fighter.hurt_group < 0, "Striking the head leaves Fortune's distant body out of hitstun");
        }
        menu.practice_key('K', false);
        check(struck && menu.hud_stats[1].hits == 1 && menu.hud_stats[1].damage > 0,
            "Filia's low kick hits the detached head once and transfers damage");
        check(head().move == 16 || head().move == 17, "The head recovers after an enemy hit");

        head().start(22);
        head().x = fighter.practice_x + (facing ? 75.0f : -75.0f);
        head().y = 250.0f;
        head().state.variables[0x3b] = 2.0f;
        head().state.variables[0x9f] = 0.0f;
        call();
        for (unsigned i = 0; i < 200 && actor.value(0x8b) != 0; ++i) {
            menu.practice_tick();
            if (head().y > 0) check(actor.value(0x8b) != 0, "Cat Call waits for the airborne head to land");
        }
        menu.practice_key('A', false);
        check(actor.value(0x8b) == 0, "Cat Call reattaches the head after landing");
    }

    menu.fighters[0] = {id("msfortune"), id("filia")};
    menu.locked_colors[0] = {0, 0};
    menu.reset_practice();
    menu.practice_tick();
    head().captured_target = true;
    auto& victim = menu.practice_players[1];
    victim.throw_phase = Menu::PracticeFighter::ThrowPhase::grabbed;
    menu.tag_practice_fighter(0, 1, Menu::PracticeTag::Kind::regular);
    menu.advance_practice_effects(false);
    check(victim.throw_phase != Menu::PracticeFighter::ThrowPhase::grabbed &&
        std::none_of(menu.practice_effects.begin(), menu.practice_effects.end(), [](const auto& effect) {
            return effect.persistent_owner && effect.side == 0;
        }), "A departing head releases its held victim");
}

void head_and_combat(Menu& menu) {
    const auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("msfortune")};menu.fighters[1]={id("filia")};menu.locked_colors[0]=menu.locked_colors[1]={0};
    menu.reset_practice();auto& f=menu.practice_players[0];f.msfortune->start(164);++f.attack_instance;
    bool detached=false;
    for(unsigned i=0;i<180;++i){menu.practice_tick();detached|=f.msfortune->value(0x8b)!=0;}
    check(detached,"Head Roll detaches the head");
    auto head=std::find_if(menu.practice_effects.begin(),menu.practice_effects.end(),[&](const auto&e){return e.root==&menu.msfortune_assets && e.table==&menu.msfortune_assets.children[0] && e.side==0;});
    check(head!=menu.practice_effects.end() && head->alive,"Detached head persists after body recovery");
    check(head->x>=0 && head->x<=1700 && head->y==0,"Head settles inside the stage");
    f.practice_x=300;menu.practice_players[1].practice_x=1000;
    head->start(16);head->x=900;head->y=0;head->state.variables[0xa0]=900.0f;head->state.variables[0xa1]=0.0f;
    head->state.variables[0x9e]=head->state.variables[0x9f]=0.0f;
    head->state.variables[0x38]=1.0f;head->facing_right=true;
    menu.hud_stats[0].hits=menu.hud_stats[0].damage=0;
    menu.practice_key('D',true);
    bool attacked=false;unsigned damage=0;
    for(unsigned i=0;i<150;++i){if(i==20)menu.practice_key('D',false);menu.practice_tick();damage=std::max(damage,menu.hud_stats[0].damage);for(const auto&e:menu.practice_effects)if(e.persistent_owner){attacked|=e.move==40;}}
    check(attacked && damage>0,"Head HP attacked="+std::to_string(attacked)+" damage="+std::to_string(damage));
    menu.save_or_restore_practice(false);const auto count=menu.practice_effects.size();
    menu.practice_effects.clear();menu.save_or_restore_practice(true);
    check(f.msfortune->value(0x8b)!=0 && menu.practice_effects.size()==count,"Practice save restores detached head and body mode");
    f.msfortune->start(160);
    for(unsigned i=0;i<240;++i)menu.practice_tick();
    check(f.msfortune->value(0x8b)==0 && !f.msfortune->attacking(),"Recall headless="+std::to_string(f.msfortune->value(0x8b))+" move="+std::to_string(f.msfortune->move));
    for(bool facing : {false,true})for(unsigned move : {107u,112u,114u,118u,124u,164u,168u,169u,170u,171u,173u,175u,182u,183u,184u,194u,197u,201u,205u,209u,139u,148u}) {
        menu.reset_practice();auto& actor=menu.practice_players[0];auto& target=menu.practice_players[1];
        actor.practice_x=facing?760.0f:940.0f;target.practice_x=facing?878.0f:822.0f;
        actor.msfortune->facing_right=facing;actor.msfortune->start(move);++actor.attack_instance;
        if(move==148 || (move>=168 && move<=170) || move==201){actor.jump_height=target.jump_height=450;actor.msfortune->state.variables[0x3b]=2.0f;}
        if(move==170){actor.jump_height=200;target.jump_height=0;}
        unsigned dealt=0;bool captured=false;
        for(unsigned i=0;i<1000;++i){menu.practice_tick();dealt=std::max(dealt,menu.hud_stats[0].damage);captured|=target.throw_phase==Menu::PracticeFighter::ThrowPhase::grabbed;}
        check(dealt>0,"Combat "+menu.msfortune_assets.names[move]+" facing="+std::to_string(facing)+" got="+std::to_string(actor.msfortune->move));
        if(move==139 || move==148)check(captured && target.throw_phase!=Menu::PracticeFighter::ThrowPhase::grabbed,"Throw captures and releases");
        check(!actor.msfortune->attacking(),"Combat recovers "+menu.msfortune_assets.names[move]+" stuck="+std::to_string(actor.msfortune->move));
    }

}
void throw_victim(Menu& menu) {
    auto id=[](std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());};
    menu.fighters[0]={id("filia")}; menu.fighters[1]={id("msfortune")};
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
            check(defender.sprite() < menu.msfortune_frames.size(), "Throw victim uses Ms. Fortune sprite references");
        }
        check(tech ? teched && !menu.hud_stats[0].damage : captured, "Ms. Fortune throw victim/tech outcome tech="+std::to_string(tech)+" captured="+std::to_string(captured)+" teched="+std::to_string(teched)+" damage="+std::to_string(menu.hud_stats[0].damage));
        check(menu.practice_players[1].throw_phase==Menu::PracticeFighter::ThrowPhase::none, "Ms. Fortune throw victim recovers");
        menu.training_options.pop_back();
    }
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
    try { Menu menu; menu.game_data=argv[1]; load_with_retry(menu);
        check(menu.msfortune_assets.animations.size()==545 && menu.msfortune_frames[2].width,"Ms. Fortune assets load");
        commands(menu,true); commands(menu,false); specials(menu,true);specials(menu,false);training(menu); movement_and_defense(menu); teams(menu); throw_victim(menu);head_lifecycle(menu);head_and_combat(menu);
        std::cout<<"Ms. Fortune moves, projectiles, throws, and Training integration passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; result=1; }
    Gdiplus::GdiplusShutdown(graphics); return result;
}
