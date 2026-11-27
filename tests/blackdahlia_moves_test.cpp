#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
namespace {
void check(bool ok,const std::string& m){if(!ok)throw std::runtime_error(m);}
int character(std::string_view n){return int(std::find(roster.begin(),roster.end(),n)-roster.begin());}
void setup(Menu& m){m.load_movement_frames();m.load_blackdahlia();m.load_parasoul();
 m.fighters[0]={character("blackdahlia")};m.fighters[1]={character("parasoul")};
 for(auto name:{"blackdahlia","parasoul"})m.palettes[character(name)].push_back({0,skullgirls::CharacterAssets(m.game_data,name).read_default_palette()});
 m.locked_colors[0]=m.locked_colors[1]={0};m.practice_width=1700;m.practice_height=900;m.practice_floor=64;m.practice_starts={700,1000};
 m.prepare_practice_bitmaps();m.load_practice_effects();m.prepare_combat_art();m.reset_practice();}
void tick(Menu& m,Menu::PracticeFighter& f,bool facing,unsigned dir=8,unsigned buttons=0){
 constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
 f.practice_left=dir>=3&&dir<=5;f.practice_right=dir==0||dir==1||dir==7;f.practice_down=dir>=1&&dir<=3;f.practice_up=dir>=5&&dir<=7;
 auto edges=buttons&~f.held_attack_buttons;f.pressed_punches=edges&7;f.pressed_kicks=edges>>3;f.held_attack_buttons=buttons;
 m.tick_blackdahlia(f,facing?1500:100,0,1700,facing);}
void movement(Menu& m,bool facing){
 Menu::PracticeFighter f;f.practice_x=800;auto& a=f.blackdahlia.emplace();a.facing_right=facing;
 for(unsigned n=0;n<10;++n)tick(m,f,facing,0);
 check(facing?f.practice_x>800:f.practice_x<800,"Walk respects facing");
 tick(m,f,facing,6);for(unsigned n=0;n<20;++n)tick(m,f,facing);
 check(f.jump_height>0,"Jump rises");
 tick(m,f,facing,6);for(unsigned n=0;n<8;++n)tick(m,f,facing);

 for(unsigned n=0;n<250;++n)tick(m,f,facing);
 check(f.jump_height==0 && a.move==21,"Jump lands and recovers to idle");
}

void basic(Menu& m,bool facing){
 for(unsigned stance=0;stance<3;++stance)for(unsigned button:{1u,2u,4u,8u,16u,32u}){
 Menu::PracticeFighter f;f.practice_x=800;f.blackdahlia.emplace().facing_right=facing;
 if(stance==2){f.blackdahlia->start(95);f.blackdahlia->state.variables[0x3b]=2.0f;f.jump_height=800;}
 tick(m,f,facing,stance==1?2:8,button);check(f.blackdahlia->attacking(),"Black Dahlia normal button="+std::to_string(button)+" got "+std::to_string(f.blackdahlia->move));
 for(unsigned t=0;t<400&&f.blackdahlia->attacking();++t)tick(m,f,facing);
 check(!f.blackdahlia->attacking(),"Normal recovers");}
}
void snapshot(Menu& m){
 for(unsigned entry:{190u,185u,138u,239u}){
 m.practice_keys_held={};m.practice_keys_previous={};
 m.begin_netplay_match(1234);m.practice_presentation={};auto& a=*m.practice_players[0].blackdahlia;a.animate(m.blackdahlia_assets);a.start(entry);
 for(unsigned t=0;t<55;++t)m.advance_netplay_frame({},false);
 const auto saved=m.capture_netplay_state();
 const auto observe=[&](){std::ostringstream o;auto s=m.capture_netplay_state();
  o<<s.next_entity<<','<<s.simulation.stage_tick<<','<<s.simulation.combat.tick<<','<<s.simulation.stats[0].damage<<','<<s.simulation.stats[1].life;
  auto& f=s.simulation.fighters[0];o<<','<<f.blackdahlia->move<<','<<f.blackdahlia->record<<','<<f.blackdahlia->ticks<<','<<f.blackdahlia->captured_target<<','<<f.blackdahlia->value(0x63)<<','<<f.blackdahlia->value(0x64)<<','<<f.blackdahlia->random_state;
  for(auto& e:s.simulation.effects)o<<','<<e.entity_id<<','<<e.move<<','<<e.record<<','<<e.ticks<<','<<e.x<<','<<e.y;
  return o.str();};
 for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},false);const auto expected=observe();
 m.load_netplay_state(saved);for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},true);
 check(observe()==expected,"Black Dahlia rollback restores independent super, effects, entity IDs and damage state");
 m.network_mode=false;
 }
}
void teams(Menu& m){
 m.fighters[0]={character("filia"),character("blackdahlia")};m.locked_colors[0]={0,0};m.reset_practice();m.practice_presentation={};
 m.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==1 && !m.practice_teams[0].tag && m.practice_players[0].blackdahlia,"Tag activates Black Dahlia playback");
 for(unsigned n=0;n<650;++n)m.practice_tick();
 const auto& arrived=m.practice_players[0];
 check(arrived.blackdahlia->move==21&&arrived.jump_height==0&&arrived.blackdahlia->value(0xa4)==1&&arrived.blackdahlia->value(0xa5)==1&&arrived.blackdahlia->value(0xa6)>0,"Two-pass tag-in lands in visible playable idle: move="+std::to_string(arrived.blackdahlia->move));
 m.begin_practice_tag(0,0,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==0 && !m.practice_teams[0].tag && !m.practice_players[0].scripted(),"Black Dahlia tags back to Filia");
 m.reset_practice();m.practice_presentation={};m.practice_key('A',true);m.practice_key('X',true);m.practice_tick();m.practice_key('A',false);m.practice_key('X',false);
 auto& team=m.practice_teams[0];check(team.assist && team.assist->fighter.blackdahlia,"Black Dahlia assist uses own playback");bool acted=false;
 for(unsigned t=0;t<450 && team.assist;++t){m.practice_tick();acted|=team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting;}
 check(acted && !team.assist,"Black Dahlia assist acts and exits");
 m.fighters[0]={character("blackdahlia")};m.locked_colors[0]={0};m.reset_practice();
}
void grabs(Menu& m){
 for(unsigned move:{138u,149u}) {
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};auto& f=m.practice_players[0];auto& a=*f.blackdahlia;
  f.practice_x=700;m.practice_players[1].practice_x=800;for(unsigned n=0;n<3;++n)m.practice_tick();a.start(move);++f.attack_instance;
  if(move==149){f.jump_height=m.practice_players[1].jump_height=300;a.state.variables[0x3b]=2.0f;}
  bool caught=false;unsigned damage=0;
  for(unsigned n=0;n<1800;++n){m.practice_tick();caught|=a.captured_target;damage=std::max(damage,m.hud_stats[0].damage);}
  std::cout<<"GRAB "<<move<<" caught="<<caught<<" damage="<<damage<<" end="<<a.move<<" captured="<<a.captured_target<<'\n';
  check(caught&&damage>0&&!a.captured_target&&!a.attacking(),"Capture damages, releases and recovers: "+std::to_string(move));
 }
}
#ifdef _WIN32
void preview(Menu& m,const std::filesystem::path& output){
 skullgirls::SpriteArchive ui_win(m.game_data/"ui-win.gfs"),ui(m.game_data/"ui.gfs");
 m.combat_scene=skullgirls::read_combat_scene(ui_win.read("ui/Win/InGame.gbs"));
 for(unsigned i=0;i<2;++i)m.hud_atlas[i].load(ui_win.read("ui/Win/Textures/TA_1001_"+std::to_string(i+1)+".dds"));
 for(auto name:{"blackdahlia","parasoul"}){auto portrait=std::make_unique<Texture>();portrait->load(ui.read("ui/true_color_hud/super_"+std::string(name)+"01.dds"));m.super_portraits[character(name)]=std::move(portrait);}
 m.prepare_hud_portraits();m.prepare_combat_art();
 m.practice_background=std::make_unique<Texture>();m.practice_background->load(skullgirls::read_sprite_asset(m.game_data/"stages-textures2d.gfs","stages/textures/2D/stage_class_notes-alt.dds"),Texture::Encoding::stage_rgb);
 std::filesystem::create_directories(output);CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 for(auto move:{21u,106u,123u,130u,178u,190u,225u,229u,239u}){m.reset_practice();m.practice_presentation={};for(unsigned n=0;n<2;++n)m.practice_tick();auto& f=m.practice_players[0];f.blackdahlia->start(move);++f.attack_instance;
  if(move==130){f.jump_height=400;f.blackdahlia->state.variables[0x3b]=2.0f;}
  for(unsigned t=0;t<(move==225?40u:move==229?40u:move==239?100u:12u);++t){m.practice_tick();
   }

  Gdiplus::Bitmap canvas(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color(255,0,0,0));m.paint_practice(dc);}
  check(canvas.Save((output/(std::to_string(move)+".png")).c_str(),&png)==Gdiplus::Ok,"Render Black Dahlia preview");
 }
}
#endif
void commands(Menu& m,bool facing){
 for(unsigned button:{1u,2u,4u}){
  m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.blackdahlia;f.practice_x=800;a.facing_right=facing;
  for(unsigned n=0;n<7;++n)tick(m,f,facing);
  tick(m,f,facing,2);tick(m,f,facing,1);tick(m,f,facing,0,button);
  check(a.move>=190&&a.move<=192,"Quarter circle punch selects native shot");
 }
 // An empty cylinder reloads through the authored shot/reload branch.
 m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.blackdahlia;
 for(unsigned n=0;n<30;++n)m.practice_tick();a.state.variables[0x63]=0.0f;a.start(185);
 for(unsigned n=0;n<500;++n)m.practice_tick();
 check(a.value(0x63)>0&&!a.attacking(),"Reload fills native ammunition and recovers: move="+std::to_string(a.move)+" ammo="+std::to_string(a.value(0x63)));
}

