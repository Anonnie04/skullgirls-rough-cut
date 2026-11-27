#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
void check(bool v,const std::string& s){if(!v)throw std::runtime_error(s);}
#ifdef _WIN32
void snapshot(Menu& m,const std::wstring& name) {
 const auto dir=std::filesystem::path("build/story-check/preview");std::filesystem::create_directories(dir);
 CLSID png;CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
 Gdiplus::Bitmap image(1280,720,PixelFormat32bppPARGB);{Gdiplus::Graphics dc(&image);m.paint(dc);}
 check(image.Save((dir/name).c_str(),&png)==Gdiplus::Ok,"Save story preview");
}
#endif
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
#else
int main(int argc,char** argv)
#endif
{if(argc<2)return 2;Gdiplus::GdiplusStartupInput gi;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&gi,nullptr);int result=0;
 try{auto m=std::make_unique<Menu>();m->load(argv[1]);m->title_visible=m->entering_menu=false;
 m->selected=4;m->cursor=0;m->activate_menu_option();check(m->story_select&&!m->story&&m->training==Menu::TrainingView::fighters,"Story Mode opens single-character select before playback");
#ifdef _WIN32
 snapshot(*m,L"story-character-select.png");
#endif
 m->story_input(VK_LEFT,false);check(std::string_view(roster[m->fighter_cursor])!="filia","Story roster navigation works");
 m->story_input('Z',false);check(m->story&&m->story->character_id=="cerebella","Cerebella selects her own native story");
 m->story_input(VK_ESCAPE,false);m->begin_story_select();m->story_input(VK_ESCAPE,false);check(!m->story_select&&m->training==Menu::TrainingView::none,"Back cancels selection");
 m->begin_story_select();m->story_input(VK_RETURN,false);check(bool(m->story)&&!m->story_select,"Confirm Filia starts her native story");
 const auto initial_node=m->story->route_index;m->story_input(VK_RETURN,true);check(m->story->route_index==initial_node,"Held confirm does not also skip opening");
 m->story_input(VK_RETURN,false);check(m->story->phase==skullgirls::FiliaStory::Phase::fight&&m->story->route_index==1,"Start skips opening to first encounter");
 m->story->route_index=9;m->story->enter();m->story_update_assets();m->story_input(VK_RETURN,false);
 check(m->story->route_index==10&&m->story->phase==skullgirls::FiliaStory::Phase::scene,"Skip advances one scene, retaining the next native cutscene");
 m->story->route_index=14;m->story->enter();m->story_update_assets();m->story_input(VK_RETURN,false);
 check(m->story->phase==skullgirls::FiliaStory::Phase::credits,"Skipping ending reaches credits");
 m->story_input(VK_ESCAPE,false);m->begin_story_select();m->story_input('Z',false);
 unsigned captured=0;
 for(unsigned tick=0;tick<100000&&m->story->phase!=skullgirls::FiliaStory::Phase::complete;++tick){
  if(m->story->phase==skullgirls::FiliaStory::Phase::fight){m->story_continue();continue;}
  m->story->tick();m->story_update_assets();
#ifdef _WIN32
  if(m->story->animation=="Filia_postmiddle_flashback"&&m->story->frame==180)snapshot(*m,L"flashback-animation.png");
  if(m->story->animation=="Catacomb01"&&m->story->frame==500)snapshot(*m,L"catacomb-animation.png");
#endif
#ifdef _WIN32
  const auto& key=m->story->text_key;
  if(!key.empty()&&(key=="Filia Intro 100"||key=="Filia Intro 700"||key=="Filia Post-Middle 700"||key=="Filia Pre-Endboss 400"||key=="Filia Ending A 100"||key=="Filia Ending A 1000")){snapshot(*m,Menu::story_wide(key)+L".png");++captured;}
  if(m->story->phase==skullgirls::FiliaStory::Phase::credits&&m->story->credit_page==0)snapshot(*m,L"credits.png");
#endif
  if(!m->story->text.empty()||m->story->phase==skullgirls::FiliaStory::Phase::credits)m->story_continue();
 }
 check(m->story->phase==skullgirls::FiliaStory::Phase::complete,"Complete narrative traversal from menu");
#ifdef _WIN32
 check(captured==6,"Rendered opening, dialogue, flashback return, boss dialogue, ending and last line");snapshot(*m,L"complete.png");
#endif
 m->story->route_index=1;m->story->enter();m->story_fight_started=false;m->story_tick();
 check(m->practice_load.valid()&&!m->story_boss_checkpoint,"First native encounter loads combat");
 m->finish_practice(nullptr);check(m->training==Menu::TrainingView::practice,"Story fight starts");
 m->practice_presentation={};m->practice_keys_held[1]=0x3ff;m->practice_attack_edges[1]=0x3f0;
 for(unsigned i=0;i<120;++i)m->practice_tick();
 check(m->practice_keys_previous[1]==0,"Temporary idle AI ignores manual P2/controller input");
 check(!m->practice_players[1].practice_left&&!m->practice_players[1].practice_right&&!m->practice_players[1].practice_up&&!m->practice_players[1].practice_down,"Enemy does not move");
#ifdef _WIN32
 snapshot(*m,L"idle-encounter.png");
#endif
 check(m->story_input(VK_RETURN,false),"Enter skips combat");check(m->story->route_index==2,"Next native encounter reached");
 for(const auto node:{2u,4u,6u,8u}) {
  m->story->route_index=node;m->story->enter();m->story_fight_started=false;m->story_tick();
  check(m->story_error.empty()&&m->practice_load.valid(),"Every regular encounter loads its native stage/team");m->finish_practice(nullptr);
  check(m->fighters[1].size()==(node==8?2u:1u),"Native Double/Valentine team retained");
  check(m->current_stage().id==m->story->world,"Native encounter stage retained");
  m->practice_presentation={};m->practice_keys_held[1]=0x3ff;m->practice_attack_edges[1]=0x3f0;
  for(unsigned i=0;i<60;++i)m->practice_tick();check(m->practice_keys_previous[1]==0,"Every enemy uses temporary neutral AI");
  m->story_continue();
 }
 m->story->route_index=13;m->story->enter();m->story_fight_started=false;m->story_tick();
 check(m->story_boss_checkpoint&&!m->practice_load.valid(),"Original boss has explicit preview checkpoint, no DLC substitution");

#ifdef _WIN32
 snapshot(*m,L"boss-checkpoint.png");
#endif
 m->story_continue();check(m->story->route_index==14,"Boss checkpoint reaches ending");
 m->story_input(VK_ESCAPE,false);check(!m->story&&m->training==Menu::TrainingView::none,"Escape returns to main menu");
 std::cout<<"Story menu, narrative rendering, idle AI combat, encounter skipping and boss handoff passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
