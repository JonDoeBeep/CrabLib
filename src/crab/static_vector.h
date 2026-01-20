#pragma once

/**
 * @file static_vector.h
 * @brief Fixed-capacity vector with stack allocation (no heap).
 * 
 * Provides a std::vector-like interface with compile-time fixed capacity.
 * Ideal for real-time systems where heap allocation is undesirable.
 * 
 */

#include "crab/macros.h"
#include "crab/result.h"
#include "crab/option.h"
#include "crab/error_types.h"

#include <cstddef>
#include <initializer_list>
#include <new>
#include <type_traits>
#include <utility>

namespace crab {

/**
 * @brief Fixed-capacity vector with no heap allocation.
 * 
 * Stores elements in-place using aligned storage. Suitable for
 * real-time and embedded systems.
 * 
 * @tparam T Element type
 * @tparam Capacity Maximum number of elements
 */
template <typename T, std::size_t Capacity>
class StaticVector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;

    // ========================================================================
    // Constructors / Destructor
    // ========================================================================

    /** @brief Default constructor: creates empty vector. */
    StaticVector() noexcept = default;

    /**
     * @brief Construct from initializer list.
     * @return Ok if successful, Err if init.size() > Capacity
     */
    static Result<StaticVector, CapacityExceeded> from_list(std::initializer_list<T> init) {
        if (init.size() > Capacity) {
            return Err(CapacityExceeded{init.size(), Capacity});
        }
        StaticVector vec;
        for (const auto& value : init) {
            vec.push_back_unchecked(value);
        }
        return Ok(std::move(vec));
    }

    /**
     * @brief Construct from initializer list (throws on overflow).
     * @note For compatibility: prefer from_list() for error handling.
     */
    StaticVector(std::initializer_list<T> init) {
        CRAB_ASSERT(init.size() <= Capacity, "StaticVector initializer list too large");
        for (const auto& value : init) {
            push_back_unchecked(value);
        }
    }

    /**
     * @brief Copy constructor: deep copies all elements.
     */
    StaticVector(const StaticVector& other) : m_size(0) {
        for (size_type i = 0; i < other.m_size; ++i) {
            push_back_unchecked(other[i]);
        }
    }

    /**
     * @brief Move constructor: moves all elements.
     */
    StaticVector(StaticVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>) 
        : m_size(0) {
        for (size_type i = 0; i < other.m_size; ++i) {
            emplace_back_unchecked(std::move(other[i]));
        }
        other.clear();
    }

