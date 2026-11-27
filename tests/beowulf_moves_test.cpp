#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
namespace {
void check(bool ok,const std::string& m){if(!ok)throw std::runtime_error(m);}
int character(std::string_view n){return int(std::find(roster.begin(),roster.end(),n)-roster.begin());}
void setup(Menu& m){m.load_movement_frames();m.load_beowulf();m.load_parasoul();
 m.fighters[0]={character("beowulf")};m.fighters[1]={character("parasoul")};
 for(auto name:{"beowulf","parasoul"})m.palettes[character(name)].push_back({0,skullgirls::CharacterAssets(m.game_data,name).read_default_palette()});
 m.locked_colors[0]=m.locked_colors[1]={0};m.practice_width=1700;m.practice_height=900;m.practice_floor=64;m.practice_starts={700,1000};
 m.prepare_practice_bitmaps();m.load_practice_effects();m.prepare_combat_art();m.reset_practice();}
void tick(Menu& m,Menu::PracticeFighter& f,bool facing,unsigned dir=8,unsigned buttons=0){
 constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
 f.practice_left=dir>=3&&dir<=5;f.practice_right=dir==0||dir==1||dir==7;f.practice_down=dir>=1&&dir<=3;f.practice_up=dir>=5&&dir<=7;
 auto edges=buttons&~f.held_attack_buttons;f.pressed_punches=edges&7;f.pressed_kicks=edges>>3;f.held_attack_buttons=buttons;
 m.tick_beowulf(f,facing?1500:100,0,1700,facing);}
void commands(Menu& m,bool facing){
 struct Command{unsigned motion,buttons,first,last,cost;bool air=false;};
 for(const auto c:{Command{0,1,191,193,0},{0,2,194,198,0},{0,4,195,198,0},
  {0,8,199,202,0},{0,16,200,202,0},{0,32,201,202,0},
  {0,8,203,206,0,true},{0,16,204,206,0,true},{0,32,205,206,0,true},
  {0,3,234,235,1},{3,24,237,238,1},{3,3,241,242,3}}){
  for(bool enough:{true,false}){
   if(!c.cost&&!enough)continue;
   m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
   auto& f=m.practice_players[0];auto& a=*f.beowulf;f.practice_x=800;a.facing_right=facing;
   m.practice_players[1].practice_x=facing?1400:200;f.tension=enough?std::max(1u,c.cost):0;
   if(c.air){a.start(99);f.jump_height=800;a.state.variables[0x3b]=2.0f;}
   const auto input=[&](unsigned dir,unsigned buttons){constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
    unsigned mask=buttons<<4;if(dir>=3&&dir<=5)mask|=1;if(dir==0||dir==1||dir==7)mask|=2;
    if(dir>=1&&dir<=3)mask|=4;if(dir>=5&&dir<=7)mask|=8;m.practice_keys_held[0]=mask;m.practice_tick();};
   for(unsigned n=0;n<7;++n)input(8,0);
   if(c.motion==0){input(2,0);input(1,0);input(0,c.buttons);}
   if(c.motion==2){input(0,0);input(2,0);input(1,c.buttons);}
   if(c.motion==3){input(2,0);input(3,0);input(4,c.buttons);}
   if(c.motion==4){for(unsigned n=0;n<50;++n)input(4,0);input(0,c.buttons);}
   const bool entered=a.move>=c.first&&a.move<=c.last;
   check(entered==enough,"Original command/meter entry: expected="+std::to_string(c.first)+" got="+std::to_string(a.move));
   if(enough){bool freeze=false;for(unsigned n=0;n<1100&&a.attacking();++n){input(8,0);freeze|=m.practice_super_frozen();}
    check(!a.attacking(),"Command recovers: "+std::to_string(c.first)+" got="+std::to_string(a.move)+" x="+std::to_string(f.practice_x)+" target="+std::to_string(m.practice_players[1].practice_x)+" a8="+std::to_string(a.value(0xa8))+" a9="+std::to_string(a.value(0xa9))+" b0="+std::to_string(a.value(0xb0))+" hits="+std::to_string(a.value(3))+" desc="+std::to_string(a.hit?a.hit->damage:0));
    if(c.cost)check(freeze && f.tension==0,"Super freezes and spends its native meter once");
    check(f.practice_x>=0&&f.practice_x<=1700,"Move returns its owner to the playfield");}
  }
 }
 m.practice_keys_held={};m.practice_keys_previous={};
}
void combat(Menu& m,bool facing){
 for(unsigned move:{112u,115u,116u,119u,120u,124u,125u,126u,127u,128u,130u,134u,149u,150u,151u,152u,156u,162u,163u,191u,194u,195u,199u,200u,201u,234u,241u}){
  m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.beowulf;
  f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?820:880;
  a.facing_right=facing;a.start(move);++f.attack_instance;
  bool air=(move>=149&&move<=164);
  if(air){f.jump_height=300;a.state.variables[0x3b]=2.0f;m.practice_players[1].jump_height=300;}

  unsigned hits=0;std::ostringstream trace;unsigned damage=0;bool freeze=false;std::set<unsigned> children;
  for(unsigned t=0;t<1100;++t){m.practice_tick();damage=std::max(damage,m.hud_stats[0].damage);hits=std::max(hits,m.hud_stats[0].hits);freeze|=m.practice_super_frozen();
   for(auto& e:m.practice_effects)if(e.root==&m.beowulf_assets){
    if(move>=156&&move<=160&&e.table==&m.beowulf_assets.children[51]&&t%10==0&&t<100)trace<<" tick="<<t<<" x="<<e.x<<" y="<<e.y<<" move="<<e.move<<" hit="<<(e.hit?e.hit->damage:0)<<" alive="<<e.alive;
    children.insert(unsigned(e.table-m.beowulf_assets.children.data()));}
  }
  check(damage>0,"Combat damages Parasoul: "+std::to_string(move)+" facing="+std::to_string(facing)+trace.str());
  check(!a.attacking(),"Combat move recovers: "+std::to_string(move));
  check(m.practice_players[1].throw_phase!=Menu::PracticeFighter::ThrowPhase::grabbed,"Special releases opponent: "+std::to_string(move));

  if(move==234||move==241)check(freeze && f.tension==(move==241?2u:4u),"Blockbuster has freeze and one meter debit");

 }
}
void grabs(Menu& m){
 m.practice_keys_held={};m.practice_keys_previous={};
 for(unsigned move:{169u,176u,229u,238u,254u}){
  m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.beowulf;
  f.practice_x=700;m.practice_players[1].practice_x=800;a.start(move==238?237u:move);++f.attack_instance;
  if(move==176){f.jump_height=300;a.state.variables[0x3b]=2.0f;}
  if(move==176)m.practice_players[1].jump_height=300;
  bool caught=false;unsigned damage=0;
  for(unsigned n=0;n<1000;++n){
   m.practice_keys_held[0]=a.move==267?4u|512u:0;m.practice_tick();caught|=a.captured_target;damage=std::max(damage,m.hud_stats[0].damage);}
  std::cout<<"GRAB "<<move<<" caught "<<caught<<" damage "<<damage<<" end "<<a.move<<" captured "<<a.captured_target<<'\n';
  check(caught && damage>0 && !a.captured_target && !a.attacking(),"Throw captures, damages, releases and recovers: "+std::to_string(move));
 }
}
void chair_and_finishers(Menu& m){
 for(bool facing:{true,false}){
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
  auto& f=m.practice_players[0];auto& a=*f.beowulf;f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?1300:400;a.facing_right=facing;
  for(unsigned n=0;n<8;++n)m.practice_tick();
  const auto held=[&](){return std::count_if(m.practice_effects.begin(),m.practice_effects.end(),[&](const auto& e){return e.root==&m.beowulf_assets&&e.table==&m.beowulf_assets.children[0]&&e.alive;});};
  check(held()==1,"CreatePersistentObjects creates exactly one held chair");
  a.start(191);++f.attack_instance;for(unsigned n=0;n<400;++n)m.practice_tick();
  check(a.value(0x8b)!=0,"Chair toss switches to unarmed normals");
  auto chair=std::find_if(m.practice_effects.begin(),m.practice_effects.end(),[&](const auto& e){return e.root==&m.beowulf_assets&&e.table==&m.beowulf_assets.children[1]&&e.alive&&e.move==8;});
  check(chair!=m.practice_effects.end(),"Tossed chair lands in its native idle");
  f.practice_x=std::clamp(chair->x+(facing?-45.0f:45.0f),100.0f,1600.0f);
  m.practice_players[1].practice_x=facing?1600:100;
  for(unsigned n=0;n<3;++n)m.practice_tick();
  m.practice_keys_held[0]=384;m.practice_tick();m.practice_keys_held[0]=0;
  check(a.move==185||a.move==186,"Nearby two-kick input enters native chair pickup: "+std::to_string(a.move));
  for(unsigned n=0;n<90;++n)m.practice_tick();
  check(a.value(0x8b)==0&&held()==1,"Pickup restores the chair without duplicating its held entity");
  check(std::any_of(m.practice_effects.begin(),m.practice_effects.end(),[&](const auto& e){return e.root==&m.beowulf_assets&&e.table==&m.beowulf_assets.children[0]&&e.alive&&e.value(0xa4)!=0&&e.pose>1;}),"Picked-up chair returns to a visible attachment");
 }
 struct Finish{unsigned keys,first,last;};
 for(bool facing:{true,false})for(auto finish:{Finish{16,281,282},Finish{128,283,284},Finish{16|2,311,318},Finish{16|1,314,318},Finish{128|2,319,329},Finish{128|1,330,337}}){
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
  auto& f=m.practice_players[0];auto& a=*f.beowulf;f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?800:900;a.facing_right=facing;
  a.start(169);++f.attack_instance;
  for(unsigned n=0;n<100&&a.move!=267;++n)m.practice_tick();
  check(a.captured_target&&a.move==267,"Throw reaches controllable native grab mode");
  auto mask=finish.keys;if(!facing)mask=(mask&~3u)|((mask&1u)?2u:0u)|((mask&2u)?1u:0u);
  m.practice_keys_held[0]=mask;m.practice_tick();m.practice_keys_held[0]=0;
  check(a.move>=finish.first&&a.move<=finish.last,"Grab input selects native finisher: expected="+std::to_string(finish.first)+" got="+std::to_string(a.move));
  unsigned damage=0;for(unsigned n=0;n<800;++n){m.practice_tick();damage=std::max(damage,m.hud_stats[0].damage);}
  check(damage>0&&!a.captured_target&&!a.attacking()&&(a.move==21||a.move==22),"Grab finisher damages, releases and recovers: "+std::to_string(finish.first)+" end="+std::to_string(a.move));
 }
}
void hype(Menu& m){
 m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
 m.practice_players[0].practice_x=300;m.practice_players[1].practice_x=1300;
 m.practice_keys_held[0]=16;bool held=false;
 for(unsigned t=0;t<180;++t){m.practice_tick();held|=m.practice_players[0].beowulf->move==113;}
 auto& a=*m.practice_players[0].beowulf;
 check(held&&a.value(0x64)>1&&a.value(0x64)<=3,"Held LP uses native per-tick Hype gain and cap: "+std::to_string(a.value(0x64)));
 m.practice_keys_held[0]=0;for(unsigned t=0;t<100;++t)m.practice_tick();
 check(a.move==21,"Releasing held LP exits its native hold");
}
void snapshot(Menu& m){
 for(unsigned entry:{234u,191u,169u}){
 m.practice_keys_held={};m.practice_keys_previous={};
 m.begin_netplay_match(1234);m.practice_presentation={};auto& a=*m.practice_players[0].beowulf;a.animate(m.beowulf_assets);a.start(entry);
 for(unsigned t=0;t<55;++t)m.advance_netplay_frame({},false);
 const auto saved=m.capture_netplay_state();
 const auto observe=[&](){std::ostringstream o;auto s=m.capture_netplay_state();
  o<<s.next_entity<<','<<s.simulation.stage_tick<<','<<s.simulation.combat.tick<<','<<s.simulation.stats[0].damage<<','<<s.simulation.stats[1].life;
  auto& f=s.simulation.fighters[0];o<<','<<f.beowulf->move<<','<<f.beowulf->record<<','<<f.beowulf->ticks<<','<<f.beowulf->captured_target<<','<<f.beowulf->value(0x64)<<','<<f.beowulf->value(0x8b);
  for(auto& e:s.simulation.effects)o<<','<<e.entity_id<<','<<e.move<<','<<e.record<<','<<e.ticks<<','<<e.x<<','<<e.y;
  return o.str();};
 for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},false);const auto expected=observe();
 m.load_netplay_state(saved);for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},true);
 check(observe()==expected,"Beowulf rollback restores independent super, effects, entity IDs and damage state");
 m.network_mode=false;
 }
}
void teams(Menu& m){
 m.fighters[0]={character("filia"),character("beowulf")};m.locked_colors[0]={0,0};m.reset_practice();m.practice_presentation={};
 m.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==1 && !m.practice_teams[0].tag && m.practice_players[0].beowulf,"Tag activates Beowulf playback");
 for(unsigned n=0;n<650;++n)m.practice_tick();
 const auto& arrived=m.practice_players[0];
 check(arrived.beowulf->move==21&&arrived.jump_height==0&&arrived.beowulf->value(0xa4)==1&&arrived.beowulf->value(0xa5)==1&&arrived.beowulf->value(0xa6)>0,"Two-pass tag-in lands in visible playable idle: move="+std::to_string(arrived.beowulf->move));
 m.begin_practice_tag(0,0,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==0 && !m.practice_teams[0].tag && !m.practice_players[0].scripted(),"Beowulf tags back to Filia");
 m.reset_practice();m.practice_presentation={};m.practice_key('A',true);m.practice_key('X',true);m.practice_tick();m.practice_key('A',false);m.practice_key('X',false);
 auto& team=m.practice_teams[0];check(team.assist && team.assist->fighter.beowulf,"Beowulf assist uses own playback");bool acted=false;
 for(unsigned t=0;t<450 && team.assist;++t){m.practice_tick();acted|=team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting;}
 check(acted && !team.assist,"Beowulf assist acts and exits");
 m.fighters[0]={character("beowulf")};m.locked_colors[0]={0};m.reset_practice();
}
void chain(Menu& m,bool facing){
 m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.beowulf;
 f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?800:900;a.facing_right=facing;
 unsigned stage=0,max_hits=0;float launch=0;bool air_attack=false;
 for(unsigned t=0;t<500;++t){unsigned mask=0;
  if(!stage){mask=16;stage=1;}
  else if(!f.hit_stop && a.value(1)>0){
   if(stage==1){mask=64|4;stage=3;}
   else if(stage==3){mask=8|(facing?2u:1u);}
   else if(stage==4){mask=32;stage=5;}
  }
  if(stage==3 && f.jump_height>60){mask=16;stage=4;}
  m.practice_keys_held[0]=mask;m.practice_tick();
  max_hits=std::max(max_hits,m.hud_stats[0].hits);launch=std::max(launch,m.practice_players[1].jump_height);
  air_attack|=a.move>=149&&a.move<=164;
 }
 std::cout<<"CHAIN "<<stage<<" hits "<<max_hits<<" launch "<<launch<<" air "<<air_attack<<'\n';
 check(max_hits>=3 && launch>100 && air_attack,"Ground chain launches and follows with air normals");
}
void movement(Menu& m,bool facing){
 Menu::PracticeFighter f;f.practice_x=800;auto& a=f.beowulf.emplace();a.facing_right=facing;
 for(unsigned n=0;n<10;++n)tick(m,f,facing,0);
 check(facing?f.practice_x>800:f.practice_x<800,"Walk respects facing");
 tick(m,f,facing,6);for(unsigned n=0;n<20;++n)tick(m,f,facing);
 check(f.jump_height>0,"Jump rises");const auto before=f.jump_height;
 tick(m,f,facing,6);for(unsigned n=0;n<8;++n)tick(m,f,facing);
 check(f.jump_height>before,"Beowulf jump rises again");
 for(unsigned n=0;n<250;++n)tick(m,f,facing);
 check(f.jump_height==0 && a.move==21,"Jump/double jump lands and recovers to idle");
}
#ifdef _WIN32
void preview(Menu& m,const std::filesystem::path& output){
 skullgirls::SpriteArchive ui_win(m.game_data/"ui-win.gfs"),ui(m.game_data/"ui.gfs");
 m.combat_scene=skullgirls::read_combat_scene(ui_win.read("ui/Win/InGame.gbs"));
 for(unsigned i=0;i<2;++i)m.hud_atlas[i].load(ui_win.read("ui/Win/Textures/TA_1001_"+std::to_string(i+1)+".dds"));
 for(auto name:{"beowulf","parasoul"}){auto portrait=std::make_unique<Texture>();portrait->load(ui.read("ui/true_color_hud/super_"+std::string(name)+"01.dds"));m.super_portraits[character(name)]=std::move(portrait);}
 m.prepare_hud_portraits();m.prepare_combat_art();
 m.practice_background=std::make_unique<Texture>();m.practice_background->load(skullgirls::read_sprite_asset(m.game_data/"stages-textures2d.gfs","stages/textures/2D/stage_class_notes-alt.dds"),Texture::Encoding::stage_rgb);
 std::filesystem::create_directories(output);CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 for(auto move:{21u,112u,131u,151u,191u,199u,234u,237u,241u}){m.reset_practice();m.practice_presentation={};for(unsigned n=0;n<2;++n)m.practice_tick();auto& f=m.practice_players[0];f.beowulf->start(move);++f.attack_instance;
  if(move==151){f.jump_height=400;f.beowulf->state.variables[0x3b]=2.0f;}
  for(unsigned t=0;t<(move==234?100u:move==237?75u:move==241?140u:12u);++t){m.practice_tick();
   }

  Gdiplus::Bitmap canvas(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color(255,0,0,0));m.paint_practice(dc);}
  check(canvas.Save((output/(std::to_string(move)+".png")).c_str(),&png)==Gdiplus::Ok,"Render Beowulf preview");
 }
}
#endif
void basic(Menu& m,bool facing){
 for(bool armed:{true,false})for(unsigned stance=0;stance<3;++stance)for(unsigned button:{1u,2u,4u,8u,16u,32u}){
 Menu::PracticeFighter f;f.practice_x=800;f.beowulf.emplace().facing_right=facing;f.beowulf->state.variables[0x8b]=armed?0.0f:-1.0f;
 if(stance==2){f.beowulf->start(98);f.beowulf->state.variables[0x3b]=2.0f;f.jump_height=800;}
 tick(m,f,facing,stance==1?2:8,button);check(f.beowulf->attacking(),"Beowulf normal starts: "+std::to_string(button)+" got "+std::to_string(f.beowulf->move));
 for(unsigned t=0;t<400&&f.beowulf->attacking();++t)tick(m,f,facing);
 check(!f.beowulf->attacking(),"Normal recovers");}
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
 if(std::filesystem::path(argv[2])==L"--commands"){commands(*m,true);commands(*m,false);}else if(std::filesystem::path(argv[2])==L"--preview")preview(*m,"build/beowulf-check/preview");else throw std::invalid_argument("Unknown preview option");
#endif
#ifndef _WIN32
 throw std::invalid_argument("Preview output requires Windows");
#endif
}else{basic(*m,true);basic(*m,false);commands(*m,true);commands(*m,false);movement(*m,true);movement(*m,false);combat(*m,true);combat(*m,false);grabs(*m);chair_and_finishers(*m);hype(*m);snapshot(*m);teams(*m);chain(*m,true);chain(*m,false);std::cout<<"Beowulf movement, normals, specials, chair pickup, grab finishers, super meter/presentation, combat, throws, rollback and combo tests passed\n";}}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
