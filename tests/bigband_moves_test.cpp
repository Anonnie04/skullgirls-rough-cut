#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
namespace {
void check(bool ok,const std::string& m){if(!ok)throw std::runtime_error(m);}
int character(std::string_view n){return int(std::find(roster.begin(),roster.end(),n)-roster.begin());}
void setup(Menu& m){m.load_movement_frames();m.load_bigband();m.load_parasoul();
 m.fighters[0]={character("bigband")};m.fighters[1]={character("parasoul")};
 for(auto name:{"bigband","parasoul"})m.palettes[character(name)].push_back({0,skullgirls::CharacterAssets(m.game_data,name).read_default_palette()});
 m.locked_colors[0]=m.locked_colors[1]={0};m.practice_width=1700;m.practice_height=2000;m.practice_floor=64;m.practice_starts={700,1000};
 m.prepare_practice_bitmaps();m.load_practice_effects();m.prepare_combat_art();m.reset_practice();}
void tick(Menu& m,Menu::PracticeFighter& f,bool facing,unsigned dir=8,unsigned buttons=0){
 constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
 f.practice_left=dir>=3&&dir<=5;f.practice_right=dir==0||dir==1||dir==7;f.practice_down=dir>=1&&dir<=3;f.practice_up=dir>=5&&dir<=7;
 auto edges=buttons&~f.held_attack_buttons;f.pressed_punches=edges&7;f.pressed_kicks=edges>>3;f.held_attack_buttons=buttons;
 m.tick_bigband(f,facing?1500:100,0,1700,facing);}
void commands(Menu& m,bool facing){
 struct Command{unsigned motion,buttons,first,last,cost;bool air=false;};
 for(const auto c:{Command{2,1,160,160,0},{2,2,161,161,0},{2,4,162,165,0},
  {3,8,166,171,0},{3,16,166,171,0},{3,32,166,171,0},
  {3,8,173,173,0,true},{3,16,174,174,0,true},{3,32,175,176,0,true},
  {4,1,177,181,0},{4,2,177,181,0},{4,4,177,181,0},
  {4,8,188,192,0},{4,16,188,192,0},{4,32,188,192,0},
  {0,3,203,204,1},{0,24,210,211,1,true},{3,3,218,218,3}}){
  for(bool enough:{true,false}){
   if(!c.cost&&!enough)continue;
   m.reset_practice();m.practice_presentation={};m.practice_keys_held={};m.practice_keys_previous={};
   auto& f=m.practice_players[0];auto& a=*f.bigband;f.practice_x=800;a.facing_right=facing;
   m.practice_players[1].practice_x=facing?1400:200;f.tension=enough?std::max(1u,c.cost):0;
   if(c.air){a.start(98);f.jump_height=800;a.state.variables[0x3b]=2.0f;}
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
 for(unsigned move:{111u,113u,114u,117u,119u,120u,121u,123u,124u,126u,127u,128u,129u,130u,131u,132u,133u,135u,160u,161u,162u,166u,167u,168u,173u,174u,176u,177u,178u,179u,203u,210u,218u}){
  m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.bigband;
  f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?820:880;
  a.facing_right=facing;a.start(move);++f.attack_instance;
  bool air=(move>=129&&move<=138)||move==173||move==174||move==176||move==210;
  if(air){f.jump_height=300;a.state.variables[0x3b]=2.0f;m.practice_players[1].jump_height=300;}
  if(move==135){m.practice_players[1].jump_height=0;}
  unsigned hits=0;std::ostringstream trace;unsigned damage=0;bool freeze=false;std::set<unsigned> children;
  for(unsigned t=0;t<1100;++t){m.practice_tick();damage=std::max(damage,m.hud_stats[0].damage);hits=std::max(hits,m.hud_stats[0].hits);freeze|=m.practice_super_frozen();
   for(auto& e:m.practice_effects)if(e.root==&m.bigband_assets){
    if(move>=156&&move<=160&&e.table==&m.bigband_assets.children[51]&&t%10==0&&t<100)trace<<" tick="<<t<<" x="<<e.x<<" y="<<e.y<<" move="<<e.move<<" hit="<<(e.hit?e.hit->damage:0)<<" alive="<<e.alive;
    children.insert(unsigned(e.table-m.bigband_assets.children.data()));}
  }
  check(damage>0,"Combat damages Parasoul: "+std::to_string(move)+" facing="+std::to_string(facing)+trace.str());
  check(!a.attacking(),"Combat move recovers: "+std::to_string(move));
  check(m.practice_players[1].throw_phase!=Menu::PracticeFighter::ThrowPhase::grabbed,"Special releases opponent: "+std::to_string(move));
  if(move==203)check(hits>=2,"Horncrush executes multiple authored impacts");
  if(move==203||move==210||move==218)check(freeze && f.tension==(move==218?2u:4u),"Blockbuster has freeze and one meter debit");

 }
}
void grabs(Menu& m){
 for(unsigned move:{143u,151u,194u,195u}){
  m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.bigband;
  f.practice_x=700;m.practice_players[1].practice_x=800;a.start(move);++f.attack_instance;
  if(move==151){f.jump_height=300;a.state.variables[0x3b]=2.0f;}
  if(move==151||move==194)m.practice_players[1].jump_height=300;
  bool caught=false;unsigned damage=0;
  for(unsigned n=0;n<1000;++n){m.practice_tick();caught|=a.captured_target;damage=std::max(damage,m.hud_stats[0].damage);}
  std::cout<<"GRAB "<<move<<" caught "<<caught<<" damage "<<damage<<" end "<<a.move<<" captured "<<a.captured_target<<'\n';
  check(caught && damage>0 && !a.captured_target && !a.attacking(),"Throw captures, damages, releases and recovers: "+std::to_string(move));
 }
}
void snapshot(Menu& m){
 m.begin_netplay_match(1234);m.practice_presentation={};auto& a=*m.practice_players[0].bigband;a.start(203);
 for(unsigned t=0;t<55;++t)m.advance_netplay_frame({},false);
 const auto saved=m.capture_netplay_state();
 const auto observe=[&](){std::ostringstream o;auto s=m.capture_netplay_state();
  o<<s.next_entity<<','<<s.simulation.stage_tick<<','<<s.simulation.combat.tick<<','<<s.simulation.stats[0].damage<<','<<s.simulation.stats[1].life;
  auto& f=s.simulation.fighters[0];o<<','<<f.bigband->move<<','<<f.bigband->record<<','<<f.bigband->ticks;
  for(auto& e:s.simulation.effects)o<<','<<e.entity_id<<','<<e.move<<','<<e.record<<','<<e.ticks<<','<<e.x<<','<<e.y;
  return o.str();};
 for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},false);const auto expected=observe();
 m.load_netplay_state(saved);for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},true);
 check(observe()==expected,"Big Band rollback restores independent super, effects, entity IDs and damage state");
 m.network_mode=false;
}
void teams(Menu& m){
 m.fighters[0]={character("filia"),character("bigband")};m.locked_colors[0]={0,0};m.reset_practice();m.practice_presentation={};
 m.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==1 && !m.practice_teams[0].tag && m.practice_players[0].bigband,"Tag activates Big Band playback");
 for(unsigned n=0;n<650;++n)m.practice_tick();
 const auto& arrived=m.practice_players[0];
 check(arrived.bigband->move==21&&arrived.jump_height==0&&arrived.bigband->value(0xa4)==1&&arrived.bigband->value(0xa5)==1&&arrived.bigband->value(0xa6)>0,"Two-pass tag-in lands in visible playable idle: move="+std::to_string(arrived.bigband->move));
 m.begin_practice_tag(0,0,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==0 && !m.practice_teams[0].tag && !m.practice_players[0].scripted(),"Big Band tags back to Filia");
 m.reset_practice();m.practice_presentation={};m.practice_key('A',true);m.practice_key('X',true);m.practice_tick();m.practice_key('A',false);m.practice_key('X',false);
 auto& team=m.practice_teams[0];check(team.assist && team.assist->fighter.bigband,"Big Band assist uses own playback");bool acted=false;
 for(unsigned t=0;t<450 && team.assist;++t){m.practice_tick();acted|=team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting;}
 check(acted && !team.assist,"Big Band assist acts and exits");
 m.fighters[0]={character("bigband")};m.locked_colors[0]={0};m.reset_practice();
}
void chain(Menu& m,bool facing){
 m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.bigband;
 f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?800:900;a.facing_right=facing;
 unsigned stage=0,max_hits=0;float launch=0;bool air_attack=false;
 for(unsigned t=0;t<500;++t){unsigned mask=0;
  if(!stage){mask=16;stage=1;}
  else if(!f.hit_stop && a.value(1)>0){
   if(stage==1){mask=256;stage=3;}
   else if(stage==3){mask=8|(facing?2u:1u);}
   else if(stage==4){mask=32;stage=5;}
  }
  if(stage==3 && f.jump_height>60){mask=16;stage=4;}
  m.practice_keys_held[0]=mask;m.practice_tick();
  max_hits=std::max(max_hits,m.hud_stats[0].hits);launch=std::max(launch,m.practice_players[1].jump_height);
  air_attack|=a.move>=129&&a.move<=138;
 }
 std::cout<<"CHAIN "<<stage<<" hits "<<max_hits<<" launch "<<launch<<" air "<<air_attack<<'\n';
 check(max_hits>=3 && launch>100 && air_attack,"Ground chain launches and follows with air normals");
}
void charge_and_armor(Menu& m){
 for(bool facing:{true,false}){
  auto f=std::make_unique<Menu::PracticeFighter>();f->practice_x=800;
  f->bigband.emplace().facing_right=facing;
  for(unsigned n=0;n<7;++n)tick(m,*f,facing);
  for(unsigned n=0;n<5;++n)tick(m,*f,facing,4);
  tick(m,*f,facing,0,1);
  check(f->bigband->move<177||f->bigband->move>198,"Short back hold cannot trigger a charge special");
 }
 auto f=std::make_unique<Menu::PracticeFighter>();auto& a=f->bigband.emplace();
 a.start(203);a.animate(m.bigband_assets);
 check(a.value(0x7b)>0,"Super startup authors its armor budget");
 const auto move=a.move;const auto budget=a.value(0x7b);
 skullgirls::NormalHit strike{};strike.height_mask=1;strike.stop=4;
 f->take_hit(strike,false,-1);
 check(a.move==move&&f->hurt_group<0&&a.value(0x7b)==budget-1&&f->hit_stop==4,"Armored strike preserves the authored move and consumes one armor hit");
 a.report_contact(false);a.report_contact(true);
 check(a.value(1)==1&&a.value(2)==1&&a.value(3)==2,"Contact serial counts both hits and blocks");
}
void movement(Menu& m,bool facing){
 Menu::PracticeFighter f;f.practice_x=800;auto& a=f.bigband.emplace();a.facing_right=facing;
 for(unsigned n=0;n<10;++n)tick(m,f,facing,0);
 check(facing?f.practice_x>800:f.practice_x<800,"Walk respects facing");
 tick(m,f,facing,6);for(unsigned n=0;n<20;++n)tick(m,f,facing);
 check(f.jump_height>0,"Jump rises");const auto before=f.jump_height;
 tick(m,f,facing,6);for(unsigned n=0;n<8;++n)tick(m,f,facing);
 check(f.jump_height>before,"Big Band jump rises again");
 for(unsigned n=0;n<250;++n)tick(m,f,facing);
 check(f.jump_height==0 && a.move==21,"Jump/double jump lands and recovers to idle");
}
#ifdef _WIN32
void preview(Menu& m,const std::filesystem::path& output){
 skullgirls::SpriteArchive ui_win(m.game_data/"ui-win.gfs"),ui(m.game_data/"ui.gfs");
 m.combat_scene=skullgirls::read_combat_scene(ui_win.read("ui/Win/InGame.gbs"));
 for(unsigned i=0;i<2;++i)m.hud_atlas[i].load(ui_win.read("ui/Win/Textures/TA_1001_"+std::to_string(i+1)+".dds"));
 for(auto name:{"bigband","parasoul"}){auto portrait=std::make_unique<Texture>();portrait->load(ui.read("ui/true_color_hud/super_"+std::string(name)+"01.dds"));m.super_portraits[character(name)]=std::move(portrait);}
 m.prepare_hud_portraits();m.prepare_combat_art();
 m.practice_background=std::make_unique<Texture>();m.practice_background->load(skullgirls::read_sprite_asset(m.game_data/"stages-textures2d.gfs","stages/textures/2D/stage_class_notes-alt.dds"),Texture::Encoding::stage_rgb);
 std::filesystem::create_directories(output);CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 for(auto move:{111u,127u,130u,177u,203u,210u,218u}){m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];f.bigband->start(move);++f.attack_instance;
  if(move==130){f.jump_height=400;f.bigband->state.variables[0x3b]=2.0f;}
  for(unsigned t=0;t<(move==203?65u:move==210?75u:move==218?140u:12u);++t){m.practice_tick();
   if(move==203&&f.bigband->move==204&&f.practice_x>300&&f.practice_x<1300)break;}

  Gdiplus::Bitmap canvas(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color(255,0,0,0));m.paint_practice(dc);}
  check(canvas.Save((output/(std::to_string(move)+".png")).c_str(),&png)==Gdiplus::Ok,"Render Big Band preview");
 }
}
#endif
void basic(Menu& m,bool facing){
 for(unsigned stance=0;stance<3;++stance)for(unsigned button:{1u,2u,4u,8u,16u,32u}){
 Menu::PracticeFighter f;f.practice_x=800;f.bigband.emplace().facing_right=facing;
 if(stance==2){f.bigband->start(98);f.bigband->state.variables[0x3b]=2.0f;f.jump_height=800;}
 tick(m,f,facing,stance==1?2:8,button);check(f.bigband->attacking(),"Big Band normal starts: "+std::to_string(button)+" got "+std::to_string(f.bigband->move));
 for(unsigned t=0;t<400&&f.bigband->attacking();++t)tick(m,f,facing);
 check(!f.bigband->attacking(),"Normal recovers");}
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
 if(std::filesystem::path(argv[2])==L"--commands"){commands(*m,true);commands(*m,false);}else if(std::filesystem::path(argv[2])==L"--preview")preview(*m,"build/bigband-check/preview");else throw std::invalid_argument("Unknown preview option");
#endif
#ifndef _WIN32
 throw std::invalid_argument("Preview output requires Windows");
#endif
}else{basic(*m,true);basic(*m,false);commands(*m,true);commands(*m,false);charge_and_armor(*m);movement(*m,true);movement(*m,false);combat(*m,true);combat(*m,false);grabs(*m);snapshot(*m);teams(*m);chain(*m,true);chain(*m,false);std::cout<<"Big Band movement, normals, specials, armor, super meter/presentation, combat, throws, rollback and combo tests passed\n";}}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
