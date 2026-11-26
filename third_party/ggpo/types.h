#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <stdexcept>
using uint8=unsigned char;using uint16=unsigned short;using uint32=unsigned int;
using byte=unsigned char;using int8=char;using int16=short;using int32=int;
class Platform {public:static int GetProcessID(){return 0;}};
#include "log.h"
#define ASSERT(x) do {if(!(x))throw std::runtime_error("GGPO invariant: " #x);}while(false)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define MAX_INT 0x0effffff
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#ifndef FALSE
#define FALSE false
#endif
