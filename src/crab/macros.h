#pragma once

/**
 * @file macros.h
 * @brief CrabLib assertion and utility macros.
 * 
 * Provides debug-mode assertions and helper macros for safe error handling.
 * Designed for -fno-exceptions environments and bare-metal embedded.
 * 
 * ## Customization
 * 
 * - `CRAB_CUSTOM_PANIC`: Define before including to use custom panic handler
 * - `CRAB_CACHE_LINE_SIZE`: Define to override default cache line (32 or 64)
 */

// ============================================================================
// Platform Configuration
// ============================================================================

/**
 * @brief Cache line size for false-sharing prevention.
 * 
 * Override by defining CRAB_CACHE_LINE_SIZE before including CrabLib.
 * Default: 32 for ARM, 64 for x86/x64.
 */
#ifndef CRAB_CACHE_LINE_SIZE
    #if defined(__arm__) || defined(__ARM_ARCH) || defined(__thumb__)
        #define CRAB_CACHE_LINE_SIZE 32
    #else
        #define CRAB_CACHE_LINE_SIZE 64
    #endif
#endif

// ============================================================================
// Panic Handler
// ============================================================================

namespace crab {

/**
 * @brief Panic handler signature.
 */
using PanicHandler = void(*)(const char* msg, const char* file, int line);

} // namespace crab

#ifdef CRAB_CUSTOM_PANIC

// User-defined panic handler: user must define crab_panic_handler()
// Example:
//   [[noreturn]] void crab_panic_handler(const char* msg, const char* file, int line) {
//       my_uart_print("PANIC: ", msg);
//       while(1) { __WFI(); }  // Halt on ARM
//   }
extern "C" [[noreturn]] void crab_panic_handler(const char* msg, const char* file, int line);

namespace crab {
[[noreturn]] inline void panic(const char* msg, const char* file, int line) {
    crab_panic_handler(msg, file, line);
}
} // namespace crab

#else

// Default panic handler using stdio (for desktop/Linux targets)
#include <cstdio>
#include <cstdlib>

namespace crab {

/**
 * @brief Default panic handler: prints message and aborts.
 * 
 * To use a custom handler (e.g., for bare-metal), define CRAB_CUSTOM_PANIC
 * and implement crab_panic_handler() in your code.
 */
[[noreturn]] inline void panic(const char* msg, const char* file, int line) {
    std::fprintf(stderr, "CRAB PANIC at %s:%d: %s\n", file, line, msg);
    std::abort();
}

} // namespace crab

#endif // CRAB_CUSTOM_PANIC

// ============================================================================
// Debug Assertions
// ============================================================================

#if defined(NDEBUG) && defined(CRAB_UNSAFE_FAST)
    // Release + unsafe fast mode: no checks at all (maximum performance)
    #define CRAB_DEBUG_ASSERT(cond, msg) ((void)0)
    #define CRAB_ASSERT(cond, msg) ((void)0)
#elif defined(NDEBUG)
    // Release mode: keep critical assertions, skip debug ones
    #define CRAB_DEBUG_ASSERT(cond, msg) ((void)0)
    #define CRAB_ASSERT(cond, msg) \
        do { if (!(cond)) { ::crab::panic(msg, __FILE__, __LINE__); } } while(0)
#else
    // Debug mode: all assertions enabled
    #define CRAB_DEBUG_ASSERT(cond, msg) \
        do { if (!(cond)) { ::crab::panic(msg, __FILE__, __LINE__); } } while(0)
    #define CRAB_ASSERT(cond, msg) \
        do { if (!(cond)) { ::crab::panic(msg, __FILE__, __LINE__); } } while(0)
#endif

// ============================================================================
// Error Propagation (like Rust's ? operator)
// ============================================================================

/**
 * @brief Early return if Result is Err.
 * 
 * Usage:
 *   Result<int, Error> foo() {
 *       auto val = CRAB_TRY(may_fail());
 *       return Result<int, Error>::Ok(val + 1);
 *   }
 * 
 * Expands to unwrap-or-return-error pattern.
 * 
 * @note Requires GCC/Clang statement expressions ({ }).
 */
#if defined(__GNUC__) || defined(__clang__)
#define CRAB_TRY(expr) \
    ({ \
        auto&& _result = (expr); \
        if (_result.is_err()) { \
            return decltype(_result)(crab::Err(_result.unwrap_err())); \
        } \
        _result.unwrap(); \
    })
#else
    #error "CRAB_TRY requires GCC or Clang. Use explicit if-checks on other compilers."
#endif

/**
 * @brief Unwrap or return a default value.
 */
#define CRAB_UNWRAP_OR(expr, default_val) \
    ((expr).is_ok() ? (expr).unwrap() : (default_val))

/**
 * @brief Intentionally mark unreachable code.
 */
#if defined(__GNUC__) || defined(__clang__)
    #define CRAB_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    #define CRAB_UNREACHABLE() __assume(false)
#else
    #define CRAB_UNREACHABLE() ::crab::panic("unreachable code reached", __FILE__, __LINE__)
#endif
