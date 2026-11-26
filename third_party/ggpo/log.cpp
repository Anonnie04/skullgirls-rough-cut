#include "types.h"
void Log(const char*, ...) {}
void Logv(const char*,va_list) {}
void Logv(FILE*,const char*,va_list) {}
void LogFlush() {}
