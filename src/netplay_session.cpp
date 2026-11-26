#include "netplay_session.hpp"
#include "sync.h"
#include "timesync.h"
#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace skullgirls {
NetInput::Bytes NetInput::encode() const {
    Bytes out{};std::copy(leading.begin(),leading.end(),out.begin());
    std::copy(fields.begin(),fields.end(),out.begin()+3);
    out[18]=std::uint8_t(auxiliary);out[19]=std::uint8_t(auxiliary>>8);out[20]=start?1:0;return out;
}
NetInput NetInput::decode(const Bytes& bytes) {
    NetInput out;std::copy_n(bytes.begin(),3,out.leading.begin());std::copy_n(bytes.begin()+3,15,out.fields.begin());
    out.auxiliary=std::uint16_t(bytes[18])|(std::uint16_t(bytes[19])<<8);out.start=bytes[20]==1;return out;
}
struct NetplaySession::Impl {
    struct Saved {State state;int frame;};
    inline static thread_local Impl* active=nullptr;
    struct Scope {Impl* previous;Scope(Impl* p):previous(active){active=p;}~Scope(){active=previous;}};
    Callbacks cb;
    std::shared_ptr<NetplayTransport> transport;
    UdpMsg::connect_status connections[2]{};
    std::unique_ptr<Sync> sync;
    TimeSync timesync;
    unsigned local;
    int next_remote,confirmed=-1;
    bool running=false,closed=false,interrupted=false;
    unsigned rollbacks=0,resimulated=0;
    std::map<int,NetInput> pending;
    NetInput::Bytes last_local{};
    explicit Impl(unsigned side,Callbacks callbacks,std::shared_ptr<NetplayTransport> t,int delay,int remote_delay):
        cb(std::move(callbacks)),transport(std::move(t)),local(side),next_remote(remote_delay) {
        if(side>1 || !transport || !cb.save || !cb.load || !cb.advance || !cb.checksum ||
           delay<0 || remote_delay<0 || delay>=prediction_limit || remote_delay>=prediction_limit)
            throw std::invalid_argument("Invalid GGPO session configuration");
        for(auto& c:connections)c.last_frame=-1;
        Scope scope(this);sync=std::make_unique<Sync>(connections);
        Sync::Config config{};config.num_prediction_frames=prediction_limit;config.num_players=2;config.input_size=21;
        config.callbacks.save_game_state=[](unsigned char** buffer,int* size,int* checksum,int frame)->bool {
            auto state=active->cb.save();if(!state)throw std::runtime_error("GGPO save returned no state");
            auto saved=std::make_unique<Saved>();saved->state=std::move(state);saved->frame=frame;
            *checksum=int(active->cb.checksum(saved->state));*size=sizeof(Saved);*buffer=reinterpret_cast<unsigned char*>(saved.release());return true;
        };
        config.callbacks.load_game_state=[](unsigned char* buffer,int size)->bool {
            if(!buffer || size!=sizeof(Saved))throw std::runtime_error("Invalid GGPO saved state");
            const auto* saved=reinterpret_cast<Saved*>(buffer);
            if(active->cb.event)active->cb.event(Event::rollback_begin,active->sync->GetFrameCount()-saved->frame);
            active->cb.load(saved->state);++active->rollbacks;return true;
        };
        config.callbacks.free_buffer=[](void* buffer){delete static_cast<Saved*>(buffer);};
        config.callbacks.advance_frame=[](int)->bool {active->advance(true);return true;};
        sync->Init(config);sync->SetFrameDelay(local,delay);
        // GGPO's delayed queues pad initial frames with zero inputs.
        for(int frame=0;frame<remote_delay;++frame){GameInput input;NetInput::Bytes zero{};input.init(frame,reinterpret_cast<char*>(zero.data()),21);sync->AddRemoteInput(1-local,input);}
    }
    ~Impl(){Scope scope(this);sync.reset();}
    void advance(bool replay) {
        std::array<NetInput::Bytes,2> bytes{};sync->SynchronizeInputs(bytes.data(),42);
        cb.advance({NetInput::decode(bytes[0]),NetInput::decode(bytes[1])},replay);
        if(replay)++resimulated;sync->IncrementFrame();
    }
    void idle() {
        Scope scope(this);if(closed)return;
        NetFrame incoming;unsigned count=0;
        while(count++<256 && transport->receive_input(incoming)) {
            if(incoming.frame<next_remote)continue;
            if(incoming.frame>sync->GetFrameCount()+127)throw std::runtime_error("Remote input exceeds GGPO queue window");
            auto existing=pending.find(incoming.frame);
            if(existing!=pending.end() && existing->second.encode()!=incoming.input.encode())throw std::runtime_error("Conflicting remote input");
            pending.emplace(incoming.frame,incoming.input);
        }
        while(true) {
            auto found=pending.find(next_remote);if(found==pending.end())break;
            auto bytes=found->second.encode();GameInput input;input.init(next_remote,reinterpret_cast<char*>(bytes.data()),21);
            sync->AddRemoteInput(1-local,input);pending.erase(found);++next_remote;
        }
        const auto before=rollbacks,frames_before=resimulated;
        if(sync->GetFrameCount()>0)sync->CheckSimulation(0);
        if(rollbacks!=before && cb.event)cb.event(Event::rollback_end,int(resimulated-frames_before));
        confirmed=std::min(sync->GetFrameCount()-1,next_remote-1);sync->SetLastConfirmedFrame(confirmed);
    }
};
NetplaySession::NetplaySession(unsigned side,Callbacks cb,std::shared_ptr<NetplayTransport> transport,int delay,int remote_delay):
    impl(std::make_unique<Impl>(side,std::move(cb),std::move(transport),delay,remote_delay)){}
NetplaySession::~NetplaySession()=default;
void NetplaySession::notify(Event event,int value) {
    if(event==Event::running){if(impl->closed)throw std::runtime_error("Cannot restart a closed GGPO session");impl->running=true;}
    if(event==Event::interrupted)impl->interrupted=true;
    if(event==Event::resumed)impl->interrupted=false;
    if(event==Event::disconnected){impl->running=false;impl->closed=true;}
    if(impl->cb.event)impl->cb.event(event,value);
}
void NetplaySession::idle(){impl->idle();}
NetplaySession::Result NetplaySession::step(const NetInput& input) {
    Impl::Scope scope(impl.get());if(impl->closed)return Result::disconnected;
    impl->idle();if(!impl->running || impl->interrupted)return Result::not_synchronized;
    auto bytes=input.encode();GameInput local;local.init(impl->sync->GetFrameCount(),reinterpret_cast<char*>(bytes.data()),21);
    if(!impl->sync->AddLocalInput(impl->local,local))return Result::prediction_threshold;
    impl->last_local=bytes;
    if(local.frame>=0)impl->transport->send_input({local.frame,input});
    impl->advance(false);return Result::advanced;
}
NetplaySession::Stats NetplaySession::stats() const {return {impl->sync->GetFrameCount(),impl->confirmed,impl->rollbacks,impl->resimulated};}
bool NetplaySession::in_rollback() const {return impl->sync->InRollback();}
int NetplaySession::recommend_frame_wait(int local_advantage,int remote_advantage) {
    GameInput input;input.init(std::max(0,impl->sync->GetFrameCount()-1),reinterpret_cast<char*>(impl->last_local.data()),21);
    impl->timesync.advance_frame(input,local_advantage,remote_advantage);
    const int frames=impl->timesync.recommend_frame_wait_duration(false);
    if(frames && impl->cb.event)impl->cb.event(Event::timesync,frames);return frames;
}
}
