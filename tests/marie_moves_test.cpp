#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
namespace {
void check(bool ok,const std::string& m){if(!ok)throw std::runtime_error(m);}
int character(std::string_view n){return int(std::find(roster.begin(),roster.end(),n)-roster.begin());}
void setup(Menu& m){m.load_movement_frames();m.load_marie();m.load_parasoul();
 m.fighters[0]={character("marie")};m.fighters[1]={character("parasoul")};
 for(auto name:{"marie","parasoul"})m.palettes[character(name)].push_back({0,skullgirls::CharacterAssets(m.game_data,name).read_default_palette()});
 m.locked_colors[0]=m.locked_colors[1]={0};m.practice_width=1700;m.practice_height=900;m.practice_floor=64;m.practice_starts={700,1000};
 m.prepare_practice_bitmaps();m.load_practice_effects();m.prepare_combat_art();m.reset_practice();}
void tick(Menu& m,Menu::PracticeFighter& f,bool facing,unsigned dir=8,unsigned buttons=0){
 constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
 f.practice_left=dir>=3&&dir<=5;f.practice_right=dir==0||dir==1||dir==7;f.practice_down=dir>=1&&dir<=3;f.practice_up=dir>=5&&dir<=7;
 auto edges=buttons&~f.held_attack_buttons;f.pressed_punches=edges&7;f.pressed_kicks=edges>>3;f.held_attack_buttons=buttons;
 m.tick_marie(f,facing?1500:100,0,1700,facing);}
void commands(Menu& m,bool facing){
 struct Command{unsigned motion,buttons,first,last,cost;bool air=false;};
 for(auto c:{Command{3,1,156,159,0},{3,2,157,159,0},{3,4,158,159,0},
  {0,1,160,164,0},{0,2,161,164,0},{0,4,162,164,0},
  {3,8,165,170,0},{3,16,166,170,0},{3,32,167,170,0},
  {0,8,175,180,0},{0,16,176,180,0},{0,32,177,180,0},
  {3,8,171,174,0,true},{0,8,181,184,0,true},
  {0,3,190,191,1},{0,24,192,192,1},{3,3,209,211,3},{3,3,210,211,3,true}}){
 for(bool enough:{true,false}){if(!c.cost&&!enough)continue;
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
  auto& f=m.practice_players[0];auto& a=*f.marie;f.practice_x=800;a.facing_right=facing;m.practice_players[1].practice_x=facing?1500:100;f.tension=enough?std::max(1u,c.cost):0;
  if(c.air){a.start(99);f.jump_height=900;a.state.variables[0x3b]=2.0f;}
  for(unsigned n=0;n<7;++n)tick(m,f,facing);
  if(c.motion==0){tick(m,f,facing,2);tick(m,f,facing,1);tick(m,f,facing,0,c.buttons);}
  if(c.motion==3){tick(m,f,facing,2);tick(m,f,facing,3);if(c.air&&c.cost==3)a.start(13);tick(m,f,facing,4,c.buttons);}
  check((a.move>=c.first&&a.move<=c.last)==enough,"Command entry expected="+std::to_string(c.first)+" got="+std::to_string(a.move));
  if(enough){for(unsigned n=0;n<1500&&a.attacking();++n)tick(m,f,facing);
   check(!a.attacking(),"Command recovery expected="+std::to_string(c.first)+" got="+std::to_string(a.move));
   if(c.cost)check(f.tension==0,"Native super spends its authored meter");
   for(unsigned n=0;n<400;++n)tick(m,f,facing);
   check(a.move==21&&f.jump_height==0,"Command returns to actionable ground idle: "+std::to_string(c.first)+" end="+std::to_string(a.move));}
 }}
}
void combat(Menu& m){
 for(bool facing:{true,false})for(unsigned move:{112u,113u,114u,115u,116u,117u,118u,123u,124u,125u,126u,127u,128u,129u,130u,131u,132u,133u,157u,158u,165u,166u,167u,175u,176u,177u,190u,209u}) {
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
  auto& f=m.practice_players[0];auto& a=*f.marie;f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?800:900;a.facing_right=facing;
  for(unsigned n=0;n<3;++n)m.practice_tick();a.start(move);++f.attack_instance;
  if(move>=128&&move<=133){f.jump_height=300;a.state.variables[0x3b]=2.0f;m.practice_players[1].jump_height=300;}
  unsigned damage=0;bool freeze=false;for(unsigned t=0;t<1500;++t){if((move==166||move==167)&&!damage)m.practice_players[1].practice_x=f.practice_x+(facing?(move==167?850.0f:550.0f):(move==167?-850.0f:-550.0f));m.practice_tick();damage=std::max(damage,m.hud_stats[0].damage);freeze|=m.practice_super_frozen();}
  check(damage>0,"Marie combat damage move="+std::to_string(move)+" facing="+std::to_string(facing));
  check(!a.attacking(),"Marie combat recovery move="+std::to_string(move)+" end="+std::to_string(a.move));
  if(move==190||move==209)check(freeze&&f.tension==(move==190?4u:2u),"Super freezes and spends native meter");
 }
}
void snapshot(Menu& m){
 for(unsigned entry:{190u,165u,138u,209u}){
 m.practice_keys_held={};m.practice_keys_previous={};
 m.begin_netplay_match(1234);m.practice_presentation={};auto& a=*m.practice_players[0].marie;a.animate(m.marie_assets);a.start(entry);
 for(unsigned t=0;t<55;++t)m.advance_netplay_frame({},false);
 const auto saved=m.capture_netplay_state();
 const auto observe=[&](){std::ostringstream o;auto s=m.capture_netplay_state();
  o<<s.next_entity<<','<<s.simulation.stage_tick<<','<<s.simulation.combat.tick<<','<<s.simulation.stats[0].damage<<','<<s.simulation.stats[1].life;
  auto& f=s.simulation.fighters[0];o<<','<<f.marie->move<<','<<f.marie->record<<','<<f.marie->ticks<<','<<f.marie->captured_target<<','<<f.marie->value(0x6a)<<','<<f.marie->value(0x68);
  for(auto& e:s.simulation.effects)o<<','<<e.entity_id<<','<<e.move<<','<<e.record<<','<<e.ticks<<','<<e.x<<','<<e.y;
  return o.str();};
 for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},false);const auto expected=observe();
 m.load_netplay_state(saved);for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},true);
 check(observe()==expected,"Marie rollback restores independent super, effects, entity IDs and damage state");
 m.network_mode=false;
 }
}
void teams(Menu& m){
 m.fighters[0]={character("filia"),character("marie")};m.locked_colors[0]={0,0};m.reset_practice();m.practice_presentation={};
 m.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==1 && !m.practice_teams[0].tag && m.practice_players[0].marie,"Tag activates Marie playback");
 for(unsigned n=0;n<650;++n)m.practice_tick();
 const auto& arrived=m.practice_players[0];
 check(arrived.marie->move==21&&arrived.jump_height==0&&arrived.marie->value(0xa4)==1&&arrived.marie->value(0xa5)==1&&arrived.marie->value(0xa6)>0,"Two-pass tag-in lands in visible playable idle: move="+std::to_string(arrived.marie->move));
 m.begin_practice_tag(0,0,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==0 && !m.practice_teams[0].tag && !m.practice_players[0].scripted(),"Marie tags back to Filia");
 m.reset_practice();m.practice_presentation={};m.practice_key('A',true);m.practice_key('X',true);m.practice_tick();m.practice_key('A',false);m.practice_key('X',false);
 auto& team=m.practice_teams[0];check(team.assist && team.assist->fighter.marie,"Marie assist uses own playback");bool acted=false;
 for(unsigned t=0;t<450 && team.assist;++t){m.practice_tick();acted|=team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting;}
 check(acted && !team.assist,"Marie assist acts and exits");
 m.fighters[0]={character("marie")};m.locked_colors[0]={0};m.reset_practice();
}
void bunnies(Menu& m){
 for(bool facing:{true,false})for(unsigned entry:{160u,161u,162u}) {
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
  auto& f=m.practice_players[0];auto& a=*f.marie;f.practice_x=800;a.facing_right=facing;m.practice_players[1].practice_x=facing?1500:100;
  for(unsigned n=0;n<3;++n)m.practice_tick();a.start(entry);++f.attack_instance;bool bunny=false;float height=0;
  for(unsigned n=0;n<600;++n){m.practice_tick();for(auto& e:m.practice_effects)if(e.root==&m.marie_assets&&e.table==&m.marie_assets.children[33]) {
   bunny|=e.alive;height=std::max(height,e.y);check(std::isfinite(e.x)&&std::isfinite(e.y),"Bunny coordinates remain finite");
  }}
  check(bunny&&height>115&&!a.attacking(),"Native Dust Bunny toss spawns, rises and owner recovers");
 }
}
void grabs(Menu& m){
 for(unsigned move:{138u,147u,185u,192u}) {
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};auto& f=m.practice_players[0];auto& a=*f.marie;
  f.practice_x=700;m.practice_players[1].practice_x=800;for(unsigned n=0;n<3;++n)m.practice_tick();a.start(move);++f.attack_instance;
  if(move==147){f.jump_height=m.practice_players[1].jump_height=300;a.state.variables[0x3b]=2.0f;}
  bool caught=false;unsigned damage=0;
  for(unsigned n=0;n<1800;++n){m.practice_tick();caught|=a.captured_target;for(auto& e:m.practice_effects)caught|=e.root==&m.marie_assets&&e.table==&m.marie_assets.children[42]&&e.captured_target;damage=std::max(damage,m.hud_stats[0].damage);}
  std::cout<<"GRAB "<<move<<" caught="<<caught<<" damage="<<damage<<" end="<<a.move<<" captured="<<a.captured_target<<'\n';
  check(caught&&damage>0&&!a.captured_target&&!a.attacking(),"Capture damages, releases and recovers: "+std::to_string(move));
 }
}
void movement(Menu& m,bool facing){
 Menu::PracticeFighter f;f.practice_x=800;auto& a=f.marie.emplace();a.facing_right=facing;
 for(unsigned n=0;n<10;++n)tick(m,f,facing,0);
 check(facing?f.practice_x>800:f.practice_x<800,"Walk respects facing");
 tick(m,f,facing,6);for(unsigned n=0;n<20;++n)tick(m,f,facing);
 check(f.jump_height>0,"Jump rises");const auto before=f.jump_height;
 tick(m,f,facing,6);for(unsigned n=0;n<8;++n)tick(m,f,facing);
 check(f.jump_height>before,"Marie jump rises again");
 for(unsigned n=0;n<250;++n)tick(m,f,facing);
 check(f.jump_height==0 && a.move==21,"Jump/double jump lands and recovers to idle");
}
#ifdef _WIN32
void preview(Menu& m,const std::filesystem::path& output){
 skullgirls::SpriteArchive ui_win(m.game_data/"ui-win.gfs"),ui(m.game_data/"ui.gfs");
 m.combat_scene=skullgirls::read_combat_scene(ui_win.read("ui/Win/InGame.gbs"));
 for(unsigned i=0;i<2;++i)m.hud_atlas[i].load(ui_win.read("ui/Win/Textures/TA_1001_"+std::to_string(i+1)+".dds"));
 for(auto name:{"marie","parasoul"}){auto portrait=std::make_unique<Texture>();portrait->load(ui.read("ui/true_color_hud/super_"+std::string(name)+"01.dds"));m.super_portraits[character(name)]=std::move(portrait);}
 m.prepare_hud_portraits();m.prepare_combat_art();
 m.practice_background=std::make_unique<Texture>();m.practice_background->load(skullgirls::read_sprite_asset(m.game_data/"stages-textures2d.gfs","stages/textures/2D/stage_class_notes-alt.dds"),Texture::Encoding::stage_rgb);
 std::filesystem::create_directories(output);CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 for(auto move:{21u,112u,126u,130u,163u,168u,190u,192u,209u}){m.reset_practice();m.practice_presentation={};for(unsigned n=0;n<2;++n)m.practice_tick();auto& f=m.practice_players[0];f.marie->start(move);++f.attack_instance;
  if(move==130){f.jump_height=400;f.marie->state.variables[0x3b]=2.0f;}
  for(unsigned t=0;t<(move==190?65u:move==192?65u:move==209?100u:12u);++t){m.practice_tick();
   }

  Gdiplus::Bitmap canvas(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color(255,0,0,0));m.paint_practice(dc);}
  check(canvas.Save((output/(std::to_string(move)+".png")).c_str(),&png)==Gdiplus::Ok,"Render Marie preview");
 }
}
#endif
void basic(Menu& m,bool facing){
 for(unsigned stance=0;stance<3;++stance)for(unsigned button:{1u,2u,4u,8u,16u,32u}){
 Menu::PracticeFighter f;f.practice_x=800;f.marie.emplace().facing_right=facing;
 if(stance==2){f.marie->start(99);f.marie->state.variables[0x3b]=2.0f;f.jump_height=800;}
 tick(m,f,facing,stance==1?2:8,button);check(f.marie->attacking(),"Marie normal button="+std::to_string(button)+" got "+std::to_string(f.marie->move));
 for(unsigned t=0;t<400&&f.marie->attacking();++t)tick(m,f,facing);
 check(!f.marie->attacking(),"Normal recovers");}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
#else
int main(int argc,char** argv)
#endif
{if(argc<2)return 2;Gdiplus::GdiplusStartupInput gi;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&gi,nullptr);int result=0;
try{auto m=std::make_unique<Menu>();m->game_data=argv[1];setup(*m);if(argc>2){
#ifdef _WIN32
 if(std::filesystem::path(argv[2])==L"--commands"){}else if(std::filesystem::path(argv[2])==L"--preview")preview(*m,"build/marie-check/preview");else throw std::invalid_argument("Unknown preview option");
#endif
#ifndef _WIN32
 throw std::invalid_argument("Preview output requires Windows");
#endif
}else{basic(*m,true);basic(*m,false);movement(*m,true);movement(*m,false);commands(*m,true);commands(*m,false);grabs(*m);combat(*m);bunnies(*m);snapshot(*m);teams(*m);std::cout<<"Marie tests passed\n";}}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
