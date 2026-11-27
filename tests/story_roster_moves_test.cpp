#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
void check(bool v,const std::string& s){if(!v)throw std::runtime_error(s);}
#ifdef _WIN32
void snapshot(Menu& m,const std::string& name){
 const auto dir=std::filesystem::path("build/story-all/preview");std::filesystem::create_directories(dir);
 CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 Gdiplus::Bitmap image(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&image);m.paint(dc);}
 check(image.Save((dir/(name+".png")).c_str(),&png)==Gdiplus::Ok,"Save story screenshot");
}
#endif
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
#else
int main(int argc,char** argv)
#endif
{if(argc<2)return 2;Gdiplus::GdiplusStartupInput gi;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&gi,nullptr);int result=0;
 try{auto m=std::make_unique<Menu>();m->load(argv[1]);m->title_visible=m->entering_menu=false;
 {
  skullgirls::SpriteArchive art(m->game_data/"story_art.gfs");auto wide=art.read("story_art/Cinematics/Annie/04_bg.dds");
  bool rejected=false;try{Texture::dds_layout(wide);}catch(const std::exception&){rejected=true;}
  check(rejected,"Normal texture source limit remains intact");
  Texture t;t.load(wide,Texture::Encoding::rgba,2048,8192);
  check(t.width<=2048&&t.height<=2048,"Large native story artwork decodes to bounded presentation size");
 }
 bool hungern=false,multiple=false,postcredit=false;
 for(const auto& c:skullgirls::story_characters){
  m->begin_story_select();m->fighter_cursor=int(std::find(roster.begin(),roster.end(),std::string_view(c.id))-roster.begin());m->story_input('Z',false);
  check(m->story&&m->story->character_id==c.id,"Selection opens correct route");bool captured=false,sawcredits=false;unsigned tick=0;
  for(;m->story->phase!=skullgirls::StoryPlayback::Phase::complete&&tick<1000000;++tick){
   auto& s=*m->story;
   if(s.phase==skullgirls::StoryPlayback::Phase::fight){
    check(!s.encounter_teams[0].empty()&&!s.encounter_teams[1].empty(),"Both native encounter teams parsed");
    m->story_continue();continue;
   }
   if(s.phase==skullgirls::StoryPlayback::Phase::credits)sawcredits=true;
   s.tick();m->story_update_assets();
#ifdef _WIN32
   if(!captured&&s.phase==skullgirls::StoryPlayback::Phase::scene&&(!s.text.empty()||s.frame>=60)){snapshot(*m,c.id);captured=true;}
   if(!hungern&&s.text.rfind("{hn",0)==0){snapshot(*m,"hungern-symbols");hungern=true;}
   if(!multiple&&s.layers.size()>=3){snapshot(*m,"multiple-layers");multiple=true;}
   if(!postcredit&&sawcredits&&s.phase==skullgirls::StoryPlayback::Phase::scene&&!s.text.empty()){snapshot(*m,"post-credits");postcredit=true;}
#endif
   if(!s.text.empty()||s.phase==skullgirls::StoryPlayback::Phase::credits)m->story_continue();
  }
  check(tick<1000000,"Rendered traversal finishes: "+std::string(c.id));m->story_input(VK_ESCAPE,false);
  check(!m->story&&m->story_art.empty(),"Exit releases scene art");
  std::cout<<c.id<<" rendered through completion\n";
 }
#ifdef _WIN32
 check(hungern&&multiple&&postcredit,"Hungern, parallel layers and post-credits rendered");
#endif
 // Native protagonist and opponent colors; special variants remain checkpoints.
 m->begin_story("parasoul");for(std::size_t i=0;i<m->story->route.size();++i)if(m->story->route[i].type=="Fight"){m->story->route_index=i;break;}
 m->story->enter();m->story_fight_started=false;m->story_tick();check(m->practice_load.valid(),"Parasoul's regular encounter loads");m->finish_practice(nullptr);
 check(std::string_view(roster[m->fighters[0][0]])=="parasoul","Selected protagonist reaches combat");m->story_continue();m->leave_story();
 m->begin_story("marie");m->story->route_index=2;m->story->enter();m->story_fight_started=false;m->story_tick();
 check(m->story_boss_checkpoint&&!m->practice_load.valid(),"Marie opening preserves original BossMarie protagonist as checkpoint");m->leave_story();
 m->begin_story("fukua");for(std::size_t i=0;i<m->story->route.size();++i)if(m->story->route[i].file.find("FinalFilia")!=std::string::npos){m->story->route_index=i;break;}
 m->story->enter();m->story_fight_started=false;m->story_tick();check(m->story_boss_checkpoint,"Scripted health/meter variant is preserved as checkpoint");m->leave_story();
 std::cout<<"All 18 selectable stories and native encounter handoffs passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
