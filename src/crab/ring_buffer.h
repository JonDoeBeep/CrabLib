#pragma once

/**
 * @file ring_buffer.h
 * @brief Lock-free SPSC (Single Producer Single Consumer) ring buffer.
 * 
 * Uses proper std::atomic memory ordering for safe cross-thread access
 * on ARM and x86. Fixed capacity, no heap allocation.
 * 
 * @warning Only safe for ONE producer thread and ONE consumer thread.
 *          For multi-producer or multi-consumer, use Mutex<StaticVector> instead.
 */

#include "crab/option.h"
#include "crab/macros.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace crab {

/**
 * @brief Lock-free SPSC ring buffer with fixed capacity.
 * 
 * Provides wait-free push/pop operations for real-time systems.
 * Uses acquire/release memory ordering for proper synchronization.
 * 
 * @tparam T Element type (should be trivially copyable for best performance)
 * @tparam Capacity Maximum number of elements (actual usable = Capacity - 1)
 * 
 * @code{cpp}
 *   // Producer thread
 *   crab::StaticRingBuffer<SensorData, 64> buffer;
 *   buffer.try_push(SensorData{...});
 *   
 *   // Consumer thread
 *   if (auto data = buffer.try_pop(); data.is_some()) {
 *       process(data.unwrap());
 *   }
 * @endcode
 * 
 * @note The actual capacity is Capacity - 1 to distinguish full from empty.
 */
template<typename T, std::size_t Capacity>
class StaticRingBuffer {
    static_assert(Capacity >= 2, "StaticRingBuffer capacity must be at least 2");
    
public:
    using value_type = T;
    using size_type = std::size_t;
    
    /**
     * @brief Default constructor, creates empty buffer.
     */
    StaticRingBuffer() noexcept : m_head(0), m_tail(0) {
        // Default-construct all elements if not trivially constructible
        if constexpr (!std::is_trivially_default_constructible_v<T>) {
            for (auto& elem : m_buffer) {
                new (&elem) T();
            }
        }
    }
    
    // Non-copyable, non-movable (contains atomics)
    StaticRingBuffer(const StaticRingBuffer&) = delete;
    StaticRingBuffer& operator=(const StaticRingBuffer&) = delete;
    StaticRingBuffer(StaticRingBuffer&&) = delete;
    StaticRingBuffer& operator=(StaticRingBuffer&&) = delete;
    
    // ========================================================================
    // Producer Operations (call from ONE thread only)
    // ========================================================================
    
    /**
     * @brief Try to push an element (producer only).
     * 
     * @param value Element to push
     * @return true if pushed, false if buffer is full
     * 
     * @note Wait-free, O(1)
     */
    [[nodiscard]] bool try_push(const T& value) noexcept {
        const size_type current_tail = m_tail.load(std::memory_order_relaxed);
        const size_type next_tail = increment(current_tail);
        
        // Check if buffer is full
        if (next_tail == m_head.load(std::memory_order_acquire)) {
            return false;
        }
        
        // Store the element
        m_buffer[current_tail] = value;
        
        // Publish the new tail (release ensures element is visible)
        m_tail.store(next_tail, std::memory_order_release);
        return true;
    }
    
    /**
     * @brief Try to push an element by move (producer only).
     */
    [[nodiscard]] bool try_push(T&& value) noexcept {
        const size_type current_tail = m_tail.load(std::memory_order_relaxed);
        const size_type next_tail = increment(current_tail);
        
        if (next_tail == m_head.load(std::memory_order_acquire)) {
            return false;
        }
        
        m_buffer[current_tail] = std::move(value);
        m_tail.store(next_tail, std::memory_order_release);
        return true;
    }
    
    /**
     * @brief Try to emplace an element in-place (producer only).
     */
    template<typename... Args>
    [[nodiscard]] bool try_emplace(Args&&... args) noexcept {
        const size_type current_tail = m_tail.load(std::memory_order_relaxed);
        const size_type next_tail = increment(current_tail);
        
        if (next_tail == m_head.load(std::memory_order_acquire)) {
            return false;
        }
        
        // Construct in-place
        new (&m_buffer[current_tail]) T(std::forward<Args>(args)...);
        m_tail.store(next_tail, std::memory_order_release);
        return true;
    }
    
    /**
     * @brief Check if buffer is full (producer perspective).
     */
    [[nodiscard]] bool is_full() const noexcept {
        const size_type next_tail = increment(m_tail.load(std::memory_order_relaxed));
        return next_tail == m_head.load(std::memory_order_acquire);
    }
    
    // ========================================================================
    // Consumer Operations (call from ONE thread only)
    // ========================================================================
    
    /**
     * @brief Try to pop an element (consumer only).
     * 
     * @return The popped element, or None if buffer is empty
     * 
     * @note Wait-free, O(1)
     */
    [[nodiscard]] Option<T> try_pop() noexcept {
        const size_type current_head = m_head.load(std::memory_order_relaxed);
        
        // Check if buffer is empty
        if (current_head == m_tail.load(std::memory_order_acquire)) {
            return None;
        }
        
        // Read the element
        T value = std::move(m_buffer[current_head]);
        
        // Publish the new head (release for proper ordering)
        m_head.store(increment(current_head), std::memory_order_release);
        
        return Some(std::move(value));
    }
    
    /**
     * @brief Peek at the front element without removing (consumer only).
     * 
     * @return Pointer to front element, or nullptr if empty
     */
    [[nodiscard]] const T* front() const noexcept {
        const size_type current_head = m_head.load(std::memory_order_relaxed);
        
        if (current_head == m_tail.load(std::memory_order_acquire)) {
            return nullptr;
        }
        
        return &m_buffer[current_head];
    }
    
    /**
     * @brief Check if buffer is empty (consumer perspective).
     */
    [[nodiscard]] bool is_empty() const noexcept {
        return m_head.load(std::memory_order_relaxed) == 
               m_tail.load(std::memory_order_acquire);
    }
    
    // ========================================================================
    // Shared Operations (safe from any thread)
    // ========================================================================
    
    /**
     * @brief Get approximate size (may be stale).
     * 
     * @note This is only an approximation due to concurrent access.
     */
    [[nodiscard]] size_type size_approx() const noexcept {
        const size_type head = m_head.load(std::memory_order_acquire);
        const size_type tail = m_tail.load(std::memory_order_acquire);
        
        if (tail >= head) {
            return tail - head;
        }
        return Capacity - head + tail;
    }
    
    /**
     * @brief Get maximum capacity (actual usable = Capacity - 1).
     */
    [[nodiscard]] constexpr size_type capacity() const noexcept {
        return Capacity - 1;
    }
    
    /**
     * @brief Clear all elements (NOT thread-safe).
     * 
     * @warning Only call when no other threads are accessing the buffer.
     */
    void clear_unsafe() noexcept {
        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
    }
    
private:
    [[nodiscard]] size_type increment(size_type index) const noexcept {
        return (index + 1) % Capacity;
    }
    
    // Storage aligned for cache efficiency
    alignas(64) std::array<T, Capacity> m_buffer{};
    
    // Head and tail on separate cache lines to avoid false sharing
    alignas(64) std::atomic<size_type> m_head;
    alignas(64) std::atomic<size_type> m_tail;
};

} // namespace crab
