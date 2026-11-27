#include "story_scene.hpp"
#include <iostream>
#include <set>
int main(int argc,char** argv){if(argc<2)return 2;unsigned failures=0,total_lines=0,total_scenes=0;const std::filesystem::path data=argv[1];
 skullgirls::SpriteArchive core(data/"core.gfs"),ui(data/"ui.gfs"),win(data/"ui-win.gfs"),art(data/"story_art.gfs");auto gbs=win.read("ui/Win/CutsceneCommon.gbs");
 for(const auto& character:skullgirls::story_characters)try{
  skullgirls::StoryPlayback s;s.load([&](auto p){return core.read(p);},[&](auto p){return ui.read(p);},gbs,character.id);
  unsigned ticks=0,lines=0;bool checked_hold=false;std::set<std::string> seen,expected,assets;std::set<std::size_t> visited;
  for(const auto& node:s.route)if(node.type=="NewScene")for(const auto& c:skullgirls::read_story_commands(skullgirls::story_text(core.read(node.file))))if(c.type=="Text")expected.insert(c.value);
  while(s.phase!=skullgirls::StoryPlayback::Phase::complete&&ticks++<1000000){
   if(s.phase==skullgirls::StoryPlayback::Phase::scene){visited.insert(s.route_index);
    for(const auto& [slot,path]:s.shots)if(!path.empty())assets.insert(path);
    for(const auto& p:s.portraits)if(!p.empty())assets.insert(p);
    if(!s.text.empty()){
     if(!checked_hold&&std::string(character.id)=="robofortune"&&!s.layers.empty()) {
      auto held=s;const auto slot=held.layers.begin()->first;const auto& a=held.clip(held.layers.at(slot).name);
      if(a.loop_start==~0u){for(unsigned i=0;i<a.animation.frames+30;++i)held.tick();
       if(!held.layers.at(slot).finished||held.layers.at(slot).frame!=a.animation.frames)throw std::runtime_error("One-shot persistent camera did not hold its final frame");checked_hold=true;}
     }
     seen.insert(s.text_key);++lines;s.advance();
    }else s.tick();
   }else s.advance();
  }
  if(ticks>=1000000)throw std::runtime_error("Timeline stalled at "+s.route.at(s.route_index).file+" / "+s.animation);
  if(seen!=expected)throw std::runtime_error("Dialogue coverage mismatch");
  const auto count=std::count_if(s.route.begin(),s.route.end(),[](const auto& n){return n.type=="NewScene";});
  if(visited.size()!=count)throw std::runtime_error("Not every scene was visited");
  if(s.scene.symbols.size()!=13)throw std::runtime_error("Hungern glyph mapping incomplete");
  const auto& dahlia=s.scene.views.at("ShotHandling").clips.at("Dahlia_Scene01");
  auto p=dahlia.pivots.at(0).front().second;const auto& t=dahlia.animation.tracks.at(0).front().transform;
  if(p[0]>-800||p[1]>-800||t.x-p[0]<-100||t.y-p[1]<-100)throw std::runtime_error("Native animated pivot would put Dahlia artwork offscreen");
  for(const auto& p:assets)skullgirls::read_story_art(art,p);
  total_lines+=lines;total_scenes+=unsigned(visited.size());
  std::cout<<character.id<<": "<<lines<<" lines, "<<visited.size()<<" scenes, "<<assets.size()<<" art references, "<<ticks<<" ticks\n";
 }catch(const std::exception& e){++failures;std::cerr<<character.id<<": "<<e.what()<<'\n';}
 std::cout<<"Total: "<<total_lines<<" lines, "<<total_scenes<<" scenes; "<<failures<<" failed stories\n";return failures?1:0;}
