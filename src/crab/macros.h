#pragma once

/**
 * @file macros.h
 * @brief CrabLib assertion and utility macros.
 * 
 * Provides debug-mode assertions and helper macros for safe error handling.
 * Designed for -fno-exceptions environments.
 */

#include <cstdio>
#include <cstdlib>

namespace crab {

/**
 * @brief Called when an assertion or unwrap fails.
 * 
 * In debug builds, prints message and aborts.
 * In release builds with CRAB_UNSAFE_FAST, this is undefined behavior.
 */
[[noreturn]] inline void panic(const char* msg, const char* file, int line) {
    std::fprintf(stderr, "CRAB PANIC at %s:%d: %s\n", file, line, msg);
    std::abort();
}

} // namespace crab

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
 *       @code
 *       auto result = may_fail();
 *       if (result.is_err()) return Err(result.unwrap_err());
 *       auto val = result.unwrap();
 *       @endcode
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
    // MSVC: No statement expression support
    #error "CRAB_TRY requires GCC or Clang. Use explicit if-checks on MSVC."
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
