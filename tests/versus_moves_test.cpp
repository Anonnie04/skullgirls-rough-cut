#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
namespace {
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int character(std::string_view name){return int(std::find(roster.begin(),roster.end(),name)-roster.begin());}
void prepare(Menu& m){
 m.load_movement_frames();m.load_robofortune();m.load_practice_effects();
 m.begin_local_versus();m.training=Menu::TrainingView::practice;
 m.fighters[0]={character("filia")};m.fighters[1]={character("robofortune")};
 m.locked_colors[0]=m.locked_colors[1]={0};m.practice_width=1700;m.practice_height=2000;m.practice_floor=64;m.practice_starts={400,1250};
 m.reset_practice();m.practice_presentation={};
}
void combat(Menu& m){
 Menu::VersusMatch clock_units;
 check(clock_units.displayed_time()==99,"Native clock starts at 99");
 clock_units.remaining_ticks-=125;check(clock_units.displayed_time()==98,"125 native ticks advance one displayed count");
 check(m.local_versus&&!m.network_mode,"Versus is independent from netplay");
 m.training_options.push_back({L"Dummy jump",{},4,"OPT_STR_DUMMYACTION"});
 m.training_options.push_back({L"Infinite life",{},2,"OPT_STR_LIFEBAR_SETTING"});
 check(m.practice_speed()==100&&m.training_life()==15500&&m.training_value("OPT_STR_ATTACK_DATA",1)==0,"Match rules ignore Training cheats and overlays");
 check(m.practice_players[0].tension==1&&m.practice_players[1].tension==1,"Match starts with the native one-bar team meter");
 m.practice_input(VK_RIGHT,false);m.practice_input('J',false);
 for(unsigned n=0;n<12;++n)m.practice_tick();
 check(m.practice_players[0].practice_x>400&&m.practice_players[1].practice_x<1250,"Both human keyboard layouts move independently");
 m.practice_key(VK_RIGHT,false);m.practice_key('J',false);
 m.record_practice_damage(0,900);m.practice_players[1].hurt_group=-1;
 for(unsigned n=0;n<30;++n)m.practice_tick();
 check(m.hud_stats[1].life==14600,"Damage persists after a combo without Training regeneration");
 const auto clock=m.versus_match.remaining_ticks;
 m.practice_input('R',false);m.practice_input(VK_F6,false);m.practice_input(VK_F8,false);
 check(m.hud_stats[1].life==14600&&!m.practice_snapshot&&!m.dummy_recording,"Training shortcuts cannot reset or record a Versus match");
 m.practice_input(VK_RETURN,false);m.practice_tick();
 check(m.practice_paused&&m.versus_match.remaining_ticks==clock,"Pause stops the clock");
 check(m.practice_keys_held==std::array<unsigned,2>{},"Pause releases both human inputs");
 m.practice_input(VK_RETURN,false);m.practice_tick();
 check(!m.practice_paused&&m.versus_match.remaining_ticks==clock-1,"Resume advances the match clock");
 auto saved=m.capture_practice_state();m.versus_match.remaining_ticks=1;m.load_practice_state(saved);
 check(m.versus_match.remaining_ticks==clock-1,"Snapshots include match clock state");
 m.practice_players[0].visual.freeze=5;const auto frozen=m.versus_match.remaining_ticks;m.practice_tick();
 check(m.versus_match.remaining_ticks==frozen,"Super freeze stops the match clock");
 m.practice_key('J',true);m.release_practice_inputs();
 check(m.practice_paused&&m.practice_keys_held==std::array<unsigned,2>{}&&m.practice_attack_edges==std::array<unsigned,2>{},"Focus loss pauses and releases both sides");
 m.practice_paused=false;
}
void outcomes(Menu& m){
 using Phase=Menu::PracticePresentation::Phase;
 for(unsigned scenario=0;scenario<4;++scenario){
  m.practice_keys_held={};m.versus_match={};m.reset_practice();
  if(scenario==0)m.hud_stats[1].life=0;
  if(scenario==1)m.hud_stats[0].life=m.hud_stats[1].life=0;
  if(scenario>=2){m.versus_match.remaining_ticks=1;if(scenario==2)m.hud_stats[1].life=1000;}
  m.practice_tick();
  check(m.practice_presentation.phase==Phase::knockout,"Team defeat or timeout finishes the match");
  check(m.practice_presentation.defeated[1]&&m.practice_presentation.defeated[0]==(scenario==1||scenario==3),"KO and timeout winners/draws are correct");
  check(m.versus_match.timeout==(scenario>=2),"Timeout is distinct from KO");
  for(unsigned n=0;n<240;++n)m.practice_tick();
  check(m.practice_presentation.phase==Phase::results,"Match reaches victory/result presentation");
  m.practice_input(VK_RETURN,false);
  check(m.practice_presentation.phase==Phase::intro&&m.hud_stats[1].life==15500&&m.versus_match.remaining_ticks==Menu::VersusMatch::initial_ticks,"Rematch resets life and clock and replays native intros");
 }
}
void rounds(Menu& m){
 using Phase=Menu::PracticePresentation::Phase;
 m.versus_match={};m.reset_practice();
 for(unsigned round=0;round<2;++round){
  m.hud_stats[1].life=0;m.practice_tick();for(unsigned n=0;n<240;++n)m.practice_tick();
  check(m.versus_match.wins[0]==round+1&&m.versus_match.match_over==(round==1),"Original solo default requires two round wins");
  m.practice_input(VK_RETURN,false);
  if(round==0){check(m.versus_match.wins[0]==1,"Next round retains score");m.finish_practice_intro();check(m.versus_match.wins[0]==1,"Intro completion retains score");}
 }
 check(m.versus_match.wins[0]==0&&!m.versus_match.match_over,"Completed match rematch clears score");
 m.finish_practice_intro();
}
void reserves(Menu& m){
 m.fighters[1]={character("robofortune"),character("filia")};m.locked_colors[1]={0,0};m.reset_practice();m.hud_stats[1].life=0;
 m.practice_tick();check(m.practice_presentation.phase==Menu::PracticePresentation::Phase::none&&m.practice_teams[1].tag,"Point KO brings the reserve instead of ending the match");
 for(unsigned n=0;n<500;++n)m.practice_tick();
 check(m.practice_teams[1].active_slot==1&&m.hud_stats[1].life>0,"Living reserve becomes playable");
 m.hud_stats[1].life=0;m.practice_tick();check(m.practice_presentation.phase==Menu::PracticePresentation::Phase::knockout,"Final reserve KO ends match");
 // Native timeout comparison sums remaining team health, including reserves.
 m.reset_practice();m.versus_match.remaining_ticks=1;m.practice_tick();
 check(m.practice_presentation.defeated[0]&&!m.practice_presentation.defeated[1],"Timeout includes living reserves in the original total-health comparison");
}
void navigation(Menu& m){
 const auto& entries=m.options[5];const auto local=std::find_if(entries.begin(),entries.end(),[](const auto& o){return o.keyword=="Local_Versus";});
 check(local!=entries.end(),"Original Versus menu exposes Local_Versus");
 m.selected=5;m.cursor=local-entries.begin();m.activate_menu_option();
 check(m.local_versus&&m.training==Menu::TrainingView::team_size,"Native submenu action opens local team selection");
 m.leave_local_versus(true);check(m.local_versus&&m.training==Menu::TrainingView::team_size&&m.fighters[0].empty(),"Reselect retains Versus mode and clears teams");
 m.training_input(VK_ESCAPE,false,true);check(!m.local_versus&&m.training==Menu::TrainingView::none,"Cancel first team selection returns to menu");
 m.begin_local_versus();m.team_size[1]=1;m.player=1;m.training=Menu::TrainingView::team_size;
 m.training_input(VK_RIGHT,false,false);check(m.team_size[1]==1,"P1 navigation cannot change P2 team");
 m.training_input('L',false,false);check(m.team_size[1]==2,"P2 own layout chooses team size");
 m.begin_local_versus();m.team_size={1,1};
 m.training_input('A',true,false);m.training_input('A',true,false);
 check(m.player==1&&m.training==Menu::TrainingView::team_size&&m.fighters[0].size()==1,"P1 chooses a playable team with own keys");
 m.training_input('F',false,false);m.training_input('V',false,false);
 check(m.training==Menu::TrainingView::stage_select&&m.fighters[1].size()==1,"P2 own keys finish selection and open stage selection");
 m.leave_local_versus(false);check(!m.local_versus&&m.training==Menu::TrainingView::none,"Exit clears Versus mode");
}
#ifdef _WIN32
void preview(Menu& m){
 const auto out=std::filesystem::path("build/versus-check/preview");std::filesystem::create_directories(out);
 CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 const auto save=[&](const wchar_t* name){Gdiplus::Bitmap image(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&image);dc.Clear(Gdiplus::Color(255,0,0,0));m.paint(dc);}check(image.Save((out/name).c_str(),&png)==Gdiplus::Ok,"Render Versus fixture");};
 m.title_visible=false;m.entering_menu=false;m.begin_local_versus();save(L"team-select.png");
 prepare(m);m.prepare_practice_bitmaps();m.prepare_hud_portraits();m.prepare_combat_art();
 skullgirls::SpriteArchive ui(m.game_data/"ui-win.gfs");m.combat_scene=skullgirls::read_combat_scene(ui.read("ui/Win/InGame.gbs"));
 for(unsigned i=0;i<2;++i)m.hud_atlas[i].load(ui.read("ui/Win/Textures/TA_1001_"+std::to_string(i+1)+".dds"));
 m.practice_background=std::make_unique<Texture>();m.practice_background->load(skullgirls::read_sprite_asset(m.game_data/"stages-textures2d.gfs","stages/textures/2D/stage_class_notes-alt.dds"),Texture::Encoding::stage_rgb);
 for(unsigned n=0;n<3;++n)m.practice_tick();m.versus_match.remaining_ticks=42*125-1;save(L"match.png");
 m.practice_input(VK_RETURN,false);save(L"pause.png");m.practice_input(VK_RETURN,false);
 m.hud_stats[1].life=0;for(unsigned n=0;n<240;++n)m.practice_tick();save(L"round-result.png");
 check(m.combat_scene.texture_names.count("Textures/HUD/Timer_Number_Glow_4.png")!=0,"Native timer artwork is available");
}
#endif
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
#else
int main(int argc,char** argv)
#endif
{if(argc<2)return 2;Gdiplus::GdiplusStartupInput gi;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&gi,nullptr);int result=0;
 try{auto m=std::make_unique<Menu>();m->game_data=argv[1];m->load(argv[1]);
#ifdef _WIN32
 if(argc>2){preview(*m);}else
#endif
 {prepare(*m);combat(*m);outcomes(*m);rounds(*m);reserves(*m);navigation(*m);std::cout<<"Local Versus tests passed\n";}}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
