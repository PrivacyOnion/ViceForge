#pragma once

#if defined(_WIN32)
  #if defined(VICEFORGE_STATIC)
    #define VICEFORGE_API
  #elif defined(VICEFORGE_CORE_EXPORTS)
    #define VICEFORGE_API __declspec(dllexport)
  #else
    #define VICEFORGE_API __declspec(dllimport)
  #endif
  #define VICEFORGE_MOD_EXPORT extern "C" __declspec(dllexport)
#else
  #define VICEFORGE_API __attribute__((visibility("default")))
  #define VICEFORGE_MOD_EXPORT extern "C" __attribute__((visibility("default")))
#endif
