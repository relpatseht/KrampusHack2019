#pragma once

#include <cstdio>

#define assertfmt(X, FMT, ...) do{ if(!(X)){ std::printf(FMT, __VA_ARGS__); std::fflush(stdout); __debugbreak();}} while(false)
#define assert(X) assertfmt(X, #X)
