#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace skullgirls {
// SDL3's public gamepad button/axis order; no SDL types cross the simulation boundary.
struct ControllerSample {
 bool connected=false;
 std::uint32_t buttons=0;
 std::array<short,6> axes{};
 std::string name;
};
struct ControllerFrame {unsigned held=0,pressed=0;std::uint32_t buttons_pressed=0;bool disconnected=false;};
struct ControllerMapper {
 unsigned previous=0;std::uint32_t buttons=0;bool connected=false,armed=false,left_trigger=false,right_trigger=false;
 unsigned navigation_ticks=0;
 void inhibit(){armed=false;left_trigger=right_trigger=false;previous=buttons=navigation_ticks=0;}
 ControllerFrame update(const ControllerSample& s,bool focused){
  ControllerFrame out;out.disconnected=connected&&!s.connected;connected=s.connected;
  unsigned mask=0;
  if(s.connected){
   const auto b=[&](unsigned i){return (s.buttons&(1u<<i))!=0;};
   bool left=b(13)||s.axes[0]<-8000,right=b(14)||s.axes[0]>8000;
   bool down=b(12)||s.axes[1]>8000,up=b(11)||s.axes[1]<-8000;
   if(left!=right)mask|=left?1:2;if(down!=up)mask|=down?4:8;
   if(b(2))mask|=16;if(b(3))mask|=32;if(b(0))mask|=128;if(b(1))mask|=256;
   // Match the previously inspected Work layout: RT=HP, LT=HK.
   right_trigger=s.axes[5]>(right_trigger?6144:8192);left_trigger=s.axes[4]>(left_trigger?6144:8192);
   if(right_trigger)mask|=64;if(left_trigger)mask|=512;
   if(b(9))mask|=16|32|64;if(b(10))mask|=128|256|512;
  }
  if(!focused||!s.connected){inhibit();return out;}
  // Reconnection/focus return cannot turn a held button into a new command.
  if(!armed){if(!mask&&!s.buttons)armed=true;previous=mask;buttons=s.buttons;return out;}
  out.held=mask;out.pressed=mask&~previous;out.buttons_pressed=s.buttons&~buttons;
  if((mask&15)!=(previous&15))navigation_ticks=0;
  else if(mask&15){++navigation_ticks;if(navigation_ticks>=18&&(navigation_ticks-18)%6==0)out.pressed|=mask&15;}
  previous=mask;buttons=s.buttons;return out;
 }
};
}
