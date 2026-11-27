#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
namespace {
void check(bool ok,const std::string& m){if(!ok)throw std::runtime_error(m);}
int character(std::string_view n){return int(std::find(roster.begin(),roster.end(),n)-roster.begin());}
void setup(Menu& m){m.load_movement_frames();m.load_annie();m.load_parasoul();
 m.fighters[0]={character("annie")};m.fighters[1]={character("parasoul")};
 for(auto name:{"annie","parasoul"})m.palettes[character(name)].push_back({0,skullgirls::CharacterAssets(m.game_data,name).read_default_palette()});
 m.locked_colors[0]=m.locked_colors[1]={0};m.practice_width=1700;m.practice_height=900;m.practice_floor=64;m.practice_starts={700,1000};
 m.prepare_practice_bitmaps();m.load_practice_effects();m.prepare_combat_art();m.reset_practice();}
void tick(Menu& m,Menu::PracticeFighter& f,bool facing,unsigned dir=8,unsigned buttons=0){
 constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
 f.practice_left=dir>=3&&dir<=5;f.practice_right=dir==0||dir==1||dir==7;f.practice_down=dir>=1&&dir<=3;f.practice_up=dir>=5&&dir<=7;
 auto edges=buttons&~f.held_attack_buttons;f.pressed_punches=edges&7;f.pressed_kicks=edges>>3;f.held_attack_buttons=buttons;
 m.tick_annie(f,facing?1500:100,0,1700,facing);}
void commands(Menu& m,bool facing){
 struct Command{unsigned motion,buttons,first,last,cost;bool air=false;};
 for(auto c:{Command{0,1,196,202,0},{0,2,198,202,0},{0,4,200,202,0},
  {2,1,185,195,0},{2,2,186,195,0},{2,4,187,195,0},
  {3,1,203,212,0},{3,2,204,212,0},{3,4,205,212,0},
  {0,8,176,184,0,true},{0,16,177,184,0,true},{0,32,178,184,0,true},
  {0,3,224,233,1},{0,24,234,236,1},{3,24,215,216,2},{3,3,220,222,3},{5,24,237,237,3}}){
 for(bool enough:{true,false}){if(!c.cost&&!enough)continue;
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
  auto& f=m.practice_players[0];auto& a=*f.annie;f.practice_x=800;a.facing_right=facing;m.practice_players[1].practice_x=facing?1500:100;f.tension=enough?std::max(1u,c.cost):0;
  if(c.air){a.start(99);f.jump_height=800;a.state.variables[0x3b]=2.0f;}
  for(unsigned n=0;n<7;++n)tick(m,f,facing);
  if(c.motion==0){tick(m,f,facing,2);tick(m,f,facing,1);tick(m,f,facing,0,c.buttons);}
  if(c.motion==2){tick(m,f,facing,0);tick(m,f,facing,2);tick(m,f,facing,1,c.buttons);}
  if(c.motion==3){tick(m,f,facing,2);tick(m,f,facing,3);tick(m,f,facing,4,c.buttons);}
  if(c.motion==5){tick(m,f,facing,2);tick(m,f,facing,8);tick(m,f,facing,2,c.buttons);}
  check((a.move>=c.first&&a.move<=c.last)==enough,"Command entry expected="+std::to_string(c.first)+" got="+std::to_string(a.move));
  if(enough){for(unsigned n=0;n<1000&&a.attacking();++n)tick(m,f,facing);
   check(!a.attacking(),"Command recovery expected="+std::to_string(c.first)+" got="+std::to_string(a.move));}
 }}
}
void star_and_throws(Menu& m){
 for(unsigned move:{156u,165u,237u}){
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};auto& f=m.practice_players[0];auto& a=*f.annie;
  f.practice_x=700;m.practice_players[1].practice_x=800;a.start(move);++f.attack_instance;
  if(move==165){f.jump_height=m.practice_players[1].jump_height=300;a.state.variables[0x3b]=2.0f;}
  bool caught=false,manager=false;float gauge=0;unsigned damage=0;
  for(unsigned n=0;n<(move==237?4000u:1000u);++n){m.practice_tick();caught|=a.captured_target;gauge=std::max(gauge,a.value(0x63));damage=std::max(damage,m.hud_stats[0].damage);
   for(auto& e:m.practice_effects)manager|=e.root==&m.annie_assets&&e.table==&m.annie_assets.children[23]&&e.alive;
  }
  std::cout<<"PATH "<<move<<" caught="<<caught<<" damage="<<damage<<" manager="<<manager<<" gauge="<<gauge<<" endgauge="<<a.value(0x63)<<" meter="<<f.tension<<" fraction="<<a.meter_remainder<<" end="<<a.move<<'\n';
  if(move!=237)check(caught&&damage>0&&!a.captured_target&&!a.attacking(),"Throw damage and recovery");
  else check(manager&&gauge>0&&a.value(0x63)==0,"Native Star Power manager spawns, counts down and expires");
 }
}
void combat(Menu& m){
 for(bool facing:{true,false})for(unsigned move:{118u,120u,123u,125u,128u,132u,133u,135u,136u,137u,138u,144u,145u,146u,147u,148u,149u,185u,186u,187u,196u,198u,200u,203u,204u,205u,224u,234u}) {
  m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
  auto& f=m.practice_players[0];auto& a=*f.annie;f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?800:900;if(move>=196&&move<=200)m.practice_players[1].practice_x=f.practice_x+(facing?330.0f:-330.0f);if(move>=185&&move<=187)m.practice_players[1].practice_x=f.practice_x+(facing?45.0f:-45.0f);a.facing_right=facing;a.start(move);++f.attack_instance;
  if(move>=144&&move<=149){f.jump_height=300;a.state.variables[0x3b]=2.0f;m.practice_players[1].jump_height=300;}
  unsigned damage=0;for(unsigned t=0;t<750;++t){m.practice_tick();damage=std::max(damage,m.hud_stats[0].damage);}
  check(damage>0,"Annie combat damage move="+std::to_string(move)+" facing="+std::to_string(facing));
  check(!a.attacking(),"Annie combat recovery move="+std::to_string(move)+" end="+std::to_string(a.move));
 }
}
void snapshot(Menu& m){
 for(unsigned entry:{224u,237u,156u}){
 m.practice_keys_held={};m.practice_keys_previous={};
 m.begin_netplay_match(1234);m.practice_presentation={};auto& a=*m.practice_players[0].annie;a.animate(m.annie_assets);a.start(entry);
 for(unsigned t=0;t<55;++t)m.advance_netplay_frame({},false);
 const auto saved=m.capture_netplay_state();
 const auto observe=[&](){std::ostringstream o;auto s=m.capture_netplay_state();
  o<<s.next_entity<<','<<s.simulation.stage_tick<<','<<s.simulation.combat.tick<<','<<s.simulation.stats[0].damage<<','<<s.simulation.stats[1].life;
  auto& f=s.simulation.fighters[0];o<<','<<f.annie->move<<','<<f.annie->record<<','<<f.annie->ticks<<','<<f.annie->captured_target<<','<<f.annie->value(0x63)<<','<<f.annie->meter_remainder;
  for(auto& e:s.simulation.effects)o<<','<<e.entity_id<<','<<e.move<<','<<e.record<<','<<e.ticks<<','<<e.x<<','<<e.y;
  return o.str();};
 for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},false);const auto expected=observe();
 m.load_netplay_state(saved);for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},true);
 check(observe()==expected,"Annie rollback restores independent super, effects, entity IDs and damage state");
 m.network_mode=false;
 }
}
void teams(Menu& m){
 m.fighters[0]={character("filia"),character("annie")};m.locked_colors[0]={0,0};m.reset_practice();m.practice_presentation={};
 m.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==1 && !m.practice_teams[0].tag && m.practice_players[0].annie,"Tag activates Annie playback");
 for(unsigned n=0;n<650;++n)m.practice_tick();
 const auto& arrived=m.practice_players[0];
 check(arrived.annie->move==21&&arrived.jump_height==0&&arrived.annie->value(0xa4)==1&&arrived.annie->value(0xa5)==1&&arrived.annie->value(0xa6)>0,"Two-pass tag-in lands in visible playable idle: move="+std::to_string(arrived.annie->move));
 m.begin_practice_tag(0,0,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==0 && !m.practice_teams[0].tag && !m.practice_players[0].scripted(),"Annie tags back to Filia");
 m.reset_practice();m.practice_presentation={};m.practice_key('A',true);m.practice_key('X',true);m.practice_tick();m.practice_key('A',false);m.practice_key('X',false);
 auto& team=m.practice_teams[0];check(team.assist && team.assist->fighter.annie,"Annie assist uses own playback");bool acted=false;
 for(unsigned t=0;t<450 && team.assist;++t){m.practice_tick();acted|=team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting;}
 check(acted && !team.assist,"Annie assist acts and exits");
 m.fighters[0]={character("annie")};m.locked_colors[0]={0};m.reset_practice();
}
void chain(Menu& m,bool facing){
 m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};auto& f=m.practice_players[0];auto& a=*f.annie;
 f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?800:900;a.facing_right=facing;
 unsigned stage=0,max_hits=0,previous_move=999;float launch=0,owner_rise=0;bool air_attack=false;std::ostringstream trace;
 for(unsigned t=0;t<500;++t){unsigned mask=0;
  if(!stage){mask=16;stage=1;}
  else if(!f.hit_stop&&a.value(1)>0){
   if(stage==1){mask=64;stage=2;}
   else if(stage==2){mask=64;stage=3;}
   else if(stage==4){if(t%2==0)mask=32;if(a.move==146)stage=5;}
  }
  if(stage==3&&f.jump_height>0){if(t%2==0)mask=16;if(a.move>=144&&a.move<=149)stage=4;}
  m.practice_keys_held[0]=mask;m.practice_tick();if(a.move!=previous_move){trace<<" t="<<t<<" move="<<a.move<<" h="<<f.jump_height<<" vy="<<f.jump_velocity<<" hit="<<a.value(1);previous_move=a.move;}max_hits=std::max(max_hits,m.hud_stats[0].hits);launch=std::max(launch,m.practice_players[1].jump_height);owner_rise=std::max(owner_rise,f.jump_height);air_attack|=a.move>=144&&a.move<=149;
 }
 std::cout<<"CHAIN "<<stage<<" hits "<<max_hits<<" launch "<<launch<<" air "<<air_attack<<'\n';
 check(max_hits>=2&&owner_rise>60&&a.move==21&&f.jump_height==0,"Ground chain and native HP follow-up rise, land and recover"+trace.str());
}
void fractional_meter(){
 skullgirls::AnniePlayback a;unsigned meter=2;a.meter_remainder=99.5f;
 for(unsigned n=0;n<120;++n){a.sample(8,0,0,true,meter);a.publish_meter(meter);}
 check(meter==2&&a.meter_remainder==99.5f,"Fractional meter survives whole-bar host sampling");
 a.state.team_variables[0x42]=299.25f;a.publish_meter(meter);a.sample(8,0,0,true,meter);
 check(meter==2&&a.value(0x42)==299.25f,"Fractional drain preserves the remaining meter");
}
void movement(Menu& m,bool facing){
 Menu::PracticeFighter f;f.practice_x=800;auto& a=f.annie.emplace();a.facing_right=facing;
 for(unsigned n=0;n<10;++n)tick(m,f,facing,0);
 check(facing?f.practice_x>800:f.practice_x<800,"Walk respects facing");
 tick(m,f,facing,6);for(unsigned n=0;n<20;++n)tick(m,f,facing);
 check(f.jump_height>0,"Jump rises");const auto before=f.jump_height;
 tick(m,f,facing,6);for(unsigned n=0;n<8;++n)tick(m,f,facing);
 check(f.jump_height>before,"Annie jump rises again");
 for(unsigned n=0;n<250;++n)tick(m,f,facing);
 check(f.jump_height==0 && a.move==21,"Jump/double jump lands and recovers to idle");
}
#ifdef _WIN32
void preview(Menu& m,const std::filesystem::path& output){
 skullgirls::SpriteArchive ui_win(m.game_data/"ui-win.gfs"),ui(m.game_data/"ui.gfs");
 m.combat_scene=skullgirls::read_combat_scene(ui_win.read("ui/Win/InGame.gbs"));
 for(unsigned i=0;i<2;++i)m.hud_atlas[i].load(ui_win.read("ui/Win/Textures/TA_1001_"+std::to_string(i+1)+".dds"));
 for(auto name:{"annie","parasoul"}){auto portrait=std::make_unique<Texture>();portrait->load(ui.read("ui/true_color_hud/super_"+std::string(name)+"01.dds"));m.super_portraits[character(name)]=std::move(portrait);}
 m.prepare_hud_portraits();m.prepare_combat_art();
 m.practice_background=std::make_unique<Texture>();m.practice_background->load(skullgirls::read_sprite_asset(m.game_data/"stages-textures2d.gfs","stages/textures/2D/stage_class_notes-alt.dds"),Texture::Encoding::stage_rgb);
 std::filesystem::create_directories(output);CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 for(auto move:{21u,118u,137u,146u,202u,191u,224u,237u,220u}){m.reset_practice();m.practice_presentation={};for(unsigned n=0;n<2;++n)m.practice_tick();auto& f=m.practice_players[0];f.annie->start(move);++f.attack_instance;
  if(move==146){f.jump_height=400;f.annie->state.variables[0x3b]=2.0f;}
  for(unsigned t=0;t<(move==224?75u:move==237?85u:move==220?140u:12u);++t){m.practice_tick();
   }

  Gdiplus::Bitmap canvas(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color(255,0,0,0));m.paint_practice(dc);}
  check(canvas.Save((output/(std::to_string(move)+".png")).c_str(),&png)==Gdiplus::Ok,"Render Annie preview");
 }
}
#endif
void basic(Menu& m,bool facing){
 for(unsigned stance=0;stance<3;++stance)for(unsigned button:{1u,2u,4u,8u,16u,32u}){
 Menu::PracticeFighter f;f.practice_x=800;f.annie.emplace().facing_right=facing;
 if(stance==2){f.annie->start(98);f.annie->state.variables[0x3b]=2.0f;f.jump_height=800;}
 tick(m,f,facing,stance==1?2:8,button);check(f.annie->attacking(),"Annie normal starts: "+std::to_string(button)+" got "+std::to_string(f.annie->move));
 for(unsigned t=0;t<400&&f.annie->attacking();++t)tick(m,f,facing);
 check(!f.annie->attacking(),"Normal recovers");}
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
 if(std::filesystem::path(argv[2])==L"--commands"){commands(*m,true);commands(*m,false);}else if(std::filesystem::path(argv[2])==L"--preview")preview(*m,"build/annie-check/preview");else throw std::invalid_argument("Unknown preview option");
#endif
#ifndef _WIN32
 throw std::invalid_argument("Preview output requires Windows");
#endif
}else{basic(*m,true);basic(*m,false);movement(*m,true);movement(*m,false);commands(*m,true);commands(*m,false);star_and_throws(*m);combat(*m);snapshot(*m);teams(*m);chain(*m,true);chain(*m,false);fractional_meter();std::cout<<"Annie tests passed\n";}}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
