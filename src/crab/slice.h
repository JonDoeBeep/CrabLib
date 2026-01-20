#pragma once

/**
 * @file slice.h
 * @brief Bounds-checked non-owning view for contiguous memory.
 * 
 * C++17 replacement for std::span with runtime bounds checking.
 * Critical for memory safety in buffer operations.
 */

#include "crab/macros.h"
#include "crab/result.h"
#include "crab/error_types.h"

#include <cstddef>
#include <cstring>
#include <iterator>
#include <type_traits>

namespace crab {

/**
 * @brief Bounds-checked view into contiguous memory.
 * 
 * Like std::span but for C++17, with mandatory bounds checking.
 * Does NOT own the data. Caller must ensure the underlying buffer
 * outlives the Slice.
 * 
 * @tparam T Element type (can be const for read-only slices)
 * 
 * @code{cpp}
 *   std::vector<uint8_t> buffer(100);
 *   crab::Slice<uint8_t> slice(buffer);  // From lvalue container
 *   
 *   auto elem = slice.get(5);  // Returns Result<ref, OutOfBounds>
 *   if (elem) {
 *       *elem.unwrap() = 42;
 *   }
 *   
 *   // This won't compile, prevents dangling:
 *   // crab::Slice<uint8_t> bad(std::vector<uint8_t>(100));  // DELETED
 * @endcode
 */
template<typename T>
class Slice {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;
    
    // ========================================================================
    // Constructors
    // ========================================================================
    
    /**
     * @brief Default constructor: empty slice.
     */
    constexpr Slice() noexcept : m_data(nullptr), m_size(0) {}
    
    /**
     * @brief Construct from raw pointer and size (explicit).
     * 
     * @param data Pointer to first element
     * @param size Number of elements
     */
    explicit constexpr Slice(T* data, size_type size) noexcept 
        : m_data(data), m_size(size) {}
    
    /**
     * @brief Construct from C-style array.
     */
    template<std::size_t N>
    constexpr Slice(T (&arr)[N]) noexcept : m_data(arr), m_size(N) {}
    
    /**
     * @brief Construct from container with .data() and .size() (LVALUE ONLY).
     * 
     * Prevents dangling references by rejecting temporary containers.
     */
    template<typename Container,
             typename = std::enable_if_t<
                 !std::is_same_v<std::decay_t<Container>, Slice> &&
                 std::is_convertible_v<
                     decltype(std::declval<Container&>().data()), T*>>>
    Slice(Container& c) noexcept 
        : m_data(c.data()), m_size(c.size()) {}
    
    /**
     * @brief DELETED: Prevent construction from temporary containers.
     * 
     * This prevents common dangling pointer bugs:
     * @code{cpp}
     *   Slice<int> bad(std::vector<int>{1,2,3});  // Compile error!
     * @endcode
     * 
     * Note: Does NOT prevent Slice rvalues (needed for Result<Slice, E>).
     */
    template<typename Container,
             typename = std::enable_if_t<
                 !std::is_same_v<std::decay_t<Container>, Slice> &&
                 std::is_convertible_v<
                     decltype(std::declval<Container&>().data()), T*>>>
    Slice(Container&& c) = delete;
    
    // Copy/move are safe (just copies pointer+size)
    Slice(const Slice&) = default;
    Slice& operator=(const Slice&) = default;
    Slice(Slice&&) = default;
    Slice& operator=(Slice&&) = default;
    
    // ========================================================================
    // Size & Emptiness
    // ========================================================================
    
    [[nodiscard]] constexpr size_type size() const noexcept { return m_size; }
    [[nodiscard]] constexpr bool is_empty() const noexcept { return m_size == 0; }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }
    
    // ========================================================================
    // Element Access (Safe)
    // ========================================================================
    
    /**
     * @brief Get element with bounds checking, returning Result.
     * 
     * @param index Element index
     * @return Result containing reference or OutOfBounds error
     */
    [[nodiscard]] Result<std::reference_wrapper<T>, OutOfBounds> 
    get(size_type index) noexcept {
        if (index >= m_size) {
            return Err(OutOfBounds{index, m_size});
        }
        return Ok(std::ref(m_data[index]));
    }
    
    [[nodiscard]] Result<std::reference_wrapper<const T>, OutOfBounds>
    get(size_type index) const noexcept {
        if (index >= m_size) {
            return Err(OutOfBounds{index, m_size});
        }
        return Ok(std::cref(m_data[index]));
    }
    
    /**
     * @brief Element access (bounds-checked unless CRAB_UNSAFE_FAST).
     * 
     * Use unchecked() for explicit unsafe access in critical hot paths.
     */
    [[nodiscard]] reference operator[](size_type index) noexcept {
        CRAB_ASSERT(index < m_size, "Slice index out of bounds");
        return m_data[index];
    }
    
    [[nodiscard]] const_reference operator[](size_type index) const noexcept {
        CRAB_ASSERT(index < m_size, "Slice index out of bounds");
        return m_data[index];
    }
    
    /**
     * @brief Unchecked element access (explicit unsafe opt-in).
     * 
     * @warning No bounds checking. Caller accepts UB risk.
     */
    [[nodiscard]] reference unchecked(size_type index) noexcept {
        return m_data[index];
    }
    
    [[nodiscard]] const_reference unchecked(size_type index) const noexcept {
        return m_data[index];
    }
    
