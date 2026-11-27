#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
namespace {
void check(bool ok,const std::string& m){if(!ok)throw std::runtime_error(m);}
int character(std::string_view n){return int(std::find(roster.begin(),roster.end(),n)-roster.begin());}
void setup(Menu& m){m.load_movement_frames();m.load_double_actor();m.load_parasoul();
 m.fighters[0]={character("double")};m.fighters[1]={character("parasoul")};
 for(auto name:{"double","parasoul"})m.palettes[character(name)].push_back({0,skullgirls::CharacterAssets(m.game_data,name).read_default_palette()});
 m.locked_colors[0]=m.locked_colors[1]={0};m.practice_width=1700;m.practice_height=2000;m.practice_floor=64;m.practice_starts={700,1000};
 m.prepare_practice_bitmaps();m.load_practice_effects();m.prepare_combat_art();m.reset_practice();}
void tick(Menu& m,Menu::PracticeFighter& f,bool facing,unsigned dir=8,unsigned buttons=0){
 constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
 f.practice_left=dir>=3&&dir<=5;f.practice_right=dir==0||dir==1||dir==7;f.practice_down=dir>=1&&dir<=3;f.practice_up=dir>=5&&dir<=7;
 auto edges=buttons&~f.held_attack_buttons;f.pressed_punches=edges&7;f.pressed_kicks=edges>>3;f.held_attack_buttons=buttons;
 m.tick_double_actor(f,facing?1500:100,0,1700,facing);}
void commands(Menu& m,bool facing){
 struct Command{unsigned motion,buttons,first,last,cost;bool air=false;};
 for(const auto c:{Command{0,1,156,156,0},{0,2,158,158,0},{0,4,160,160,0},
  {1,1,155,156,0},{1,2,157,158,0},{1,4,159,160,0},
  {2,8,161,165,0},{2,16,161,165,0},{2,32,161,165,0},
  {3,8,171,172,0,true},{3,16,174,175,0,true},{3,32,177,178,0,true},
  {3,8,180,182,0},{4,40,152,153,0},{0,3,185,186,1},{0,24,188,188,2},{3,24,230,230,1}}){
  for(bool enough:{true,false}){
   if(!c.cost&&!enough)continue;
   m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
   auto& f=m.practice_players[0];auto& a=*f.double_actor;f.practice_x=800;a.facing_right=facing;
   m.practice_players[1].practice_x=facing?1400:200;f.tension=enough?std::max(1u,c.cost):0;
   if(c.air){a.start(98);f.jump_height=800;a.state.variables[0x3b]=2.0f;}
   const auto input=[&](unsigned dir,unsigned buttons){constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
    unsigned mask=buttons<<4;if(dir>=3&&dir<=5)mask|=1;if(dir==0||dir==1||dir==7)mask|=2;
    if(dir>=1&&dir<=3)mask|=4;if(dir>=5&&dir<=7)mask|=8;m.practice_keys_held[0]=mask;m.practice_tick();};
   for(unsigned n=0;n<9;++n)input(8,0);
   if(c.motion==0){input(2,0);input(1,0);input(0,c.buttons);}
   if(c.motion==1){input(4,0);input(3,0);input(2,0);input(1,0);input(0,c.buttons);}
   if(c.motion==2){input(0,0);input(2,0);input(1,c.buttons);}
   if(c.motion==3){input(2,0);input(3,0);input(4,c.buttons);}
   if(c.motion==4)input(4,c.buttons);
   const bool entered=a.move>=c.first&&a.move<=c.last;
   check(entered==enough,"Original command/meter entry: expected="+std::to_string(c.first)+" got="+std::to_string(a.move));
   if(enough){bool freeze=false;for(unsigned n=0;n<1000&&a.attacking();++n){input(8,0);freeze|=m.practice_super_frozen();}
    check(!a.attacking(),"Command recovers: "+std::to_string(c.first)+" got="+std::to_string(a.move)+" x="+std::to_string(f.practice_x)+" d="+std::to_string(a.value(0x0d))+" b0="+std::to_string(a.value(0xb0))+" vx="+std::to_string(a.value(0x9e)));
    if(c.cost)check(freeze && f.tension==0,"Super freezes and spends its native meter once");
    check(f.practice_x>=0&&f.practice_x<=1700,"Super returns its owner to the playfield");}
  }
 }
 m.practice_keys_held={};m.practice_keys_previous={};
}
void combat(Menu& m,bool facing){
 for(unsigned move:{111u,113u,114u,115u,117u,118u,119u,121u,122u,123u,124u,125u,126u,127u,128u,129u,130u,131u,152u,156u,158u,160u,161u,162u,163u,171u,174u,177u,185u,230u}){
  m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.double_actor;
  f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?820:880;
  a.facing_right=facing;a.start(move);++f.attack_instance;
  bool air=(move>=126&&move<=131)||move==171||move==174||move==177;
  if(air){f.jump_height=300;a.state.variables[0x3b]=2.0f;m.practice_players[1].jump_height=300;}
  if(move==171||move==174||move==177){m.practice_players[1].practice_x=facing?755.0f:945.0f;m.practice_players[1].jump_height=0.0f;}
  unsigned hits=0;std::ostringstream trace;unsigned damage=0;bool freeze=false;std::set<unsigned> children;
  for(unsigned t=0;t<450;++t){m.practice_tick();damage=std::max(damage,m.hud_stats[0].damage);hits=std::max(hits,m.hud_stats[0].hits);freeze|=m.practice_super_frozen();
   for(auto& e:m.practice_effects)if(e.root==&m.double_assets){
    if(move>=156&&move<=160&&e.table==&m.double_assets.children[51]&&t%10==0&&t<100)trace<<" tick="<<t<<" x="<<e.x<<" y="<<e.y<<" move="<<e.move<<" hit="<<(e.hit?e.hit->damage:0)<<" alive="<<e.alive;
    children.insert(unsigned(e.table-m.double_assets.children.data()));}
  }
  check(damage>0,"Combat damages Parasoul: "+std::to_string(move)+" facing="+std::to_string(facing)+trace.str());
  check(!a.attacking(),"Combat move recovers: "+std::to_string(move));
  check(m.practice_players[1].throw_phase!=Menu::PracticeFighter::ThrowPhase::grabbed,"Special releases opponent: "+std::to_string(move));
  if(move==185)check(hits>=2,"Car executes multiple authored impacts");
  if(move==185||move==230)check(freeze && f.tension==4,"Blockbuster has freeze and one meter debit");

 }
}
void catellites(Menu& m,bool facing){
 m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
 auto& f=m.practice_players[0];f.practice_x=facing?600:1100;f.double_actor->facing_right=facing;
 m.practice_players[1].practice_x=facing?1150:550;f.double_actor->start(188);++f.attack_instance;
 unsigned max_heads=0,damage=0;bool drill=false;
 for(unsigned n=0;n<900;++n){
  m.practice_keys_held[0]=n>=150&&n%25==0?16u:0u;m.practice_tick();unsigned heads=0;
  for(auto& e:m.practice_effects)if(e.root==&m.double_assets&&e.table==&m.double_assets.children[81]&&e.alive){++heads;drill|=e.move>=4&&e.move<=6;}
  max_heads=std::max(max_heads,heads);damage=std::max(damage,m.hud_stats[0].damage);
 }
 check(max_heads>=6&&drill&&damage>0,"Six Catellites follow attacks and damage a distant opponent: heads="+std::to_string(max_heads)+" drill="+std::to_string(drill)+" damage="+std::to_string(damage));
 std::ostringstream timer;for(auto& e:m.practice_effects)if(e.root==&m.double_assets&&e.table==&m.double_assets.children[80])timer<<" a8="<<e.value(0xa8)<<" record="<<e.record<<" ticks="<<e.ticks<<" alive="<<e.alive<<" bits="<<e.state.bits[0].value_or(false)<<e.state.bits[1].value_or(false)<<e.state.bits[2].value_or(false)<<e.state.bits[3].value_or(false)<<e.state.bits[4].value_or(false)<<e.state.bits[5].value_or(false);
 for(auto& e:m.practice_effects)if(e.root==&m.double_assets&&e.table==&m.double_assets.children[81])timer<<" HEAD move="<<e.move<<" x="<<e.x<<" y="<<e.y<<" vx="<<e.value(0x9e)<<" kind="<<e.value(0xb0)<<" slot="<<e.state.bits[0].value_or(false)<<e.state.bits[1].value_or(false)<<e.state.bits[2].value_or(false)<<e.state.bits[3].value_or(false)<<e.state.bits[4].value_or(false)<<e.state.bits[5].value_or(false);
 check(!f.double_actor->state.bits[14].value_or(false),"Catellite duration expires through manager writes"+timer.str());
 m.practice_keys_held={};m.practice_keys_previous={};
}
void grabs(Menu& m){
 for(unsigned move:{136u,144u}){
  m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.double_actor;
  f.practice_x=700;m.practice_players[1].practice_x=800;a.start(move);++f.attack_instance;
  if(move==144){f.jump_height=300;m.practice_players[1].jump_height=300;a.state.variables[0x3b]=2.0f;}
  bool caught=false;unsigned damage=0;
  for(unsigned n=0;n<1000;++n){m.practice_tick();caught|=a.captured_target;damage=std::max(damage,m.hud_stats[0].damage);}
  std::cout<<"GRAB "<<move<<" caught "<<caught<<" damage "<<damage<<" end "<<a.move<<" captured "<<a.captured_target<<'\n';
  check(caught && damage>0 && !a.captured_target && !a.attacking(),"Throw captures, damages, releases and recovers: "+std::to_string(move));
 }
}
void snapshot(Menu& m){
 m.begin_netplay_match(1234);m.practice_presentation={};auto& a=*m.practice_players[0].double_actor;a.start(188);
 for(unsigned t=0;t<55;++t)m.advance_netplay_frame({},false);
 const auto saved=m.capture_netplay_state();
 const auto observe=[&](){std::ostringstream o;auto s=m.capture_netplay_state();
  o<<s.next_entity<<','<<s.simulation.stage_tick<<','<<s.simulation.combat.tick<<','<<s.simulation.stats[0].damage<<','<<s.simulation.stats[1].life;
  auto& f=s.simulation.fighters[0];o<<','<<f.double_actor->move<<','<<f.double_actor->record<<','<<f.double_actor->ticks;
  for(auto& e:s.simulation.effects)o<<','<<e.entity_id<<','<<e.move<<','<<e.record<<','<<e.ticks<<','<<e.x<<','<<e.y;
  return o.str();};
 for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},false);const auto expected=observe();
 m.load_netplay_state(saved);for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},true);
 check(observe()==expected,"Double rollback restores independent super, effects, entity IDs and damage state");
 m.network_mode=false;
}
void teams(Menu& m){
 m.fighters[0]={character("filia"),character("double")};m.locked_colors[0]={0,0};m.reset_practice();m.practice_presentation={};
 m.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==1 && !m.practice_teams[0].tag && m.practice_players[0].double_actor,"Tag activates Double playback");
 for(unsigned n=0;n<650;++n)m.practice_tick();
 const auto& arrived=m.practice_players[0];
 check(arrived.double_actor->move==21&&arrived.jump_height==0&&arrived.double_actor->value(0xa4)==1&&arrived.double_actor->value(0xa5)==1&&arrived.double_actor->value(0xa6)>0,"Two-pass tag-in lands in visible playable idle: move="+std::to_string(arrived.double_actor->move));
 m.begin_practice_tag(0,0,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==0 && !m.practice_teams[0].tag && !m.practice_players[0].scripted(),"Double tags back to Filia");
 m.reset_practice();m.practice_presentation={};m.practice_key('A',true);m.practice_key('X',true);m.practice_tick();m.practice_key('A',false);m.practice_key('X',false);
 auto& team=m.practice_teams[0];check(team.assist && team.assist->fighter.double_actor,"Double assist uses own playback");bool acted=false;
 for(unsigned t=0;t<450 && team.assist;++t){m.practice_tick();acted|=team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting;}
 check(acted && !team.assist,"Double assist acts and exits");
 m.fighters[0]={character("double")};m.locked_colors[0]={0};m.reset_practice();
}
void chain(Menu& m,bool facing){
 m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.double_actor;
 f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?880:820;a.facing_right=facing;
 unsigned stage=0,max_hits=0;float launch=0;bool air_attack=false;
 for(unsigned t=0;t<500;++t){unsigned mask=0;
  if(!stage){mask=16;stage=1;}
  else if(!f.hit_stop && a.value(1)>0){
   if(stage==1){mask=32;stage=2;}
   else if(stage==2){mask=512;stage=3;}
   else if(stage==3){mask=8|(facing?2u:1u);}
   else if(stage==4){mask=32;stage=5;}
  }
  if(stage==3 && f.jump_height>60){mask=16;stage=4;}
  m.practice_keys_held[0]=mask;m.practice_tick();
  max_hits=std::max(max_hits,m.hud_stats[0].hits);launch=std::max(launch,m.practice_players[1].jump_height);
  air_attack|=a.move>=126&&a.move<=131;
 }
 std::cout<<"CHAIN "<<stage<<" hits "<<max_hits<<" launch "<<launch<<" air "<<air_attack<<'\n';
 check(max_hits>=4 && launch>100 && air_attack,"Ground chain launches and follows with air normals");
}
void movement(Menu& m,bool facing){
 Menu::PracticeFighter f;f.practice_x=800;auto& a=f.double_actor.emplace();a.facing_right=facing;
 for(unsigned n=0;n<10;++n)tick(m,f,facing,0);
 check(facing?f.practice_x>800:f.practice_x<800,"Walk respects facing");
 tick(m,f,facing,6);for(unsigned n=0;n<20;++n)tick(m,f,facing);
 check(f.jump_height>0,"Jump rises");const auto before=f.jump_height;
 tick(m,f,facing,6);for(unsigned n=0;n<8;++n)tick(m,f,facing);
 check(f.jump_height>before,"Double jump rises again");
 for(unsigned n=0;n<250;++n)tick(m,f,facing);
 check(f.jump_height==0 && a.move==21,"Jump/double jump lands and recovers to idle");
}
#ifdef _WIN32
void preview(Menu& m,const std::filesystem::path& output){
 skullgirls::SpriteArchive ui_win(m.game_data/"ui-win.gfs"),ui(m.game_data/"ui.gfs");
 m.combat_scene=skullgirls::read_combat_scene(ui_win.read("ui/Win/InGame.gbs"));
 for(unsigned i=0;i<2;++i)m.hud_atlas[i].load(ui_win.read("ui/Win/Textures/TA_1001_"+std::to_string(i+1)+".dds"));
 for(auto name:{"double","parasoul"}){auto portrait=std::make_unique<Texture>();portrait->load(ui.read("ui/true_color_hud/super_"+std::string(name)+"01.dds"));m.super_portraits[character(name)]=std::move(portrait);}
 m.prepare_hud_portraits();m.prepare_combat_art();
 m.practice_background=std::make_unique<Texture>();m.practice_background->load(skullgirls::read_sprite_asset(m.game_data/"stages-textures2d.gfs","stages/textures/2D/stage_class_notes-alt.dds"),Texture::Encoding::stage_rgb);
 std::filesystem::create_directories(output);CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 for(auto move:{111u,124u,128u,161u,185u,188u,230u}){m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];f.double_actor->start(move);++f.attack_instance;
  if(move==128){f.jump_height=400;f.double_actor->state.variables[0x3b]=2.0f;}
  for(unsigned t=0;t<(move==185?400u:move==188?220u:move==230?75u:12u);++t){m.practice_tick();
   if(move==185&&f.double_actor->move==186&&f.practice_x>300&&f.practice_x<1300)break;}

  Gdiplus::Bitmap canvas(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color(255,0,0,0));m.paint_practice(dc);}
  check(canvas.Save((output/(std::to_string(move)+".png")).c_str(),&png)==Gdiplus::Ok,"Render Double preview");
 }
}
#endif
void basic(Menu& m,bool facing){
 for(unsigned stance=0;stance<3;++stance)for(unsigned button:{1u,2u,4u,8u,16u,32u}){
 Menu::PracticeFighter f;f.practice_x=800;f.double_actor.emplace().facing_right=facing;
 if(stance==2){f.double_actor->start(98);f.double_actor->state.variables[0x3b]=2.0f;f.jump_height=800;}
 tick(m,f,facing,stance==1?2:8,button);check(f.double_actor->attacking(),"Double normal starts: "+std::to_string(button)+" got "+std::to_string(f.double_actor->move));
 for(unsigned t=0;t<400&&f.double_actor->attacking();++t)tick(m,f,facing);
 check(!f.double_actor->attacking(),"Normal recovers");}
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
 if(std::filesystem::path(argv[2])==L"--commands"){commands(*m,true);commands(*m,false);}else if(std::filesystem::path(argv[2])==L"--preview")preview(*m,"build/double-check/preview");else throw std::invalid_argument("Unknown preview option");
#endif
#ifndef _WIN32
 throw std::invalid_argument("Preview output requires Windows");
#endif
}else{basic(*m,true);basic(*m,false);commands(*m,true);commands(*m,false);movement(*m,true);movement(*m,false);combat(*m,true);combat(*m,false);catellites(*m,true);catellites(*m,false);grabs(*m);snapshot(*m);teams(*m);chain(*m,true);chain(*m,false);std::cout<<"Double movement, normals, specials, super meter/presentation, combat, throws, projectile and super, rollback and combo tests passed\n";}}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
