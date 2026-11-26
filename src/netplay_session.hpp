#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace skullgirls {
// FUN_0053b020 / FUN_0053b070: 3 bytes, 15 bytes, LE16, normalized bool.
// This preserves the original record storage; it does not guess every field's meaning.
struct NetInput {
    std::array<std::uint8_t,3> leading{};
    std::array<std::uint8_t,15> fields{};
    std::uint16_t auxiliary=0;
    bool start=false;
    using Bytes=std::array<std::uint8_t,21>;
    Bytes encode() const;
    static NetInput decode(const Bytes& bytes);
};
struct NetFrame {int frame=0;NetInput input;};

// Boundary for the original SteamTransport / SteamNetworkingMessages002.
// TODO(STEAM): FUN_0069f560 constructs the transport, FUN_0069f600 receives,
// FUN_0069f6d0 sends on channels 1/999 with flags 0x21. Feed only authenticated,
// ordered GGPO player inputs here after the original protocol has been decoded.
// TODO(PROTOCOL): port/verify the original Peer2PeerBackend packet decoding,
// acknowledgements, retransmission, synchronization and spectator handling before
// connecting this frame-level adapter to live messages. These are distinct from
// the Steam API calls; this synchronization core does not implement them.
// No substitute TCP/UDP wire protocol is defined by this interface.
class NetplayTransport {
public:
    virtual ~NetplayTransport()=default;
    virtual void send_input(const NetFrame&)=0;
    virtual bool receive_input(NetFrame&)=0;
};

class NetplaySession {
public:
    static constexpr int prediction_limit=36,saved_state_count=38;
    enum class Result {advanced,not_synchronized,prediction_threshold,disconnected};
    enum class Event {connected=1000,synchronizing=1001,synchronized=1002,running=1003,
        disconnected=1004,timesync=1020,interrupted=1021,resumed=1022,rollback_begin=2000,rollback_end=2001};
    using State=std::shared_ptr<const void>;
    struct Callbacks {
        std::function<State()> save;
        std::function<void(const State&)> load;
        std::function<void(const std::array<NetInput,2>&,bool rollback)> advance;
        std::function<std::uint32_t(const State&)> checksum;
        std::function<void(Event,int)> event;
    };
    struct Stats {int frame=0,confirmed=-1;unsigned rollbacks=0,resimulated_frames=0;};
    NetplaySession(unsigned local_side,Callbacks,std::shared_ptr<NetplayTransport>,int local_delay=0,int remote_delay=0);
    ~NetplaySession();
    NetplaySession(const NetplaySession&)=delete;
    NetplaySession& operator=(const NetplaySession&)=delete;
    // TODO(STEAM): invoke only from the original Connected/Synchronizing/
    // Synchronized/Running callbacks, FUN_0053ce70. No fake automatic connection.
    void notify(Event,int value=0);
    void idle();
    Result step(const NetInput&);
    Stats stats() const;
    bool in_rollback() const;
    int recommend_frame_wait(int local_advantage,int remote_advantage);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