    /**
     * @brief Get first element (panics if empty in debug).
     */
    [[nodiscard]] reference front() noexcept {
        CRAB_DEBUG_ASSERT(!is_empty(), "front() called on empty Slice");
        return m_data[0];
    }
    
    [[nodiscard]] const_reference front() const noexcept {
        CRAB_DEBUG_ASSERT(!is_empty(), "front() called on empty Slice");
        return m_data[0];
    }
    
    /**
     * @brief Get last element (panics if empty in debug).
     */
    [[nodiscard]] reference back() noexcept {
        CRAB_DEBUG_ASSERT(!is_empty(), "back() called on empty Slice");
        return m_data[m_size - 1];
    }
    
    [[nodiscard]] const_reference back() const noexcept {
        CRAB_DEBUG_ASSERT(!is_empty(), "back() called on empty Slice");
        return m_data[m_size - 1];
    }
    
    // ========================================================================
    // Raw Access
    // ========================================================================
    
    [[nodiscard]] constexpr pointer data() noexcept { return m_data; }
    [[nodiscard]] constexpr const_pointer data() const noexcept { return m_data; }
    
    // ========================================================================
    // Subslicing
    // ========================================================================
    
    /**
     * @brief Extract a subslice with bounds checking.
     * 
     * @param start Starting index (inclusive)
     * @param end Ending index (exclusive)
     * @return Result containing subslice or OutOfBounds error
     */
    [[nodiscard]] Result<Slice, OutOfBounds> 
    subslice(size_type start, size_type end) const noexcept {
        if (start > end) {
            return Err(OutOfBounds{start, end}); // start > end case
        }
        if (end > m_size) {
            return Err(OutOfBounds{end, m_size}); // bounds case
        }
        return Ok(Slice(m_data + start, end - start));
    }
    
    /**
     * @brief Get first n elements (clamped to size).
     */
    [[nodiscard]] Slice first(size_type n) const noexcept {
        const size_type count = (n > m_size) ? m_size : n;
        return Slice(m_data, count);
    }
    
    /**
     * @brief Get last n elements (clamped to size).
     */
    [[nodiscard]] Slice last(size_type n) const noexcept {
        const size_type count = (n > m_size) ? m_size : n;
        return Slice(m_data + (m_size - count), count);
    }
    
    /**
     * @brief Skip first n elements (clamped to size).
     */
    [[nodiscard]] Slice skip(size_type n) const noexcept {
        const size_type offset = (n > m_size) ? m_size : n;
        return Slice(m_data + offset, m_size - offset);
    }
    
    // ========================================================================
    // Copying
    // ========================================================================
    
    /**
     * @brief Copy contents to another slice.
     * 
     * @param dest Destination slice (must be at least as large as source)
     * @return Ok if copy succeeded, Err if dest is too small
     * @note Uses memmove, safe for overlapping ranges.
     */
    Result<Unit, OutOfBounds> copy_to(Slice<std::remove_const_t<T>> dest) const noexcept {
        if (dest.size() < m_size) {
            return Err(OutOfBounds{m_size, dest.size()});
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memmove(dest.data(), m_data, m_size * sizeof(T));
        } else {
            for (size_type i = 0; i < m_size; ++i) {
                dest[i] = m_data[i];
            }
        }
        return Ok();
    }
    
    /**
     * @brief Copy from another slice into this one.
     * 
     * @param src Source slice (must fit in this slice)
     * @return Ok if copy succeeded, Err if source is too large
     * @note Uses memmove, safe for overlapping ranges.
     * @note Only available for non-const Slice.
     */
    template<typename U = T, typename = std::enable_if_t<!std::is_const_v<U>>>
    Result<Unit, OutOfBounds> copy_from(Slice<const T> src) noexcept {
        if (src.size() > m_size) {
            return Err(OutOfBounds{src.size(), m_size});
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memmove(m_data, src.data(), src.size() * sizeof(T));
        } else {
            for (size_type i = 0; i < src.size(); ++i) {
                m_data[i] = src[i];
            }
        }
        return Ok();
    }
    
    // ========================================================================
    // Iteration
    // ========================================================================
    
    [[nodiscard]] iterator begin() noexcept { return m_data; }
    [[nodiscard]] iterator end() noexcept { return m_data + m_size; }
    [[nodiscard]] const_iterator begin() const noexcept { return m_data; }
    [[nodiscard]] const_iterator end() const noexcept { return m_data + m_size; }
    [[nodiscard]] const_iterator cbegin() const noexcept { return m_data; }
    [[nodiscard]] const_iterator cend() const noexcept { return m_data + m_size; }
    
private:
    T* m_data;
    size_type m_size;
};

// ============================================================================
// Type Aliases
// ============================================================================

template<typename T>
using MutSlice = Slice<T>;

template<typename T>
using ConstSlice = Slice<const T>;

/// Byte slice for raw buffer operations
using ByteSlice = Slice<uint8_t>;
using ConstByteSlice = Slice<const uint8_t>;

// ============================================================================
// Deduction Guides (C++17)
// ============================================================================

template<typename T, std::size_t N>
Slice(T (&)[N]) -> Slice<T>;

template<typename Container>
Slice(Container&) -> Slice<typename Container::value_type>;

} // namespace crab