void actions(Menu& m){
 for(bool facing:{true,false})for(unsigned entry:{106u,108u,109u,110u,111u,114u,115u,116u,117u,118u,123u,127u,172u,178u,182u,185u,190u,191u,192u,212u,225u,229u,236u,239u}){
  m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.blackdahlia;
  f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?820:880;
  if(entry>=190&&entry<=192)m.practice_players[1].practice_x=f.practice_x+(facing?650:-650);
  for(unsigned n=0;n<3;++n)m.practice_tick();a.facing_right=facing;a.start(entry);++f.attack_instance;
  unsigned damage=0;bool spawned=false,freeze=false;
  for(unsigned n=0;n<1200;++n){m.practice_tick();damage=std::max(damage,m.hud_stats[0].damage);freeze|=m.practice_super_frozen();for(auto& e:m.practice_effects)if(e.root==&m.blackdahlia_assets){spawned=true;check(std::isfinite(e.x)&&std::isfinite(e.y),"Finite child coordinates");}}
  std::cout<<"ACTION "<<entry<<" damage="<<damage<<" ammo="<<a.value(0x63)<<" end="<<a.move<<" meter="<<f.tension<<'\n';
  if(entry<=127||(entry>=190&&entry<=192))check(damage>0,"Native attack damages target: "+std::to_string(entry));
  check(!a.attacking(),"Action recovers: "+std::to_string(entry));
  if(entry==190||entry==191||entry==192)check(spawned&&a.value(0x63)!=6111111.0f,"Shot consumes native ammunition");
  if(entry==225||entry==229||entry==239)check(freeze&&f.tension==(entry==239?2u:4u),"Super freezes and spends authored meter");
 }
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
#else
int main(int argc,char** argv)
#endif
{if(argc<2)return 2;Gdiplus::GdiplusStartupInput gi;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&gi,nullptr);int result=0;
try{auto m=std::make_unique<Menu>();m->game_data=argv[1];setup(*m);
#ifdef _WIN32
if(argc>2){preview(*m,"build/blackdahlia-check/preview");}else
#endif
{basic(*m,true);basic(*m,false);movement(*m,true);movement(*m,false);commands(*m,true);commands(*m,false);grabs(*m);actions(*m);snapshot(*m);teams(*m);std::cout<<"Black Dahlia tests passed\n";}}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
