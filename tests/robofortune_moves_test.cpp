#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>

namespace {
void check(bool ok,const std::string& text) {if(!ok)throw std::runtime_error(text);}
int character(std::string_view name) {return int(std::find(roster.begin(),roster.end(),name)-roster.begin());}
void tick(Menu& menu,Menu::PracticeFighter& fighter,bool facing,unsigned direction=8,unsigned buttons=0) {
    constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};
    if(!facing)direction=mirror[direction];
    fighter.practice_left=direction>=3 && direction<=5;
    fighter.practice_right=direction==0 || direction==1 || direction==7;
    fighter.practice_down=direction>=1 && direction<=3;
    fighter.practice_up=direction>=5 && direction<=7;
    const auto edges=buttons&~fighter.held_attack_buttons;
    fighter.pressed_punches=edges&7;fighter.pressed_kicks=edges>>3;fighter.held_attack_buttons=buttons;
    menu.tick_robofortune(fighter,facing?1500:100,0,1700,facing);
}
void commands(Menu& menu,bool facing) {
    struct Command {std::initializer_list<unsigned> motion;unsigned buttons,entry,cost=0;bool air=false;};
    for(const auto& command:{Command{{2,1,0},1,162},{{2,1,0},2,163},{{2,1,0},4,164},
        {{2,1,0},1,165,0,true},{{2,1,0},2,166,0,true},{{2,1,0},4,167,0,true},
        {{2,1,0},8,176},{{2,1,0},16,177},{{2,1,0},32,179},
        {{2,3,4},8,175},{{2,3,4},16,175},{{2,3,4},32,175},
        {{2,1,0},3,facing?181u:182u,1},{{2,1,0},24,189,1},{{2,3,4},3,195,3},{{2,1,0},18,217,1},{{2,1,0},36,217,1}}) {
        Menu::PracticeFighter fighter;fighter.practice_x=800;
        auto& actor=fighter.robofortune.emplace();actor.facing_right=facing;
        if(command.air){actor.start(89);actor.state.variables[0x3b]=2.0f;fighter.jump_height=900;}
        for(auto step=command.motion.begin();step!=command.motion.end();++step)
            tick(menu,fighter,facing,*step,step+1==command.motion.end()?command.buttons:0);
        check(actor.move==command.entry,"Robo command entry "+std::to_string(command.entry)+" got "+std::to_string(actor.move));
        for(unsigned t=0;t<1000 && actor.attacking();++t)tick(menu,fighter,facing);
        check(!actor.attacking(),"Robo special recovers "+std::to_string(command.entry));
        check(fighter.tension==5-command.cost,"Authored meter debit "+std::to_string(command.entry));
    }
    for(unsigned level:{1u,3u,5u}) {
        Menu::PracticeFighter fighter;fighter.practice_x=800;
        auto& actor=fighter.robofortune.emplace();actor.facing_right=facing;
        for(unsigned dir:{2u,1u,0u})tick(menu,fighter,facing,dir,dir==0?3:0);
        unsigned tier=0;
        for(unsigned t=0;t<1000 && actor.attacking();++t) {
            tick(menu,fighter,facing,8,t<80?(level==5?4u:level==3?3u:0u):0u);
            tier=std::max(tier,unsigned(actor.value(0xa8)));
        }
        check(!actor.attacking() && fighter.tension==5-level,"Beam tier spends and recovers: "+std::to_string(level));
        check(tier==(level-1)/2,"Beam uses its authored level branch");
    }
    for(unsigned bars:{0u,2u}) {
        Menu::PracticeFighter fighter;fighter.practice_x=800;fighter.tension=bars;
        fighter.robofortune.emplace().facing_right=facing;
        for(unsigned dir:{2u,3u,4u})tick(menu,fighter,facing,dir,dir==4?3:0);
        check(fighter.robofortune->move!=195 && fighter.tension==bars,"Insufficient meter rejects Circuit Breaker");
    }
}
void setup(Menu& menu) {
    menu.load_movement_frames();
    menu.fighters[0]={character("robofortune")};menu.fighters[1]={character("filia")};
    for(auto name:{"robofortune","filia"})menu.palettes[character(name)].push_back(
        {0,skullgirls::CharacterAssets(menu.game_data,name).read_default_palette()});
    menu.locked_colors[0]=menu.locked_colors[1]={0};
    menu.practice_width=1700;menu.practice_height=2000;menu.practice_floor=64;menu.practice_starts={700,1000};
    menu.prepare_practice_bitmaps();menu.load_practice_effects();menu.prepare_combat_art();
}
void combat(Menu& menu,bool facing) {
    for(unsigned move:{162u,163u,164u,165u,166u,167u,176u,177u,179u,180u,189u,195u}) {
        menu.reset_practice();auto& fighter=menu.practice_players[0];auto& actor=*fighter.robofortune;
        fighter.practice_x=facing?700:1000;menu.practice_players[1].practice_x=facing?1000:700;
        actor.facing_right=facing;actor.start(move);++fighter.attack_instance;
        if(move==176)menu.practice_players[1].practice_x=fighter.practice_x+(facing?100:-100);
        if(move>=165 && move<=167){fighter.jump_height=450;actor.state.variables[0x3b]=2.0f;}
        unsigned damage=0;bool freeze=false,exploded=false,captured=false;std::set<unsigned> children;
        for(unsigned t=0;t<1600;++t) {
            menu.practice_tick();damage=std::max(damage,menu.hud_stats[0].damage);
            freeze|=menu.practice_super_frozen();exploded|=actor.move==198 || actor.move==199;
            captured|=actor.captured_target;
            // Place an airborne target in the authored magnetic catch region.
            if(move==189 && actor.hit && !actor.captured_target) {
                auto& victim=menu.practice_players[1];victim.practice_x=fighter.practice_x+(facing?140:-140);
                victim.jump_height=400;victim.jump_velocity=0;
            }
            for(const auto& effect:menu.practice_effects)if(effect.root==&menu.robofortune_assets)
                children.insert(unsigned(effect.table-menu.robofortune_assets.children.data()));
        }
        check(!actor.attacking() && !actor.captured_target,"Combat recovers and releases: "+std::to_string(move));
        if(move==166 || move==167)check(children.count(36),"Air beam creates its own combat entity");
        else check(damage>0,"Robo move damages opponent: "+std::to_string(move));
        if(move>=180)check(freeze,"Blockbuster presents a super freeze");
        if(move==189)check(captured,"Magnetic Trap catches airborne opponents");
        if(move==195)check(exploded,"Circuit Breaker timer triggers its explosion");
    }
}
void headrones(Menu& menu,bool facing) {
    for(unsigned command:{169u,171u,173u}) {
        menu.reset_practice();auto& fighter=menu.practice_players[0];auto& actor=*fighter.robofortune;
        fighter.practice_x=facing?700:1000;menu.practice_players[1].practice_x=facing?1000:700;
        actor.facing_right=facing;
        for(unsigned i=0;i<3;++i) {
            actor.start(125);++fighter.attack_instance;
            for(unsigned t=0;t<140;++t)menu.practice_tick();
        }
        check(actor.value(0x6a)==1,"Three headrones occupy three of the four head slots");
        unsigned heads=0;for(auto& effect:menu.practice_effects)
            if(effect.table==&menu.robofortune_assets.children[2])++heads;
        check(heads==3,"Headrone entities survive later summoning animations");
        actor.start(command);++fighter.attack_instance;unsigned damage=0;std::set<unsigned> moves;
        for(unsigned t=0;t<900;++t) {
            menu.practice_tick();damage=std::max(damage,menu.hud_stats[0].damage);
            for(const auto& effect:menu.practice_effects)if(effect.table==&menu.robofortune_assets.children[2]) {
                moves.insert(effect.move);
                if(command==171 && effect.move==11) {
                    menu.practice_players[1].practice_x=effect.x;menu.practice_players[1].jump_height=0;
                }
            }
        }
        check(moves.count(command==169?14u:command==171?7u:18u),"Headrone command reaches its authored branch");
        check(damage>0,"Headrone attack damages opponent: "+std::to_string(command));
        check(actor.value(0x6a)==(command==173?4:2),"Head slots are reclaimed after use");
    }
}
void circuit_recharge(Menu& menu,bool facing) {
    for(unsigned button:{1u,2u,4u}) {
        menu.reset_practice();auto& fighter=menu.practice_players[0];auto& actor=*fighter.robofortune;
        actor.facing_right=facing;
        for(unsigned i=0;i<3;++i){actor.start(125);++fighter.attack_instance;for(unsigned t=0;t<140;++t)menu.practice_tick();}
        actor.start(195);++fighter.attack_instance;
        for(unsigned t=0;t<300;++t)menu.practice_tick();
        const auto timer=[&]()->float {for(const auto& effect:menu.practice_effects)
            if(effect.table==&menu.robofortune_assets.children[57])return effect.value(0xa8);return -1;};
        const auto before=timer();check(before>0 && actor.state.bits[29]==true,"Circuit Breaker remains active after startup: timer="+std::to_string(before)+" move="+std::to_string(actor.move)+" bit="+std::to_string(actor.state.bits[29].value_or(false)));
        for(unsigned dir:{2u,3u,4u})tick(menu,fighter,facing,dir,dir==4?button:0);
        check(actor.move==209,"Active Circuit Breaker accepts recharge commands");
        for(unsigned t=0;t<150;++t)menu.practice_tick();
        check(timer()>before-150,"Recharge extends or suspends the Circuit Breaker countdown");
        const auto saved=timer();menu.practice_paused=true;menu.practice_tick();menu.practice_paused=false;
        check(timer()==saved,"Pause stops the Circuit Breaker countdown");
        menu.save_or_restore_practice(false);menu.practice_tick();menu.save_or_restore_practice(true);
        check(timer()==saved,"Training restore preserves the Circuit Breaker entity and timer");
    }
}

