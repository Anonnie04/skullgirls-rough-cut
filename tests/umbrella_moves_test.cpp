#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
namespace {
void check(bool ok,const std::string& m){if(!ok)throw std::runtime_error(m);}
int character(std::string_view n){return int(std::find(roster.begin(),roster.end(),n)-roster.begin());}
void setup(Menu& m){m.load_movement_frames();m.load_umbrella();m.load_parasoul();
 m.fighters[0]={character("umbrella")};m.fighters[1]={character("parasoul")};
 for(auto name:{"umbrella","parasoul"})m.palettes[character(name)].push_back({0,skullgirls::CharacterAssets(m.game_data,name).read_default_palette()});
 m.locked_colors[0]=m.locked_colors[1]={0};m.practice_width=1700;m.practice_height=900;m.practice_floor=64;m.practice_starts={700,1000};
 m.prepare_practice_bitmaps();m.load_practice_effects();m.prepare_combat_art();m.reset_practice();}
void tick(Menu& m,Menu::PracticeFighter& f,bool facing,unsigned dir=8,unsigned buttons=0){
 constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
 f.practice_left=dir>=3&&dir<=5;f.practice_right=dir==0||dir==1||dir==7;f.practice_down=dir>=1&&dir<=3;f.practice_up=dir>=5&&dir<=7;
 auto edges=buttons&~f.held_attack_buttons;f.pressed_punches=edges&7;f.pressed_kicks=edges>>3;f.held_attack_buttons=buttons;
 m.tick_umbrella(f,facing?1500:100,0,1700,facing);}
void commands(Menu& m,bool facing){
 struct Command{unsigned motion,buttons,first,last,cost;};
 for(auto c:{Command{3,8,205,205,0},{3,16,206,207,0},{3,32,208,208,0},
  {0,1,226,226,0},{4,2,218,219,0},{6,4,209,213,0},
  {3,24,244,244,1},{0,3,245,245,1},{0,24,246,248,1},{3,3,249,249,3}}){
 for(bool enough:{true,false}){if(!c.cost&&!enough)continue;
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
  auto& f=m.practice_players[0];auto& a=*f.umbrella;f.practice_x=800;a.facing_right=facing;m.practice_players[1].practice_x=facing?1500:100;f.tension=enough?std::max(1u,c.cost):0;
  for(unsigned n=0;n<7;++n)tick(m,f,facing);
  if(c.motion==0){tick(m,f,facing,2);tick(m,f,facing,1);tick(m,f,facing,0,c.buttons);}
  if(c.motion==3){tick(m,f,facing,2);tick(m,f,facing,3);tick(m,f,facing,4,c.buttons);}
  if(c.motion==4){for(unsigned n=0;n<55;++n)tick(m,f,facing,4);tick(m,f,facing,0,c.buttons);}
  if(c.motion==6){for(unsigned n=0;n<55;++n)tick(m,f,facing,2);tick(m,f,facing,6,c.buttons);}
  check((a.move>=c.first&&a.move<=c.last)==enough,"Command entry expected="+std::to_string(c.first)+" got="+std::to_string(a.move));
  if(enough){for(unsigned n=0;n<1500&&a.attacking();++n)tick(m,f,facing);
   check(!a.attacking(),"Command recovery expected="+std::to_string(c.first)+" got="+std::to_string(a.move));
   if(c.cost)check(f.tension==0,"Native super spends its authored meter");}
 }}
}
void combat(Menu& m){
 for(bool facing:{true,false})for(unsigned move:{124u,126u,132u,136u,140u,147u,148u,149u,150u,151u,153u,154u,155u,161u,162u,171u,175u,205u,218u,226u,244u,245u}) {
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
  auto& f=m.practice_players[0];auto& a=*f.umbrella;f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?800:900;a.facing_right=facing;
  for(unsigned n=0;n<3;++n)m.practice_tick();a.start(move);++f.attack_instance;
  if(move>=154&&move<=175){f.jump_height=300;a.state.variables[0x3b]=2.0f;m.practice_players[1].jump_height=300;}
  unsigned damage=0;bool freeze=false;for(unsigned t=0;t<1500;++t){m.practice_tick();damage=std::max(damage,m.hud_stats[0].damage);freeze|=m.practice_super_frozen();}
  check(damage>0,"Umbrella combat damage move="+std::to_string(move)+" facing="+std::to_string(facing));
  check(!a.attacking(),"Umbrella combat recovery move="+std::to_string(move)+" end="+std::to_string(a.move));
  if(move>=244)check(freeze&&f.tension==4,"Super freezes and spends one bar");
 }
}
void grabs(Menu& m){
 for(unsigned move:{186u,196u,249u,226u,209u,233u}) {
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};auto& f=m.practice_players[0];auto& a=*f.umbrella;
  f.practice_x=700;m.practice_players[1].practice_x=move==233?1100:800;for(unsigned n=0;n<3;++n)m.practice_tick();
  if(move==226||move==209){a.state.variables[0x6a]=900.0f;a.state.variables[0x68]=2.0f;}
  a.start(move);++f.attack_instance;
  if(move==141)m.practice_players[1].jump_height=300;
  if(move==196){f.jump_height=m.practice_players[1].jump_height=300;a.state.variables[0x3b]=2.0f;}
  bool caught=false;unsigned damage=0;
  for(unsigned n=0;n<2500;++n){m.practice_keys_held[0]=a.captured_target&&move==226&&n%8==0?16:0;m.practice_tick();caught|=a.captured_target;damage=std::max(damage,m.hud_stats[0].damage);}
  std::cout<<"GRAB "<<move<<" caught="<<caught<<" damage="<<damage<<" end="<<a.move<<" captured="<<a.captured_target<<'\n';
  check(caught&&damage>0&&!a.captured_target&&!a.attacking(),"Capture damages, releases and recovers: "+std::to_string(move));
 }
}
void snapshot(Menu& m){
 for(unsigned entry:{244u,205u,186u}){
 m.practice_keys_held={};m.practice_keys_previous={};
 m.begin_netplay_match(1234);m.practice_presentation={};auto& a=*m.practice_players[0].umbrella;a.animate(m.umbrella_assets);a.start(entry);
 for(unsigned t=0;t<55;++t)m.advance_netplay_frame({},false);
 const auto saved=m.capture_netplay_state();
 const auto observe=[&](){std::ostringstream o;auto s=m.capture_netplay_state();
  o<<s.next_entity<<','<<s.simulation.stage_tick<<','<<s.simulation.combat.tick<<','<<s.simulation.stats[0].damage<<','<<s.simulation.stats[1].life;
  auto& f=s.simulation.fighters[0];o<<','<<f.umbrella->move<<','<<f.umbrella->record<<','<<f.umbrella->ticks<<','<<f.umbrella->captured_target<<','<<f.umbrella->value(0x6a)<<','<<f.umbrella->value(0x68);
  for(auto& e:s.simulation.effects)o<<','<<e.entity_id<<','<<e.move<<','<<e.record<<','<<e.ticks<<','<<e.x<<','<<e.y;
  return o.str();};
 for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},false);const auto expected=observe();
 m.load_netplay_state(saved);for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},true);
 check(observe()==expected,"Umbrella rollback restores independent super, effects, entity IDs and damage state");
 m.network_mode=false;
 }
}
void teams(Menu& m){
 m.fighters[0]={character("filia"),character("umbrella")};m.locked_colors[0]={0,0};m.reset_practice();m.practice_presentation={};
 m.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==1 && !m.practice_teams[0].tag && m.practice_players[0].umbrella,"Tag activates Umbrella playback");
 for(unsigned n=0;n<650;++n)m.practice_tick();
 const auto& arrived=m.practice_players[0];
 check(arrived.umbrella->move==21&&arrived.jump_height==0&&arrived.umbrella->value(0xa4)==1&&arrived.umbrella->value(0xa5)==1&&arrived.umbrella->value(0xa6)>0,"Two-pass tag-in lands in visible playable idle: move="+std::to_string(arrived.umbrella->move));
 m.begin_practice_tag(0,0,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==0 && !m.practice_teams[0].tag && !m.practice_players[0].scripted(),"Umbrella tags back to Filia");
 m.reset_practice();m.practice_presentation={};m.practice_key('A',true);m.practice_key('X',true);m.practice_tick();m.practice_key('A',false);m.practice_key('X',false);
 auto& team=m.practice_teams[0];check(team.assist && team.assist->fighter.umbrella,"Umbrella assist uses own playback");bool acted=false;
 for(unsigned t=0;t<450 && team.assist;++t){m.practice_tick();acted|=team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting;}
 check(acted && !team.assist,"Umbrella assist acts and exits");
 m.fighters[0]={character("umbrella")};m.locked_colors[0]={0};m.reset_practice();
}
void bubbles(Menu& m){
 for(auto entry:{206u,208u}) {
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
  auto& f=m.practice_players[0];auto& a=*f.umbrella;m.practice_players[1].practice_x=1500;
  for(unsigned n=0;n<3;++n)m.practice_tick();a.start(entry);++f.attack_instance;bool bubble=false;
  for(unsigned n=0;n<600;++n){m.practice_tick();for(auto& e:m.practice_effects) {
   bubble|=e.root==&m.umbrella_assets&&e.table==&m.umbrella_assets.children[entry==206?66:68]&&e.alive;
   check(std::isfinite(e.x)&&std::isfinite(e.y),"Bubble coordinates remain finite");
  }}
  check(bubble&&!a.attacking(),"Native bubble entity spawns and owner recovers");
 }
}
void hunger(Menu& m){
 m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
 auto& a=*m.practice_players[0].umbrella;
 for(unsigned n=0;n<10;++n)m.practice_tick();
 std::cout<<"INITIAL hunger="<<a.value(0x6a)<<" mode="<<a.value(0x68)<<" effects="<<m.practice_effects.size()<<'\n';
 check(std::any_of(m.practice_effects.begin(),m.practice_effects.end(),[&](auto& e){return e.root==&m.umbrella_assets&&e.table==&m.umbrella_assets.children[0]&&e.alive;}),"Native Hunger manager spawns");
 for(float gauge:{100.0f,400.0f,850.0f,1000.0f}) {
  a.state.variables[0x6a]=gauge;for(unsigned n=0;n<5;++n)m.practice_tick();
  check(a.value(0x68)==(gauge<223?4.0f:gauge<777?3.0f:gauge<1000?2.0f:1.0f),"Native Hunger thresholds select their authored modes");
  check(a.value(0x6a)>=0&&a.value(0x6a)<=1000,"Hunger gauge stays bounded");
  std::cout<<"HUNGER input="<<gauge<<" result="<<a.value(0x6a)<<" mode="<<a.value(0x68)<<'\n';
 }
}
void movement(Menu& m,bool facing){
 Menu::PracticeFighter f;f.practice_x=800;auto& a=f.umbrella.emplace();a.facing_right=facing;
 for(unsigned n=0;n<10;++n)tick(m,f,facing,0);
 check(facing?f.practice_x>800:f.practice_x<800,"Walk respects facing");
 tick(m,f,facing,6);for(unsigned n=0;n<20;++n)tick(m,f,facing);
 check(f.jump_height>0,"Jump rises");const auto before=f.jump_height;
 tick(m,f,facing,6);for(unsigned n=0;n<8;++n)tick(m,f,facing);
 check(f.jump_height>before,"Umbrella jump rises again");
 for(unsigned n=0;n<250;++n)tick(m,f,facing);
 check(f.jump_height==0 && a.move==21,"Jump/double jump lands and recovers to idle");
}
#ifdef _WIN32
void preview(Menu& m,const std::filesystem::path& output){
 skullgirls::SpriteArchive ui_win(m.game_data/"ui-win.gfs"),ui(m.game_data/"ui.gfs");
 m.combat_scene=skullgirls::read_combat_scene(ui_win.read("ui/Win/InGame.gbs"));
 for(unsigned i=0;i<2;++i)m.hud_atlas[i].load(ui_win.read("ui/Win/Textures/TA_1001_"+std::to_string(i+1)+".dds"));
 for(auto name:{"umbrella","parasoul"}){auto portrait=std::make_unique<Texture>();portrait->load(ui.read("ui/true_color_hud/super_"+std::string(name)+"01.dds"));m.super_portraits[character(name)]=std::move(portrait);}
 m.prepare_hud_portraits();m.prepare_combat_art();
 m.practice_background=std::make_unique<Texture>();m.practice_background->load(skullgirls::read_sprite_asset(m.game_data/"stages-textures2d.gfs","stages/textures/2D/stage_class_notes-alt.dds"),Texture::Encoding::stage_rgb);
 std::filesystem::create_directories(output);CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 for(auto move:{21u,124u,151u,161u,205u,209u,244u,245u,249u}){m.reset_practice();m.practice_presentation={};for(unsigned n=0;n<2;++n)m.practice_tick();auto& f=m.practice_players[0];f.umbrella->start(move);++f.attack_instance;
  if(move==161){f.jump_height=400;f.umbrella->state.variables[0x3b]=2.0f;}
  for(unsigned t=0;t<(move==244?75u:move==245?50u:move==249?100u:12u);++t){m.practice_tick();
   }

  Gdiplus::Bitmap canvas(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color(255,0,0,0));m.paint_practice(dc);}
  check(canvas.Save((output/(std::to_string(move)+".png")).c_str(),&png)==Gdiplus::Ok,"Render Umbrella preview");
 }
}
#endif
void basic(Menu& m,bool facing){
 for(unsigned mode=1;mode<=4;++mode)for(unsigned stance=0;stance<3;++stance)for(unsigned button:{1u,2u,4u,8u,16u,32u}){
 Menu::PracticeFighter f;f.practice_x=800;f.umbrella.emplace().facing_right=facing;f.umbrella->state.variables[0x68]=float(mode);
 if(stance==2){f.umbrella->start(111);f.umbrella->state.variables[0x3b]=2.0f;f.jump_height=800;}
 tick(m,f,facing,stance==1?2:8,button);check(f.umbrella->attacking(),"Umbrella normal mode="+std::to_string(mode)+" button="+std::to_string(button)+" got "+std::to_string(f.umbrella->move));
 for(unsigned t=0;t<400&&f.umbrella->attacking();++t)tick(m,f,facing);
 check(!f.umbrella->attacking(),"Normal recovers");}
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
 if(std::filesystem::path(argv[2])==L"--commands"){}else if(std::filesystem::path(argv[2])==L"--preview")preview(*m,"build/umbrella-check/preview");else throw std::invalid_argument("Unknown preview option");
#endif
#ifndef _WIN32
 throw std::invalid_argument("Preview output requires Windows");
#endif
}else{basic(*m,true);basic(*m,false);movement(*m,true);movement(*m,false);hunger(*m);commands(*m,true);commands(*m,false);grabs(*m);combat(*m);bubbles(*m);snapshot(*m);teams(*m);std::cout<<"Umbrella tests passed\n";}}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
