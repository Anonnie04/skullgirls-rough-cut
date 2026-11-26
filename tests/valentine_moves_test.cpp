#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
namespace {
void check(bool ok,const std::string& m){if(!ok)throw std::runtime_error(m);}
int character(std::string_view n){return int(std::find(roster.begin(),roster.end(),n)-roster.begin());}
void setup(Menu& m){m.load_movement_frames();m.load_valentine();m.load_parasoul();
 m.fighters[0]={character("valentine")};m.fighters[1]={character("parasoul")};
 for(auto name:{"valentine","parasoul"})m.palettes[character(name)].push_back({0,skullgirls::CharacterAssets(m.game_data,name).read_default_palette()});
 m.locked_colors[0]=m.locked_colors[1]={0};m.practice_width=1700;m.practice_height=2000;m.practice_floor=64;m.practice_starts={700,1000};
 m.prepare_practice_bitmaps();m.load_practice_effects();m.prepare_combat_art();m.reset_practice();}
void tick(Menu& m,Menu::PracticeFighter& f,bool facing,unsigned dir=8,unsigned buttons=0){
 constexpr unsigned mirror[]{4,3,2,1,0,7,6,5,8};if(!facing)dir=mirror[dir];
 f.practice_left=dir>=3&&dir<=5;f.practice_right=dir==0||dir==1||dir==7;f.practice_down=dir>=1&&dir<=3;f.practice_up=dir>=5&&dir<=7;
 auto edges=buttons&~f.held_attack_buttons;f.pressed_punches=edges&7;f.pressed_kicks=edges>>3;f.held_attack_buttons=buttons;
 m.tick_valentine(f,facing?1500:100,0,1700,facing);}
void commands(Menu& m,bool facing){
 struct Command {std::initializer_list<unsigned> dirs;unsigned button,entry;bool air=false;};
 for(const auto& c:{Command{{2,1,0},1,170},{{2,1,0},2,171},{{2,1,0},4,172},
  {{2,1,0},8,198},{{2,1,0},16,199},{{2,1,0},32,200},
  {{2,3,4},1,183},{{2,3,4},2,184},{{2,3,4},4,185},
  {{2,1,0},3,214},{{2,1,0},24,219},
  {{2,1,0},1,177,true},{{2,1,0},8,204,true},{{2,1,0},3,215,true},{{2,1,0},24,223,true}}){
  Menu::PracticeFighter f;f.practice_x=800;f.valentine.emplace().facing_right=facing;
  if(c.air){f.valentine->start(98);f.valentine->state.variables[0x3b]=2.0f;f.jump_height=800;}
  for(auto d=c.dirs.begin();d!=c.dirs.end();++d)tick(m,f,facing,*d,d+1==c.dirs.end()?c.button:0);
  const auto expected=c.entry>=170&&c.entry<=172?175u:c.entry>=177&&c.entry<=179?181u:
   c.entry>=198&&c.entry<=200?201u:c.entry>=204&&c.entry<=206?207u:c.entry;
  check(f.valentine->move==expected,"Command "+std::to_string(c.entry)+" got "+std::to_string(f.valentine->move));
  for(unsigned n=0;n<1200&&f.valentine->attacking();++n)tick(m,f,facing);
  check(!f.valentine->attacking(),"Command recovers "+std::to_string(c.entry));
  check(f.tension==(c.entry>=214?4u:5u),"Special/Blockbuster spends its authored meter exactly once");
 }
}
void combat(Menu& m,bool facing){
 for(unsigned move:{119u,127u,128u,130u,131u,132u,135u,136u,138u,139u,140u,141u,142u,143u,144u,145u,146u,147u,170u,177u,198u,204u,214u,215u,219u,223u}){
  m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.valentine;
  f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?820:880;
  a.facing_right=facing;a.start(move);++f.attack_instance;
  bool air=(move>=142&&move<=147)||move==177||move==204||move==215||move==223;
  if(air){f.jump_height=move==143?150.0f:300.0f;a.state.variables[0x3b]=2.0f;m.practice_players[1].jump_height=move==143?0.0f:300.0f;}
  unsigned damage=0;bool freeze=false;std::set<unsigned> children;
  for(unsigned t=0;t<450;++t){m.practice_tick();damage=std::max(damage,m.hud_stats[0].damage);freeze|=m.practice_super_frozen();
   for(auto& e:m.practice_effects)if(e.root==&m.valentine_assets)
    children.insert(unsigned(e.table-m.valentine_assets.children.data()));
  }
  check(damage>0,"Combat damages Parasoul: "+std::to_string(move));
  check(!a.attacking(),"Combat move recovers: "+std::to_string(move));
  if(move>=214)check(freeze && f.tension==4,"Blockbuster has freeze and one meter debit");
  if(move==170||move==177)check(children.count(34),"Dead Cross creates authored projectile");
 }
}
void grabs(Menu& m){
 for(unsigned move:{152u,161u}){
  m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.valentine;
  f.practice_x=700;m.practice_players[1].practice_x=800;a.start(move);++f.attack_instance;
  if(move==161){f.jump_height=300;m.practice_players[1].jump_height=300;a.state.variables[0x3b]=2.0f;}
  bool caught=false;unsigned damage=0;
  for(unsigned n=0;n<1000;++n){m.practice_tick();caught|=a.captured_target;damage=std::max(damage,m.hud_stats[0].damage);}
  std::cout<<"GRAB "<<move<<" caught "<<caught<<" damage "<<damage<<" end "<<a.move<<" captured "<<a.captured_target<<'\n';
  check(caught && damage>0 && !a.captured_target && !a.attacking(),"Throw captures, damages, releases and recovers: "+std::to_string(move));
 }
}
void vials(Menu& m){
 for(unsigned type=0;type<3;++type){m.reset_practice();m.practice_presentation={};m.practice_keys_held={};auto& f=m.practice_players[0];auto& a=*f.valentine;
  for(unsigned level=1;level<=3;++level){
   for(unsigned mask:{4u,5u,1u|(16u<<type)}){m.practice_keys_held[0]=mask;m.practice_tick();}
   m.practice_keys_held[0]=16u<<type;
   for(unsigned n=0;n<400&&a.attacking();++n)m.practice_tick();
   m.practice_keys_held[0]=0;for(unsigned n=0;n<4;++n)m.practice_tick();
   check(a.value(0x63)==float(level),"Vial level: type="+std::to_string(type)+" expected="+std::to_string(level)+" got="+std::to_string(a.value(0x63)));}
  for(unsigned mask:{4u,6u,2u|16u}){m.practice_keys_held[0]=mask;m.practice_tick();}
  // Holding the punch through the authored decision selects the loaded syringe.
  m.practice_keys_held[0]=16;for(unsigned n=0;n<40 && a.move!=176;++n)m.practice_tick();
  check(a.move==176,"Loaded vial changes Dead Cross into syringe toss: got="+std::to_string(a.move));m.practice_keys_held[0]=0;
 }
}
void snapshot(Menu& m){
 m.begin_netplay_match(1234);m.practice_presentation={};auto& a=*m.practice_players[0].valentine;a.start(214);
 for(unsigned t=0;t<55;++t)m.advance_netplay_frame({},false);
 const auto saved=m.capture_netplay_state();
 const auto observe=[&](){std::ostringstream o;auto s=m.capture_netplay_state();
  o<<s.next_entity<<','<<s.simulation.stage_tick<<','<<s.simulation.combat.tick<<','<<s.simulation.stats[0].damage<<','<<s.simulation.stats[1].life;
  auto& f=s.simulation.fighters[0];o<<','<<f.valentine->move<<','<<f.valentine->record<<','<<f.valentine->ticks;
  for(auto& e:s.simulation.effects)o<<','<<e.entity_id<<','<<e.move<<','<<e.record<<','<<e.ticks<<','<<e.x<<','<<e.y;
  return o.str();};
 for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},false);const auto expected=observe();
 m.load_netplay_state(saved);for(unsigned t=0;t<120;++t)m.advance_netplay_frame({},true);
 check(observe()==expected,"Valentine rollback restores independent super, effects, entity IDs and damage state");
 m.network_mode=false;
}
void teams(Menu& m){
 m.fighters[0]={character("filia"),character("valentine")};m.locked_colors[0]={0,0};m.reset_practice();m.practice_presentation={};
 m.begin_practice_tag(0,1,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==1 && !m.practice_teams[0].tag && m.practice_players[0].valentine,"Tag activates Valentine playback");
 m.begin_practice_tag(0,0,Menu::PracticeTag::Kind::regular);
 for(unsigned t=0;t<400 && m.practice_teams[0].tag;++t)m.practice_tick();
 check(m.practice_teams[0].active_slot==0 && !m.practice_teams[0].tag && !m.practice_players[0].scripted(),"Valentine tags back to Filia");
 m.reset_practice();m.practice_presentation={};m.practice_key('A',true);m.practice_key('X',true);m.practice_tick();m.practice_key('A',false);m.practice_key('X',false);
 auto& team=m.practice_teams[0];check(team.assist && team.assist->fighter.valentine,"Valentine assist uses own playback");bool acted=false;
 for(unsigned t=0;t<450 && team.assist;++t){m.practice_tick();acted|=team.assist && team.assist->phase==Menu::PracticeAssist::Phase::acting;}
 check(acted && !team.assist,"Valentine assist acts and exits");
 m.fighters[0]={character("valentine")};m.locked_colors[0]={0};m.reset_practice();
}
void chain(Menu& m,bool facing){
 m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];auto& a=*f.valentine;
 f.practice_x=facing?700:1000;m.practice_players[1].practice_x=facing?880:820;a.facing_right=facing;
 unsigned stage=0,max_hits=0;float launch=0;bool air_attack=false;
 for(unsigned t=0;t<500;++t){unsigned mask=0;
  if(!stage){mask=16;stage=1;}
  else if(!f.hit_stop && a.value(1)>0){
   if(stage==1){mask=32;stage=2;}
   else if(stage==2){mask=4|64;stage=3;}
   else if(stage==3){mask=8|(facing?2u:1u);}
   else if(stage==4){mask=32;stage=5;}
  }
  if(stage==3 && f.jump_height>60){mask=16;stage=4;}
  m.practice_keys_held[0]=mask;m.practice_tick();
  max_hits=std::max(max_hits,m.hud_stats[0].hits);launch=std::max(launch,m.practice_players[1].jump_height);
  air_attack|=a.move>=142&&a.move<=147;
 }
 std::cout<<"CHAIN "<<stage<<" hits "<<max_hits<<" launch "<<launch<<" air "<<air_attack<<'\n';
 check(max_hits>=4 && launch>100 && air_attack,"Ground chain launches and follows with air normals");
}
void movement(Menu& m,bool facing){
 Menu::PracticeFighter f;f.practice_x=800;auto& a=f.valentine.emplace();a.facing_right=facing;
 for(unsigned n=0;n<10;++n)tick(m,f,facing,0);
 check(facing?f.practice_x>800:f.practice_x<800,"Walk respects facing");
 tick(m,f,facing,6);for(unsigned n=0;n<20;++n)tick(m,f,facing);
 check(f.jump_height>0,"Jump rises");const auto before=f.jump_height;
 tick(m,f,facing,6);for(unsigned n=0;n<8;++n)tick(m,f,facing);
 check(f.jump_height>before,"Double jump rises again");
 for(unsigned n=0;n<250;++n)tick(m,f,facing);
 check(f.jump_height==0 && a.move==20,"Jump/double jump lands and recovers to idle");
}
#ifdef _WIN32
void preview(Menu& m,const std::filesystem::path& output){
 skullgirls::SpriteArchive ui_win(m.game_data/"ui-win.gfs"),ui(m.game_data/"ui.gfs");
 m.combat_scene=skullgirls::read_combat_scene(ui_win.read("ui/Win/InGame.gbs"));
 for(unsigned i=0;i<2;++i)m.hud_atlas[i].load(ui_win.read("ui/Win/Textures/TA_1001_"+std::to_string(i+1)+".dds"));
 for(auto name:{"valentine","parasoul"}){auto portrait=std::make_unique<Texture>();portrait->load(ui.read("ui/true_color_hud/super_"+std::string(name)+"01.dds"));m.super_portraits[character(name)]=std::move(portrait);}
 m.prepare_hud_portraits();m.prepare_combat_art();
 m.practice_background=std::make_unique<Texture>();m.practice_background->load(skullgirls::read_sprite_asset(m.game_data/"stages-textures2d.gfs","stages/textures/2D/stage_class_notes-alt.dds"),Texture::Encoding::stage_rgb);
 std::filesystem::create_directories(output);CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 for(auto move:{119u,140u,144u,198u,214u}){m.reset_practice();m.practice_presentation={};auto& f=m.practice_players[0];f.valentine->start(move);++f.attack_instance;
  if(move==144){f.jump_height=400;f.valentine->state.variables[0x3b]=2.0f;}
  for(unsigned t=0;t<(move==214?45u:12u);++t)m.practice_tick();
  Gdiplus::Bitmap canvas(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&canvas);dc.Clear(Gdiplus::Color(255,0,0,0));m.paint_practice(dc);}
  check(canvas.Save((output/(std::to_string(move)+".png")).c_str(),&png)==Gdiplus::Ok,"Render Valentine preview");
 }
}
#endif
void basic(Menu& m,bool facing){
 for(bool air:{false,true})for(unsigned button:{1u,2u,4u,8u,16u,32u}){
 Menu::PracticeFighter f;f.practice_x=800;f.valentine.emplace().facing_right=facing;
 if(air){f.valentine->start(98);f.valentine->state.variables[0x3b]=2.0f;f.jump_height=800;}
 tick(m,f,facing,8,button);check(f.valentine->attacking(),"Valentine normal starts: "+std::to_string(button)+" got "+std::to_string(f.valentine->move));
 for(unsigned t=0;t<400&&f.valentine->attacking();++t)tick(m,f,facing);
 check(!f.valentine->attacking(),"Normal recovers");}
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
 if(std::filesystem::path(argv[2])==L"--vials")vials(*m);else if(std::filesystem::path(argv[2])==L"--preview")preview(*m,"build/valentine-check/preview");else throw std::invalid_argument("Unknown preview option");
#endif
#ifndef _WIN32
 throw std::invalid_argument("Preview output requires Windows");
#endif
}else{basic(*m,true);basic(*m,false);commands(*m,true);commands(*m,false);movement(*m,true);movement(*m,false);combat(*m,true);combat(*m,false);grabs(*m);vials(*m);snapshot(*m);teams(*m);chain(*m,true);chain(*m,false);std::cout<<"Valentine movement, normals, specials, super meter/presentation, combat, throws, vials, rollback and combo tests passed\n";}}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
