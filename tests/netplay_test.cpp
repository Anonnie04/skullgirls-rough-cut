#include "netplay_session.hpp"
#include <algorithm>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace skullgirls;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct Model {unsigned frame=0,rng=1;std::uint32_t value=0;};
void advance(Model& m,const std::array<NetInput,2>& input) {
    m.rng=m.rng*0x41c64e6du+0x3039u;
    for(auto& in:input)for(auto b:in.encode())m.value=(m.value*16777619u)^b;
    m.value^=m.rng;++m.frame;
}
NetInput input(unsigned frame,unsigned side) {
    NetInput value;for(unsigned i=0;i<3;++i)value.leading[i]=std::uint8_t((frame/7+side+i)%9);
    for(unsigned i=0;i<15;++i)value.fields[i]=std::uint8_t((frame+side*9+i*3)%5);
    value.auxiliary=std::uint16_t(frame*31+side);value.start=frame%79==0;return value;
}
struct Bus;
struct Endpoint:NetplayTransport {Bus* bus;unsigned side;std::deque<NetFrame> incoming;
    Endpoint(Bus* b,unsigned s):bus(b),side(s){}void send_input(const NetFrame&)override;
    bool receive_input(NetFrame& out)override{if(incoming.empty())return false;out=incoming.front();incoming.pop_front();return true;}};
struct Bus {struct Delivery{unsigned target,time;NetFrame frame;};unsigned now=0;std::vector<Delivery> waiting;
    std::shared_ptr<Endpoint> endpoints[2]{std::make_shared<Endpoint>(this,0),std::make_shared<Endpoint>(this,1)};
    void pump(){for(auto i=waiting.begin();i!=waiting.end();)if(i->time<=now){endpoints[i->target]->incoming.push_back(i->frame);i=waiting.erase(i);}else ++i;}
};
void Endpoint::send_input(const NetFrame& f){
    // Test-only delivery scheduler, not a wire protocol. Reorders, duplicates and
    // models loss followed by a transport retransmit; the Steam layer remains TODO.
    const unsigned delay=unsigned(f.frame)%11==0?14:unsigned(f.frame*3+side)%7;
    bus->waiting.push_back({1-side,bus->now+delay,f});
    if(f.frame%13==0)bus->waiting.push_back({1-side,bus->now+delay+2,f});
}
NetplaySession::Callbacks callbacks(Model& model,unsigned& events) {
    return {[&](){return std::make_shared<Model>(model);},
        [&](const NetplaySession::State& saved){model=*std::static_pointer_cast<const Model>(saved);},
        [&](const auto& inputs,bool){advance(model,inputs);},
        [](const NetplaySession::State& saved){auto m=std::static_pointer_cast<const Model>(saved);return m->value^m->frame^m->rng;},
        [&](NetplaySession::Event,int){++events;}};
}
int main(){try{
    NetInput raw;raw.leading={1,2,3};for(unsigned i=0;i<15;++i)raw.fields[i]=std::uint8_t(i+4);
    raw.auxiliary=0xbeef;raw.start=true;auto bytes=raw.encode();
    check(bytes.size()==21 && bytes[18]==0xef && bytes[19]==0xbe && bytes[20]==1,"Original 21-byte input layout");
    check(NetInput::decode(bytes).encode()==bytes,"Input codec roundtrip");bytes[20]=255;
    check(!NetInput::decode(bytes).start,"Original boolean decoder accepts exactly one");
    for(int delay:{0,3}){
        Bus bus;Model a,b,expected;unsigned events=0;
        NetplaySession peers0(0,callbacks(a,events),bus.endpoints[0],delay,0);
        NetplaySession peers1(1,callbacks(b,events),bus.endpoints[1],0,delay);
        check(peers0.step(input(0,0))==NetplaySession::Result::not_synchronized && a.frame==0,"Wait for original running event");
        peers0.notify(NetplaySession::Event::running);peers1.notify(NetplaySession::Event::running);
        constexpr unsigned frames=500;
        for(bus.now=0;bus.now<2000 && (a.frame<frames || b.frame<frames);++bus.now){
            bus.pump();peers0.idle();peers1.idle();
            if(a.frame<frames)peers0.step(input(a.frame,0));
            if(b.frame<frames)peers1.step(input(b.frame,1));
        }
        for(unsigned n=0;n<40;++n){++bus.now;bus.pump();peers0.idle();peers1.idle();}
        for(unsigned f=0;f<frames;++f)advance(expected,{f<unsigned(delay)?NetInput{}:input(f-delay,0),input(f,1)});
        check(a.frame==frames && b.frame==frames,"Both simulated peers progress");
        check(a.value==expected.value && b.value==expected.value && a.rng==b.rng,"Rollback converges to confirmed-input execution");
        check(peers0.stats().rollbacks && peers1.stats().rollbacks && peers0.stats().resimulated_frames,"Late inputs trigger actual GGPO rollback");
        peers0.notify(NetplaySession::Event::interrupted);auto before=a.frame;
        check(peers0.step({})==NetplaySession::Result::not_synchronized && a.frame==before,"Interrupted session does not simulate");
        peers0.notify(NetplaySession::Event::resumed);peers0.notify(NetplaySession::Event::disconnected);
        check(peers0.step({})==NetplaySession::Result::disconnected,"Disconnected session does not simulate");
    }
    {Bus bus;Model model;unsigned events=0;NetplaySession session(0,callbacks(model,events),bus.endpoints[0]);session.notify(NetplaySession::Event::running);
     for(unsigned f=0;f<36;++f)check(session.step(input(f,0))==NetplaySession::Result::advanced,"Original prediction window");
     check(session.step({})==NetplaySession::Result::prediction_threshold && model.frame==36,"Original prediction barrier stalls without remote input");}
    std::cout<<"GGPO 21-byte inputs, 38-state ring, delayed/reordered/duplicate inputs, rollback convergence, frame delay and lifecycle passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
