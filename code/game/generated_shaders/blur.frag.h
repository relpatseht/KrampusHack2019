// Generated by GLSLToC. Do no modify.

#pragma once

#include "ShaderReflection.h"

#ifdef EXPORT
# error "'EXPORT' already defined."
#endif //#ifdef EXPORT

#if BUILD_DLL
# define EXPORT __declspec(dllexport)
#elif USE_DLL //#if BUILD_DLL
# define EXPORT __declspec(dllimport)
#else //#elif USE_DLL //#if BUILD_DLL
# define EXPORT
#endif //#else //#elif USE_DLL //#if BUILD_DLL

#if EXPORT_SHADER_SYM
extern "C" EXPORT const char * const shader_sym;
#endif //#if EXPORT_SHADER_SYM

extern "C" EXPORT const ShaderModule blur_frag_shader;

#undef EXPORT
