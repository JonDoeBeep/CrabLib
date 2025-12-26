#pragma once

/**
 * @file option.h
 * @brief Ergonomic Option<T> type with Rust-inspired monadic interface.
 * 
 * Wraps std::optional with a more expressive API including map, and_then,
 * match, and safe unwrapping.
 */

#include "crab/macros.h"
#include "crab/error_types.h"

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace crab {

// Forward declaration for Result integration
template<typename T, typename E>
class Result;

template<typename T>
class Option;

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create an Option containing a value.
 */
template<typename T>
Option<std::decay_t<T>> Some(T&& value) {
    return Option<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Tag type for constructing empty Option.
 */
struct NoneType {};

/**
 * @brief Empty Option constant.
 */
inline constexpr NoneType None{};

/**
 * @brief Optional value with Rust-inspired API.
 * 
 * More ergonomic than std::optional with monadic operations and
 * pattern matching support.
 * 
 * @tparam T Value type
 * 
 * @code{cpp}
 *   Option<int> find_value(int key) {
 *       if (key < 0) return None;
 *       return Some(key * 2);
 *   }
 *   
 *   // Monadic chaining
 *   auto result = find_value(5)
 *       .map([](int v) { return v + 1; })
 *       .unwrap_or(0);
 *   
 *   // Pattern matching
 *   find_value(5).match(
 *       [](int v) { std::cout << "Got: " << v; },
 *       []() { std::cout << "None"; }
 *   );
 * @endcode
 */
template<typename T>
class Option {
public:
    using value_type = T;
    
    // ========================================================================
    // Constructors
    // ========================================================================
    
    /**
     * @brief Default constructor: creates empty Option.
     */
    constexpr Option() noexcept : m_inner(std::nullopt) {}
    
    /**
     * @brief Construct empty Option from None.
     */
    constexpr Option(NoneType) noexcept : m_inner(std::nullopt) {}
    
    /**
     * @brief Construct from value.
     */
    template<typename U = T,
             typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    constexpr Option(U&& value) : m_inner(std::forward<U>(value)) {}
    
    /**
     * @brief Construct from std::optional.
     */
    constexpr Option(std::optional<T> opt) : m_inner(std::move(opt)) {}
    
    // Default copy/move
    Option(const Option&) = default;
    Option(Option&&) = default;
    Option& operator=(const Option&) = default;
    Option& operator=(Option&&) = default;
    
    /**
     * @brief Assign None.
     */
    Option& operator=(NoneType) noexcept {
        m_inner = std::nullopt;
        return *this;
    }
    
    // ========================================================================
    // Queries
    // ========================================================================
    
    /**
     * @brief Check if Option contains a value.
     */
    [[nodiscard]] constexpr bool is_some() const noexcept {
        return m_inner.has_value();
    }
    
    /**
     * @brief Check if Option is empty.
     */
    [[nodiscard]] constexpr bool is_none() const noexcept {
        return !m_inner.has_value();
    }
    
    /**
     * @brief Boolean conversion: true if Some.
     */
    explicit constexpr operator bool() const noexcept {
        return is_some();
    }
    
    // ========================================================================
    // Unwrapping (panics if None)
    // ========================================================================
    
    /**
     * @brief Extract the value, panicking if None.
     */
    [[nodiscard]] T& unwrap() & {
        CRAB_ASSERT(is_some(), "Called unwrap() on None Option");
        return *m_inner;
    }
    
    [[nodiscard]] const T& unwrap() const & {
        CRAB_ASSERT(is_some(), "Called unwrap() on None Option");
        return *m_inner;
    }
    
    [[nodiscard]] T&& unwrap() && {
        CRAB_ASSERT(is_some(), "Called unwrap() on None Option");
        return std::move(*m_inner);
    }
    
    // ========================================================================
    // Safe Unwrapping
    // ========================================================================
    
    /**
     * @brief Get value or return default.
     */
    [[nodiscard]] T unwrap_or(T default_value) && {
        if (is_some()) {
            return std::move(*m_inner);
        }
        return default_value;
    }
    
    /**
     * @brief Get value or compute default from function.
     */
    template<typename F>
    [[nodiscard]] T unwrap_or_else(F&& fn) && {
        if (is_some()) {
            return std::move(*m_inner);
        }
        return fn();
    }
    
    /**
     * @brief Get pointer to value, or nullptr if None.
     */
    [[nodiscard]] T* as_ptr() noexcept {
        return is_some() ? &(*m_inner) : nullptr;
    }
    
    [[nodiscard]] const T* as_ptr() const noexcept {
        return is_some() ? &(*m_inner) : nullptr;
    }
    
    // ========================================================================
    // Monadic Operations
    // ========================================================================
    
    /**
     * @brief Transform the contained value.
     * 
     * If Some, applies fn to value and returns Option<U>.
     * If None, returns None.
     */
    template<typename F>
    [[nodiscard]] auto map(F&& fn) && 
        -> Option<std::invoke_result_t<F, T>>
    {
        using U = std::invoke_result_t<F, T>;
        if (is_some()) {
            return Some(fn(std::move(*m_inner)));
        }
        return None;
    }
    
    /**
     * @brief Chain operations that return Option.
     * 
     * If Some, applies fn to value (fn must return Option<U>).
     * If None, returns None.
     */
    template<typename F>
    [[nodiscard]] auto and_then(F&& fn) &&
        -> std::invoke_result_t<F, T>
    {
        if (is_some()) {
            return fn(std::move(*m_inner));
        }
        return None;
    }
    
    /**
     * @brief Provide alternative on None.
     * 
     * If None, applies fn (fn must return Option<T>).
     * If Some, returns self unchanged.
     */
    template<typename F>
    [[nodiscard]] Option or_else(F&& fn) && {
        if (is_none()) {
            return fn();
        }
        return std::move(*this);
    }
    
    /**
     * @brief Take the value, leaving None in its place.
     */
    [[nodiscard]] Option take() noexcept {
        Option result = std::move(*this);
        m_inner = std::nullopt;
        return result;
    }
    
    /**
     * @brief Replace the value, returning the old one.
     */
    Option replace(T value) {
        Option old = std::move(*this);
        m_inner = std::move(value);
        return old;
    }
    
    /**
     * @brief Filter the Option based on predicate.
     * 
     * If Some and predicate returns true, returns Some.
     * Otherwise returns None.
     */
    template<typename F>
    [[nodiscard]] Option filter(F&& predicate) && {
        if (is_some() && predicate(*m_inner)) {
            return std::move(*this);
        }
        return None;
    }
    
    // ========================================================================
    // Pattern Matching
    // ========================================================================
    
    /**
     * @brief Pattern match on the Option.
     * 
     * @param some_fn Called with value if Some
     * @param none_fn Called if None
     * @return Result of whichever branch is taken
     * 
     * @code{cpp}
     *   opt.match(
     *       [](int v) { return v * 2; },
     *       []() { return 0; }
     *   );
     * @endcode
     */
    template<typename SomeFn, typename NoneFn>
    [[nodiscard]] auto match(SomeFn&& some_fn, NoneFn&& none_fn) && {
        if (is_some()) {
            return some_fn(std::move(*m_inner));
        }
        return none_fn();
    }
    
    template<typename SomeFn, typename NoneFn>
    [[nodiscard]] auto match(SomeFn&& some_fn, NoneFn&& none_fn) const & {
        if (is_some()) {
            return some_fn(*m_inner);
        }
        return none_fn();
    }
    
    // ========================================================================
    // Conversion to Result
    // ========================================================================
    
    /**
     * @brief Convert to Result, using provided error if None.
     */
    template<typename E>
    [[nodiscard]] Result<T, E> ok_or(E error) &&;
    
    /**
     * @brief Convert to Result, computing error from function if None.
     */
    template<typename F>
    [[nodiscard]] auto ok_or_else(F&& fn) &&;
    
    // ========================================================================
    // Comparison
    // ========================================================================
    
    bool operator==(const Option& other) const {
        return m_inner == other.m_inner;
    }
    
    bool operator!=(const Option& other) const {
        return m_inner != other.m_inner;
    }
    
    bool operator==(NoneType) const noexcept {
        return is_none();
    }
    
    bool operator!=(NoneType) const noexcept {
        return is_some();
    }
    
    // ========================================================================
    // std::optional Interop
    // ========================================================================
    
    /**
     * @brief Convert to std::optional.
     */
    [[nodiscard]] std::optional<T>& as_std() & noexcept { 
        return m_inner; 
    }
    
    [[nodiscard]] const std::optional<T>& as_std() const & noexcept { 
        return m_inner; 
    }
    
    [[nodiscard]] std::optional<T>&& as_std() && noexcept { 
        return std::move(m_inner); 
    }
    
private:
    std::optional<T> m_inner;
};

// ============================================================================
// Result Integration (defined after Result is available)
// ============================================================================

} // namespace crab

// Include Result for ok_or integration
#include "crab/result.h"

namespace crab {

template<typename T>
template<typename E>
Result<T, E> Option<T>::ok_or(E error) && {
    if (is_some()) {
        return Ok(std::move(*m_inner));
    }
    return Err(std::move(error));
}

template<typename T>
template<typename F>
auto Option<T>::ok_or_else(F&& fn) && {
    using E = std::invoke_result_t<F>;
    if (is_some()) {
        return Result<T, E>(Ok(std::move(*m_inner)));
    }
    return Result<T, E>(Err(fn()));
}

} // namespace crab
