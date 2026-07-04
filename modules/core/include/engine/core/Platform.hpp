#pragma once

#if defined(_WIN32)
    #define ENGINE_PLATFORM_WINDOWS 1
#else
    #define ENGINE_PLATFORM_WINDOWS 0
#endif

#if defined(__ANDROID__)
    #define ENGINE_PLATFORM_ANDROID 1
    #define ENGINE_PLATFORM_LINUX   0
#elif defined(__linux__)
    #define ENGINE_PLATFORM_ANDROID 0
    #define ENGINE_PLATFORM_LINUX   1
#else
    #define ENGINE_PLATFORM_ANDROID 0
    #define ENGINE_PLATFORM_LINUX   0
#endif

#if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IOS
        #define ENGINE_PLATFORM_IOS   1
        #define ENGINE_PLATFORM_MACOS 0
    #else
        #define ENGINE_PLATFORM_IOS   0
        #define ENGINE_PLATFORM_MACOS 1
    #endif
#else
    #define ENGINE_PLATFORM_IOS   0
    #define ENGINE_PLATFORM_MACOS 0
#endif