    /**
     * @brief Copy assignment operator.
     */
    StaticVector& operator=(const StaticVector& other) {
        if (this != &other) {
            clear();
            for (size_type i = 0; i < other.m_size; ++i) {
                push_back_unchecked(other[i]);
            }
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     */
    StaticVector& operator=(StaticVector&& other) noexcept(
        std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>) {
        if (this != &other) {
            clear();
            for (size_type i = 0; i < other.m_size; ++i) {
                emplace_back_unchecked(std::move(other[i]));
            }
            other.clear();
        }
        return *this;
    }

    /** @brief Destructor: properly destroys all elements. */
    ~StaticVector() { clear(); }

    // ========================================================================
    // Size & Capacity
    // ========================================================================

    [[nodiscard]] size_type size() const noexcept { return m_size; }
    [[nodiscard]] constexpr size_type capacity() const noexcept { return Capacity; }
    [[nodiscard]] bool empty() const noexcept { return m_size == 0; }
    [[nodiscard]] bool is_full() const noexcept { return m_size >= Capacity; }
    [[nodiscard]] size_type remaining() const noexcept { return Capacity - m_size; }

    // ========================================================================
    // Iterators
    // ========================================================================

    [[nodiscard]] iterator begin() noexcept { return data(); }
    [[nodiscard]] iterator end() noexcept { return data() + m_size; }
    [[nodiscard]] const_iterator begin() const noexcept { return data(); }
    [[nodiscard]] const_iterator end() const noexcept { return data() + m_size; }
    [[nodiscard]] const_iterator cbegin() const noexcept { return data(); }
    [[nodiscard]] const_iterator cend() const noexcept { return data() + m_size; }

    // ========================================================================
    // Element Access (Safe)
    // ========================================================================

    /**
     * @brief Access element with bounds checking, returning Result.
     */
    [[nodiscard]] Result<std::reference_wrapper<T>, OutOfBounds> get(size_type index) noexcept {
        if (index >= m_size) {
            return Err(OutOfBounds{index, m_size});
        }
        return Ok(std::ref(data()[index]));
    }

    [[nodiscard]] Result<std::reference_wrapper<const T>, OutOfBounds> 
    get(size_type index) const noexcept {
        if (index >= m_size) {
            return Err(OutOfBounds{index, m_size});
        }
        return Ok(std::cref(data()[index]));
    }

    /**
     * @brief Element access (bounds-checked unless CRAB_UNSAFE_FAST).
     */
    [[nodiscard]] T& operator[](size_type index) noexcept {
        CRAB_ASSERT(index < m_size, "StaticVector index out of bounds");
        return data()[index];
    }
    
    [[nodiscard]] const T& operator[](size_type index) const noexcept {
        CRAB_ASSERT(index < m_size, "StaticVector index out of bounds");
        return data()[index];
    }
    
    /**
     * @brief Unchecked element access (explicit unsafe opt-in).
     */
    [[nodiscard]] T& unchecked(size_type index) noexcept {
        return data()[index];
    }
    
    [[nodiscard]] const T& unchecked(size_type index) const noexcept {
        return data()[index];
    }

    /**
     * @brief Access first element.
     */
    [[nodiscard]] Option<std::reference_wrapper<T>> front_opt() noexcept {
        if (empty()) return None;
        return Some(std::ref(data()[0]));
    }

    [[nodiscard]] T& front() noexcept {
        CRAB_DEBUG_ASSERT(!empty(), "front() called on empty StaticVector");
        return data()[0];
    }
    
    [[nodiscard]] const T& front() const noexcept {
        CRAB_DEBUG_ASSERT(!empty(), "front() called on empty StaticVector");
        return data()[0];
    }

    /**
     * @brief Access last element.
     */
    [[nodiscard]] Option<std::reference_wrapper<T>> back_opt() noexcept {
        if (empty()) return None;
        return Some(std::ref(data()[m_size - 1]));
    }

    [[nodiscard]] T& back() noexcept {
        CRAB_DEBUG_ASSERT(!empty(), "back() called on empty StaticVector");
        return data()[m_size - 1];
    }
    
    [[nodiscard]] const T& back() const noexcept {
        CRAB_DEBUG_ASSERT(!empty(), "back() called on empty StaticVector");
        return data()[m_size - 1];
    }

    /** @brief Get pointer to underlying storage. */
    [[nodiscard]] T* data() noexcept { 
        return std::launder(reinterpret_cast<T*>(m_storage)); 
    }
    
    [[nodiscard]] const T* data() const noexcept { 
        return std::launder(reinterpret_cast<const T*>(m_storage)); 
    }

    // ========================================================================
    // Modifiers (Safe)
    // ========================================================================

    /** @brief Remove all elements. */
    void clear() noexcept {
        T* storage = data();
        for (size_type i = m_size; i > 0; --i) {
            storage[i - 1].~T();
        }
        m_size = 0;
    }

    /**
     * @brief Add element by copy (checked).
     * @return Ok if successful, Err if at capacity
     */
    [[nodiscard]] Result<Unit, CapacityExceeded> try_push_back(const T& value) {
        if (m_size >= Capacity) {
            return Err(CapacityExceeded{m_size + 1, Capacity});
        }
        new (data() + m_size) T(value);
        ++m_size;
        return Ok();
    }

    /**
     * @brief Add element by move (checked).
     */
    [[nodiscard]] Result<Unit, CapacityExceeded> try_push_back(T&& value) {
        if (m_size >= Capacity) {
            return Err(CapacityExceeded{m_size + 1, Capacity});
        }
        new (data() + m_size) T(std::move(value));
        ++m_size;
        return Ok();
    }

    /**
     * @brief Construct element in-place (checked).
     */
    template <typename... Args>
    [[nodiscard]] Result<std::reference_wrapper<T>, CapacityExceeded> 
    try_emplace_back(Args&&... args) {
        if (m_size >= Capacity) {
            return Err(CapacityExceeded{m_size + 1, Capacity});
        }
        T* slot = new (data() + m_size) T(std::forward<Args>(args)...);
        ++m_size;
        return Ok(std::ref(*slot));
    }

    /**
     * @brief Add element by copy (panics if full in debug).
     * @note For compatibility: prefer try_push_back().
     */
    void push_back(const T& value) {
        CRAB_ASSERT(m_size < Capacity, "StaticVector capacity exceeded");
        push_back_unchecked(value);
    }

    void push_back(T&& value) {
        CRAB_ASSERT(m_size < Capacity, "StaticVector capacity exceeded");
        new (data() + m_size) T(std::move(value));
        ++m_size;
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        CRAB_ASSERT(m_size < Capacity, "StaticVector capacity exceeded");
        return emplace_back_unchecked(std::forward<Args>(args)...);
    }

    /**
     * @brief Remove last element.
     * @return The removed element, or None if empty
     */
    [[nodiscard]] Option<T> pop_back() noexcept {
        if (m_size == 0) {
            return None;
        }
        --m_size;
        T value = std::move(data()[m_size]);
        data()[m_size].~T();
        return Some(std::move(value));
    }

    /**
     * @brief Remove last element (void version for compatibility).
     */
    void pop_back_void() noexcept {
        if (m_size > 0) {
            --m_size;
            data()[m_size].~T();
        }
    }

    /**
     * @brief Resize the vector.
     * @return Ok if successful, Err if new_size > Capacity
     */
    [[nodiscard]] Result<Unit, CapacityExceeded> try_resize(size_type new_size) {
        if (new_size > Capacity) {
            return Err(CapacityExceeded{new_size, Capacity});
        }
        while (m_size > new_size) {
            pop_back_void();
        }
        while (m_size < new_size) {
            emplace_back_unchecked();
        }
        return Ok();
    }

    /**
     * @brief Reserve capacity (no-op for StaticVector).
     * @return Err if requested > Capacity
     */
    [[nodiscard]] Result<Unit, CapacityExceeded> try_reserve(size_type requested) {
        if (requested > Capacity) {
            return Err(CapacityExceeded{requested, Capacity});
        }
        return Ok();
    }

    // Compatibility methods (panic on failure)
    void resize(size_type new_size) {
        CRAB_ASSERT(new_size <= Capacity, "StaticVector resize exceeds capacity");
        while (m_size > new_size) pop_back_void();
        while (m_size < new_size) emplace_back_unchecked();
    }

    void reserve(size_type requested) {
        CRAB_ASSERT(requested <= Capacity, "StaticVector reserve exceeds capacity");
    }

private:
    void push_back_unchecked(const T& value) {
        new (data() + m_size) T(value);
        ++m_size;
    }

    template <typename... Args>
    T& emplace_back_unchecked(Args&&... args) {
        T* slot = new (data() + m_size) T(std::forward<Args>(args)...);
        ++m_size;
        return *slot;
    }

    alignas(T) unsigned char m_storage[sizeof(T) * Capacity]{};
    size_type m_size{0};
};

} // namespace crab