#pragma once

/**
 * @file result.h
 * @brief Rust-inspired Result<T, E> type for error handling without exceptions.
 * 
 * Provides monadic error handling suitable for -fno-exceptions environments.
 * Use CRAB_TRY macro for early-return error propagation (like Rust's ?).
 */

#include "crab/macros.h"
#include "crab/error_types.h"

#include <type_traits>
#include <utility>
#include <variant>

namespace crab {

// Forward declaration for friendship
template<typename T, typename E>
class Result;

namespace detail {

/**
 * @brief Tag type for Ok variant construction.
 */
template<typename T>
struct OkTag {
    T value;
    
    template<typename U>
    explicit OkTag(U&& v) : value(std::forward<U>(v)) {}
};

/**
 * @brief Tag type for Err variant construction.
 */
template<typename E>
struct ErrTag {
    E error;
    
    template<typename U>
    explicit ErrTag(U&& e) : error(std::forward<U>(e)) {}
};

} // namespace detail

/**
 * @brief Factory function to create an Ok result.
 * 
 * Usage: return crab::Ok(42);
 */
template<typename T>
detail::OkTag<std::decay_t<T>> Ok(T&& value) {
    return detail::OkTag<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Factory function for Ok with void/Unit result.
 * 
 * Usage: return crab::Ok();
 */
inline detail::OkTag<Unit> Ok() {
    return detail::OkTag<Unit>(Unit{});
}

/**
 * @brief Factory function to create an Err result.
 * 
 * Usage: return crab::Err(OutOfBounds{5, 3});
 */
template<typename E>
detail::ErrTag<std::decay_t<E>> Err(E&& error) {
    return detail::ErrTag<std::decay_t<E>>(std::forward<E>(error));
}

/**
 * @brief Result type for operations that can fail.
 * 
 * Models Rust's Result<T, E>. Either contains a success value (Ok) or
 * an error value (Err). Uses std::variant internally for storage.
 * 
 * @tparam T Success value type (use Unit for void operations)
 * @tparam E Error type
 * 
 * @code{cpp}
 *   Result<int, OutOfBounds> get_element(size_t i) {
 *       if (i >= size) return Err(OutOfBounds{i, size});
 *       return Ok(data[i]);
 *   }
 *   
 *   // Usage:
 *   auto result = get_element(5);
 *   if (result.is_ok()) {
 *       int val = result.unwrap();
 *   }
 *   
 *   // Or with CRAB_TRY:
 *   int val = CRAB_TRY(get_element(5));
 * @endcode
 */
template<typename T, typename E>
class [[nodiscard]] Result {
    static_assert(!std::is_same_v<T, void>, 
        "Use crab::Unit instead of void for Result<T, E>");
    
public:
    using value_type = T;
    using error_type = E;
    
    // ========================================================================
    // Constructors
    // ========================================================================
    
    /**
     * @brief Construct Ok result from OkTag.
     */
    template<typename U, 
             typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    Result(detail::OkTag<U> ok) 
        : m_storage(std::in_place_index<0>, std::move(ok.value)) {}
    
    /**
     * @brief Construct Err result from ErrTag.
     */
    template<typename U,
             typename = std::enable_if_t<std::is_convertible_v<U, E>>>
    Result(detail::ErrTag<U> err)
        : m_storage(std::in_place_index<1>, std::move(err.error)) {}
    
    // Default move/copy
    Result(Result&&) = default;
    Result(const Result&) = default;
    Result& operator=(Result&&) = default;
    Result& operator=(const Result&) = default;
    
    // ========================================================================
    // Queries
    // ========================================================================
    
    /**
     * @brief Check if result contains a success value.
     */
    [[nodiscard]] bool is_ok() const noexcept {
        return m_storage.index() == 0;
    }
    
    /**
     * @brief Check if result contains an error.
     */
    [[nodiscard]] bool is_err() const noexcept {
        return m_storage.index() == 1;
    }
    
    /**
     * @brief Boolean conversion: true if Ok.
     */
    explicit operator bool() const noexcept {
        return is_ok();
    }
    
    // ========================================================================
    // Unwrapping (panics if wrong variant)
    // ========================================================================
    
    /**
     * @brief Extract the Ok value.
     * @note Panics in debug mode if called on Err.
     */
    [[nodiscard]] T& unwrap() & {
        CRAB_ASSERT(is_ok(), "Called unwrap() on Err Result");
        return std::get<0>(m_storage);
    }
    
    [[nodiscard]] const T& unwrap() const & {
        CRAB_ASSERT(is_ok(), "Called unwrap() on Err Result");
        return std::get<0>(m_storage);
    }
    
    [[nodiscard]] T&& unwrap() && {
        CRAB_ASSERT(is_ok(), "Called unwrap() on Err Result");
        return std::get<0>(std::move(m_storage));
    }
    
    /**
     * @brief Extract the Err value.
     * @note Panics in debug mode if called on Ok.
     */
    [[nodiscard]] E& unwrap_err() & {
        CRAB_ASSERT(is_err(), "Called unwrap_err() on Ok Result");
        return std::get<1>(m_storage);
    }
    
    [[nodiscard]] const E& unwrap_err() const & {
        CRAB_ASSERT(is_err(), "Called unwrap_err() on Ok Result");
        return std::get<1>(m_storage);
    }
    
    [[nodiscard]] E&& unwrap_err() && {
        CRAB_ASSERT(is_err(), "Called unwrap_err() on Ok Result");
        return std::get<1>(std::move(m_storage));
    }
    
    // ========================================================================
    // Safe Unwrapping
    // ========================================================================
    
    /**
     * @brief Get Ok value or return default.
     */
    [[nodiscard]] T unwrap_or(T default_value) && {
        if (is_ok()) {
            return std::get<0>(std::move(m_storage));
        }
        return default_value;
    }
    
    /**
     * @brief Get Ok value or compute default from function.
     */
    template<typename F>
    [[nodiscard]] T unwrap_or_else(F&& fn) && {
        if (is_ok()) {
            return std::get<0>(std::move(m_storage));
        }
        return fn(std::get<1>(std::move(m_storage)));
    }
    
    /**
     * @brief Get a pointer to the Ok value, or nullptr if Err.
     */
    [[nodiscard]] T* ok() noexcept {
        return is_ok() ? &std::get<0>(m_storage) : nullptr;
    }
    
    [[nodiscard]] const T* ok() const noexcept {
        return is_ok() ? &std::get<0>(m_storage) : nullptr;
    }
    
    /**
     * @brief Get a pointer to the Err value, or nullptr if Ok.
     */
    [[nodiscard]] E* err() noexcept {
        return is_err() ? &std::get<1>(m_storage) : nullptr;
    }
    
    [[nodiscard]] const E* err() const noexcept {
        return is_err() ? &std::get<1>(m_storage) : nullptr;
    }
    
    // ========================================================================
    // Monadic Operations
    // ========================================================================
    
    /**
     * @brief Transform the Ok value.
     * 
     * If Ok, applies fn to the value and returns Result<U, E>.
     * If Err, returns the error unchanged.
     */
    template<typename F>
    [[nodiscard]] auto map(F&& fn) && 
        -> Result<std::invoke_result_t<F, T>, E>
    {
        using U = std::invoke_result_t<F, T>;
        if (is_ok()) {
            return crab::Ok(fn(std::get<0>(std::move(m_storage))));
        }
        return crab::Err(std::get<1>(std::move(m_storage)));
    }
    
    /**
     * @brief Transform the Err value.
     * 
     * If Err, applies fn to the error and returns Result<T, U>.
     * If Ok, returns the value unchanged.
     */
    template<typename F>
    [[nodiscard]] auto map_err(F&& fn) &&
        -> Result<T, std::invoke_result_t<F, E>>
    {
        using U = std::invoke_result_t<F, E>;
        if (is_err()) {
            return crab::Err(fn(std::get<1>(std::move(m_storage))));
        }
        return crab::Ok(std::get<0>(std::move(m_storage)));
    }
    
    /**
     * @brief Chain operations that return Result.
     * 
     * If Ok, applies fn to value (fn must return a Result with same error type).
     * If Err, returns the error unchanged.
     */
    template<typename F>
    [[nodiscard]] auto and_then(F&& fn) &&
        -> std::invoke_result_t<F, T>
    {
        using ReturnType = std::invoke_result_t<F, T>;
        static_assert(std::is_same_v<typename ReturnType::error_type, E>,
            "and_then: function must return Result with same error type E");
        
        if (is_ok()) {
            return fn(std::get<0>(std::move(m_storage)));
        }
        return crab::Err(std::get<1>(std::move(m_storage)));
    }
    
    /**
     * @brief Provide alternative on Err.
     * 
     * If Err, applies fn to error (fn must return Result<T, E2>).
     * If Ok, returns the value unchanged.
     */
    template<typename F>
    [[nodiscard]] auto or_else(F&& fn) &&
        -> std::invoke_result_t<F, E>
    {
        if (is_err()) {
            return fn(std::get<1>(std::move(m_storage)));
        }
        using ReturnType = std::invoke_result_t<F, E>;
        return crab::Ok(std::get<0>(std::move(m_storage)));
    }
    
    /**
     * @brief Pattern matching helper.
     * 
     * @code{cpp}
     *   result.match(
     *       [](int v) { return v * 2; },
     *       [](Error e) { return 0; }
     *   );
     * @endcode
     */
    template<typename OkFn, typename ErrFn>
    [[nodiscard]] auto match(OkFn&& ok_fn, ErrFn&& err_fn) && {
        if (is_ok()) {
            return ok_fn(std::get<0>(std::move(m_storage)));
        }
        return err_fn(std::get<1>(std::move(m_storage)));
    }
    
private:
    std::variant<T, E> m_storage;
};

// ============================================================================
// Convenience Aliases
// ============================================================================

/**
 * @brief Result with Unit as success type (for void operations).
 */
template<typename E>
using VoidResult = Result<Unit, E>;

} // namespace crab
