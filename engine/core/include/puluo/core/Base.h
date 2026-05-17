#pragma once

// Platform detection
#ifdef _WIN32
    #define PULUO_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define PULUO_PLATFORM_LINUX
#elif defined(__APPLE__)
    #define PULUO_PLATFORM_MACOS
#endif

// Debug break
#ifdef PULUO_PLATFORM_WINDOWS
    #define PULUO_DEBUGBREAK() __debugbreak()
#else
    #include <signal.h>
    #define PULUO_DEBUGBREAK() raise(SIGTRAP)
#endif

// Assertions
#ifdef NDEBUG
    #define PULUO_ASSERT(expr, ...)
    #define PULUO_CORE_ASSERT(expr, ...)
#else
    #define PULUO_ASSERT(expr, ...) \
        if (!(expr)) { \
            PULUO_DEBUGBREAK(); \
        }
    #define PULUO_CORE_ASSERT(expr, ...) \
        if (!(expr)) { \
            PULUO_DEBUGBREAK(); \
        }
#endif

// Bit manipulation
#define BIT(x) (1 << (x))

namespace Puluo {
} // namespace Puluo