void beam_range(Menu& menu,bool facing) {
    menu.practice_width=3750;menu.practice_starts=facing?std::array<float,2>{150,3600}:std::array<float,2>{3600,150};
    for(unsigned move:{162u,163u,164u,180u})for(unsigned level:{1u,3u,5u}) {
        if(move!=180 && level!=1)continue;
        menu.reset_practice();auto& actor=*menu.practice_players[0].robofortune;
        actor.facing_right=facing;actor.start(move);++menu.practice_players[0].attack_instance;
        bool painted=false;unsigned damage=0;
        for(unsigned t=0;t<900;++t) {
            menu.practice_key('A',move==180 && level==3 && t<80);
            menu.practice_key('S',move==180 && level==3 && t<80);
            menu.practice_key('D',move==180 && level==5 && t<80);
            menu.practice_tick();damage=std::max(damage,menu.hud_stats[0].damage);
            if(!painted && damage && t%8==0) {
                auto effects=std::move(menu.practice_effects);
                for(const auto& effect:effects)if(menu.robo_beam_part(effect,{33,36,49,50}))menu.practice_effects.push_back(effect);
                Gdiplus::Bitmap canvas(1280,720,PixelFormat32bppPARGB);
                {Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color::Transparent);
                 dc.ScaleTransform(1280.0f/3750,0.36f);menu.paint_practice_effects(dc,0,1800,true);menu.paint_practice_effects(dc,0,1800,false);}
                for(int y=0;y<720;++y){Gdiplus::Color pixel;canvas.GetPixel(facing?1270:10,y,&pixel);painted|=pixel.GetAlpha()>20;}
                menu.practice_effects=std::move(effects);
            }
        }
        check(damage>0,"Beam hits an opponent across the 3750-unit arena: "+std::to_string(move));
        check(painted,"Beam artwork reaches the opposite arena edge in either facing");
    }
}

}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
#else
int main(int argc,char** argv)
#endif
{
    if(argc!=2)return 2;
    Gdiplus::GdiplusStartupInput input;ULONG_PTR graphics=0;
    if(Gdiplus::GdiplusStartup(&graphics,&input,nullptr)!=Gdiplus::Ok)return 2;
    int result=0;
    try {
        auto menu_storage = std::make_unique<Menu>(); auto& menu = *menu_storage;menu.game_data=argv[1];menu.load_robofortune();
        check(menu.robofortune_assets.animations.size()==545,"Robo-Fortune move table loads");
        commands(menu,true);commands(menu,false);setup(menu);
        combat(menu,true);combat(menu,false);headrones(menu,true);headrones(menu,false);circuit_recharge(menu,true);circuit_recharge(menu,false);beam_range(menu,true);beam_range(menu,false);
        std::cout<<"Robo-Fortune specials, 1/3/5-bar beams, magnetic catch/slam, Circuit Breaker, three headrones and both facings passed\n";
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=1;}
    Gdiplus::GdiplusShutdown(graphics);return result;
}
