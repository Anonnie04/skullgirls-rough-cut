#ifndef _WIN32
#define SKULLGIRLS_NO_MAIN
#endif
#include "../src/main_menu.cpp"
#include <iostream>
namespace {
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void mapping(){
 using namespace skullgirls;ControllerMapper m;ControllerSample p;p.connected=true;m.update(p,true);
 const std::array<std::pair<unsigned,unsigned>,6> cases{{{2,16},{3,32},{0,128},{1,256},{9,112},{10,896}}};
 for(auto [button,mask]:cases){p.buttons=1u<<button;auto f=m.update(p,true);check(f.held==mask&&f.pressed==mask,"Six-button layout and bumper macros map to combat masks");check(m.update(p,true).pressed==0,"Held attacks produce one edge");p.buttons=0;m.update(p,true);}
 p.buttons=1u<<9;m.update(p,true);p.buttons=0;p.axes[5]=7000;check(m.update(p,true).held==0,"Releasing a bumper macro cannot latch a trigger");
 p.axes[5]=9000;check(m.update(p,true).held==64,"RT maps HP");p.axes[5]=7000;check(m.update(p,true).held==64,"Trigger hysteresis prevents threshold chatter");p.axes[5]=6000;check(m.update(p,true).held==0,"Trigger releases below lower threshold");p.axes[5]=0;p.axes[4]=9000;check(m.update(p,true).held==512,"LT maps HK");p.axes={};m.update(p,true);
 p.axes[0]=-20000;p.axes[1]=20000;check(m.update(p,true).held==5,"Analog stick maps diagonal directions");p.axes={};p.buttons=(1u<<13)|(1u<<14);check(m.update(p,true).held==0,"Opposing directions resolve neutral");
 p.buttons=1;m.update(p,false);check(m.update(p,true).held==0,"Focus return suppresses held buttons");p.buttons=0;m.update(p,true);p.buttons=1;check(m.update(p,true).pressed==128,"Release rearms controller after focus loss");
 p.connected=false;check(m.update(p,true).disconnected,"Unplug is reported once");check(!m.update(p,true).disconnected,"Disconnect is not repeated");p.connected=true;p.buttons=1;check(m.update(p,true).held==0,"Reconnect while held is neutral until release");
}
void integration(Menu& m){
 using Sample=skullgirls::ControllerSample;std::array<Sample,2> pads{};pads[0].connected=true;pads[0].name="Test Steam Controller";
 const auto emit=[&](WPARAM key){if(m.training==Menu::TrainingView::practice){m.practice_input(key,false);m.practice_key(key,false);}else m.training_input(key,key=='A'||key=='Z',key==VK_ESCAPE);};
 m.begin_local_versus();m.team_size={1,1};m.controller_frame(pads,true,emit);
 pads[0].buttons=1;m.controller_frame(pads,true,emit);check(m.training==Menu::TrainingView::fighters,"Controller confirms team size");
 pads[0].buttons=0;m.controller_frame(pads,true,emit);pads[0].buttons=1;m.controller_frame(pads,true,emit);check(m.player==1&&m.fighters[0].size()==1,"Controller selects P1 fighter");
 m.controller_frame(pads,true,emit);check(m.fighters[1].empty(),"P1 controller cannot pick P2 team");
 m.training_input('F',false,false);m.training_input('V',false,false);check(m.training==Menu::TrainingView::stage_select,"P2 keyboard remains independent from P1 controller");
 m.load_movement_frames();m.load_practice_effects();m.fighters[0]=m.fighters[1]={9};m.locked_colors[0]=m.locked_colors[1]={0};
 m.practice_width=1700;m.practice_height=2000;m.practice_floor=64;m.practice_starts={400,1250};m.training=Menu::TrainingView::practice;m.reset_practice();m.practice_presentation={};
 pads[0].buttons=0;m.controller_frame(pads,true,emit);pads[0].axes[0]=20000;m.controller_frame(pads,true,emit);m.practice_key('J',true);
 for(unsigned i=0;i<8;++i)m.practice_tick();check(m.practice_players[0].practice_x>400&&m.practice_players[1].practice_x<1250,"Controller and keyboard fight simultaneously");
 pads[0].axes={};pads[0].buttons=1u<<2;m.controller_frame(pads,true,emit);m.practice_tick();check(m.practice_players[0].active_normal>=0,"Controller punch enters real combat normal");
 pads[0].buttons=1u<<6;m.controller_frame(pads,true,emit);check(m.practice_paused,"Start pauses a local match");
 pads[0].buttons=0;m.controller_frame(pads,true,emit);pads[0].buttons=1;m.controller_frame(pads,true,emit);check(!m.practice_paused&&m.practice_keys_held[0]==0,"A resumes without a stuck synthetic keyboard attack");
 pads[0].connected=false;m.controller_frame(pads,true,emit);check(m.practice_paused&&m.controller_held[0]==0,"Disconnect pauses Versus and releases input");
 pads[0].connected=true;pads[0].buttons=1;m.controller_frame(pads,true,emit);check(m.controller_held[0]==0,"Held reconnect does not attack or resume");
 m.network_mode=true;m.controller_frame(pads,true,emit);check(m.controller_held==std::array<unsigned,2>{},"Hardware inputs cannot leak into synchronized netplay frames");m.network_mode=false;
}
#ifdef _WIN32
void live(){
 skullgirls::WindowsControllers backend;check(backend.initialize(),backend.status.c_str());std::cout<<backend.status<<std::endl;
 std::array<skullgirls::ControllerMapper,2> mapper{};std::array<unsigned,2> previous{};bool seen=false;
 for(unsigned tick=0;tick<900;++tick){auto pads=backend.poll();for(unsigned side=0;side<2;++side){auto f=mapper[side].update(pads[side],true);if(pads[side].connected&&!seen){std::cout<<"Connected: "<<pads[side].name<<std::endl;seen=true;}if(previous[side]!=f.held){std::cout<<"P"<<side+1<<" mask="<<f.held<<" pressed="<<f.pressed<<std::endl;previous[side]=f.held;}}Sleep(16);}
 check(seen,"No live controller detected");
}
#endif
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
#else
int main(int argc,char** argv)
#endif
{if(argc<2)return 2;Gdiplus::GdiplusStartupInput gi;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&gi,nullptr);int result=0;
try{
#ifdef _WIN32
 if(argc>2){live();}else
#endif
 {mapping();auto m=std::make_unique<Menu>();m->load(argv[1]);integration(*m);std::cout<<"Controller mapping and integration tests passed\n";}}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}Gdiplus::GdiplusShutdown(token);return result;}
