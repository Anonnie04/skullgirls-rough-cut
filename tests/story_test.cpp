#include "story_scene.hpp"
#include "story_enemy_ai.hpp"
#include <iostream>
#include <set>
void check(bool v,const std::string& s){if(!v)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
 if(argc<2)return 2;
 const std::filesystem::path data=argv[1];
 skullgirls::SpriteArchive core(data/"core.gfs"),ui(data/"ui.gfs"),win(data/"ui-win.gfs"),art(data/"story_art.gfs");
 skullgirls::FiliaStory story;
 story.load([&](const auto& p){return core.read(p);},[&](const auto& p){return ui.read(p);},win.read("ui/Win/CutsceneCommon.gbs"));
 check(story.route.size()==16,"All sixteen native route nodes");
 check(story.scene.views.at("Filia_Legacy_Intro").clips.at("Intro").animation.frames==1743,"Native opening timeline");
 check(story.scene.views.at("Filia_Legacy_Ending").clips.at("Ending").animation.frames==4502,"Native ending timeline");
 check(!story.credits.empty(),"Native credits loaded");
 check(story.scene.flips.at("Filia_Legacy_Ending").at(24)[0],"Native ending shot 09 horizontal flip is preserved");
 // Exercise a reader lingering on dialogue, then resuming while a loop
 // or continued animation is still running. Native STOP is not a new page.
 for(unsigned i=0;story.text.empty()&&i<2000;++i)story.tick();
 check(story.text_key=="Filia Intro 100"&&story.frame==984,"Opening waits at first native dialogue cue");
 for(unsigned i=0;i<100;++i)story.tick();check(story.frame==994&&story.paused,"CONT_ANIM reaches its authored stop");
 story.advance();for(unsigned i=0;story.text.empty()&&i<2000;++i)story.tick();
 check(story.text_key=="Filia Intro 200"&&story.frame==1238,"Resume reaches next dialogue cue");
 story.advance();check(story.text_key=="Filia Intro 300","Consecutive lines share the cue");story.advance();
 for(unsigned i=0;story.text.empty()&&i<2000;++i)story.tick();
 check(story.text_key=="Filia Intro 400"&&story.frame==1293,"Loop dialogue cue");
 for(unsigned i=0;i<125;++i)story.tick();check(story.frame>=1293&&story.frame<1343&&!story.paused,"Authored dialogue loop repeats");
 {auto skipped=story;skipped.skip_scene();check(skipped.route_index==1&&skipped.phase==skullgirls::FiliaStory::Phase::fight,"Skip looped dialogue reaches exactly the following encounter");
  check(skipped.text.empty()&&skipped.animation.empty()&&skipped.persistent.empty()&&skipped.dialogue_loop<0,"Skip cleans up dialogue, animation and loop state");}
 story.advance();for(unsigned i=0;story.text.empty()&&i<2000;++i)story.tick();
 check(story.text_key=="Filia Intro 500"&&story.frame==1398,"Resume bypasses loop stop to next dialogue cue");
 story.advance();for(unsigned i=0;story.text.empty()&&i<2000;++i)story.tick();
 check(story.text_key=="Filia Intro 700","Opening footage finishes before portrait dialogue");
 story.route_index=0;story.enter();
 std::set<std::string> seen,assets;std::set<std::size_t> scenes,fights;unsigned ticks=0;
 for(;story.phase!=skullgirls::FiliaStory::Phase::complete&&ticks<100000;++ticks){
  if(story.phase==skullgirls::FiliaStory::Phase::scene){
   scenes.insert(story.route_index);
   for(const auto& [slot,path]:story.shots)assets.insert(path);
   for(const auto& path:story.portraits)if(!path.empty())assets.insert(path);
   if(!story.text.empty()){
    check(!story.speaker.empty(),"Dialogue has original speaker: "+story.text_key);
    check(seen.insert(story.text_key).second,"No duplicate/repeated dialogue: "+story.text_key);
    check(story.text.find("\\n")==std::string::npos,"JSON newlines decoded");
    story.advance();
   }else story.tick();
  }else if(story.phase==skullgirls::FiliaStory::Phase::fight){
   fights.insert(story.route_index);check(!story.enemies.empty()&&!story.world.empty(),"Native encounter metadata");
   auto ai=skullgirls::story_enemy_ai();check(ai.held==0&&ai.pressed==0,"Temporary AI remains neutral");story.advance();
  }else story.advance();
 }
 check(ticks<100000,"Full story terminates without animation deadlock");
 check(scenes.size()==9&&fights.size()==6,"All scenes and encounters visited");
 std::set<std::string> expected;
 for(const auto& node:story.route)if(node.type=="NewScene")for(const auto& c:skullgirls::read_story_commands(skullgirls::story_text(core.read(node.file))))if(c.type=="Text")expected.insert(c.value);
 check(seen==expected,"Every native dialogue line reached, including final ending line");
 check(seen.count("Filia Ending A 1000"),"Ending reaches last line");
 for(const auto& path:assets){auto b=art.read(path);check(b.size()>=128&&std::string(b.begin(),b.begin()+4)=="DDS ","Story art resolves: "+path);}
 check(story.credits.back().back()=="...and You!","Credits reach final native page");
 auto bytes=win.read("ui/Win/CutsceneCommon.gbs");bytes.resize(60);bool rejected=false;
 try{skullgirls::read_story_scene(bytes);}catch(const std::exception&){rejected=true;}check(rejected,"Reject truncated scene");
 std::cout<<seen.size()<<" dialogue lines, "<<assets.size()<<" art assets, 9 scenes, 6 encounters, "<<story.credits.size()<<" credits pages; "<<ticks<<" ticks\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
