#pragma once
#include "controller_input.hpp"
#ifdef _WIN32
#include <windows.h>
#include <filesystem>
namespace skullgirls {
// Optional SDL3 runtime, loaded only beside this executable. SDL owns Bluetooth
// report assembly, transient gamepad configuration and restoration on close.
class WindowsControllers {
 HMODULE library=nullptr;
 struct Slot{void* pad=nullptr;std::uint32_t id=0;};
 std::array<Slot,2> slots{};
 bool initialized=false;
 bool (__cdecl *init)(std::uint32_t)=nullptr;
 bool (__cdecl *hint)(const char*,const char*)=nullptr;
 void (__cdecl *quit)(std::uint32_t)=nullptr;
 void (__cdecl *update)()=nullptr;
 std::uint32_t* (__cdecl *enumerate)(int*)=nullptr;
 void* (__cdecl *open)(std::uint32_t)=nullptr;
 void (__cdecl *close)(void*)=nullptr;
 bool (__cdecl *connected)(void*)=nullptr;
 const char* (__cdecl *name)(void*)=nullptr;
 bool (__cdecl *button)(void*,int)=nullptr;
 short (__cdecl *axis)(void*,int)=nullptr;
 void (__cdecl *free_memory)(void*)=nullptr;
 void (__cdecl *flush_events)(std::uint32_t,std::uint32_t)=nullptr;
 template<class T> bool symbol(T& f,const char* n){f=reinterpret_cast<T>(GetProcAddress(library,n));return f!=nullptr;}
public:
 std::string status="Controller runtime not initialized";
 WindowsControllers()=default;
 WindowsControllers(const WindowsControllers&)=delete;
 WindowsControllers& operator=(const WindowsControllers&)=delete;
 ~WindowsControllers(){for(auto& s:slots)if(s.pad)close(s.pad);if(initialized)quit(0x2000);if(library)FreeLibrary(library);}
 bool initialize(){
  wchar_t path[32768]{};if(!GetModuleFileNameW(nullptr,path,32768))return false;
  library=LoadLibraryExW((std::filesystem::path(path).parent_path()/L"SDL3.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
  if(!library){status="Install controller runtime with scripts/prepare-controllers.ps1";return false;}
  if(!symbol(init,"SDL_Init")||!symbol(hint,"SDL_SetHint")||!symbol(quit,"SDL_QuitSubSystem")||!symbol(update,"SDL_UpdateGamepads")||!symbol(enumerate,"SDL_GetGamepads")||!symbol(open,"SDL_OpenGamepad")||!symbol(close,"SDL_CloseGamepad")||!symbol(connected,"SDL_GamepadConnected")||!symbol(name,"SDL_GetGamepadName")||!symbol(button,"SDL_GetGamepadButton")||!symbol(axis,"SDL_GetGamepadAxis")||!symbol(free_memory,"SDL_free")||!symbol(flush_events,"SDL_FlushEvents")){status="Incompatible SDL3 runtime";return false;}
  hint("SDL_JOYSTICK_HIDAPI_STEAM","1");
  initialized=init(0x2000);status=initialized?"Controller runtime ready":"SDL3 gamepad initialization failed";return initialized;
 }
 std::array<ControllerSample,2> poll(){
  std::array<ControllerSample,2> result{};if(!initialized)return result;
  update();
  for(auto& s:slots)if(s.pad&&!connected(s.pad)){close(s.pad);s={};}
  int count=0;auto ids=enumerate(&count);
  for(int i=0;ids&&i<count;++i){
   bool known=false;for(const auto& s:slots)known|=s.pad&&s.id==ids[i];if(known)continue;
   for(auto& s:slots)if(!s.pad){s.pad=open(ids[i]);if(s.pad)s.id=ids[i];break;}
  }
  free_memory(ids);
  for(unsigned side=0;side<2;++side)if(auto p=slots[side].pad){
   auto& out=result[side];out.connected=true;if(const char* n=name(p))out.name=n;
   for(int i=0;i<26;++i)if(button(p,i))out.buttons|=1u<<i;
   for(int i=0;i<6;++i)out.axes[i]=axis(p,i);
  }
  // This backend polls state instead of consuming SDL's event queue. Drain it
  // so long sessions cannot accumulate thousands of unused joystick events.
  flush_events(0,0xffff);
  return result;
 }
};
}
#endif
