#pragma once

/**
 * @file mutex.h
 * @brief Rust-style data-owning Mutex<T>.
 * 
 * Unlike std::mutex which requires separate data management, crab::Mutex<T>
 * owns the protected data and only allows access through a lock guard.
 * This prevents the common "forgot to lock" anti-pattern.
 */

#include "crab/option.h"

#include <mutex>
#include <utility>

namespace crab {

/**
 * @brief Data-owning mutex (Rust-style).
 * 
 * The mutex owns the data it protects. The only way to access the data
 * is through the Guard returned by lock() or try_lock().
 * 
 * @tparam T Protected data type
 * 
 * @code{cpp}
 *   crab::Mutex<std::vector<int>> data(std::vector<int>{1, 2, 3});
 *   
 *   // Data can only be accessed while holding the lock
 *   {
 *       auto guard = data.lock();
 *       guard->push_back(4);
 *       (*guard)[0] = 10;
 *   }  // Lock released here
 *   
 *   // try_lock for non-blocking
 *   if (auto guard = data.try_lock(); guard.is_some()) {
 *       guard.unwrap()->clear();
 *   }
 * @endcode
 */
template<typename T>
class Mutex {
public:
    /**
     * @brief Guard providing exclusive access to the data.
     * 
     * Automatically releases the lock when destroyed.
     */
    class Guard {
    public:
        // Non-copyable
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        
        // Movable
        Guard(Guard&& other) noexcept 
            : m_lock(std::move(other.m_lock)), m_data(other.m_data) {
            other.m_data = nullptr;
        }
        
        Guard& operator=(Guard&& other) noexcept {
            if (this != &other) {
                m_lock = std::move(other.m_lock);
                m_data = other.m_data;
                other.m_data = nullptr;
            }
            return *this;
        }
        
        /**
         * @brief Access the protected data.
         */
        [[nodiscard]] T& operator*() noexcept { return *m_data; }
        [[nodiscard]] const T& operator*() const noexcept { return *m_data; }
        
        [[nodiscard]] T* operator->() noexcept { return m_data; }
        [[nodiscard]] const T* operator->() const noexcept { return m_data; }
        
        /**
         * @brief Get raw pointer to data.
         */
        [[nodiscard]] T* get() noexcept { return m_data; }
        [[nodiscard]] const T* get() const noexcept { return m_data; }
        
        ~Guard() = default;
        
    private:
        friend class Mutex;
        
        Guard(std::unique_lock<std::mutex> lock, T& data) noexcept
            : m_lock(std::move(lock)), m_data(&data) {}
        
        std::unique_lock<std::mutex> m_lock;
        T* m_data;
    };
    
    // ========================================================================
    // Constructors
    // ========================================================================
    
    /**
     * @brief Construct mutex with default-constructed data.
     */
    Mutex() : m_data() {}
    
    /**
     * @brief Construct mutex with given data.
     */
    template<typename U = T,
             typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    explicit Mutex(U&& value) : m_data(std::forward<U>(value)) {}
    
    // Non-copyable, non-movable (data inside is protected)
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    Mutex(Mutex&&) = delete;
    Mutex& operator=(Mutex&&) = delete;
    
    // ========================================================================
    // Locking
    // ========================================================================
    
    /**
     * @brief Acquire exclusive lock, blocking until available.
     * @return Guard providing access to the data
     */
    [[nodiscard]] Guard lock() {
        return Guard(std::unique_lock<std::mutex>(m_mutex), m_data);
    }
    
    /**
     * @brief Try to acquire lock without blocking.
     * @return Some(Guard) if lock acquired, None if already locked
     */
    [[nodiscard]] Option<Guard> try_lock() {
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (lock.owns_lock()) {
            return Some(Guard(std::move(lock), m_data));
        }
        return None;
    }
    
    /**
     * @brief Get a mutable reference without locking (UNSAFE).
     * 
     * Only call this when you have exclusive access through other means
     * (e.g., during single-threaded initialization).
     */
    [[nodiscard]] T& get_mut_unsafe() noexcept { return m_data; }
    
    /**
     * @brief Get a const reference without locking (UNSAFE).
     * 
     * Only safe if no other thread can modify the data.
     */
    [[nodiscard]] const T& get_unsafe() const noexcept { return m_data; }
    
private:
    mutable std::mutex m_mutex;
    T m_data;
};

/**
 * @brief RAII lock guard with scope-based automatic unlock.
 * 
 * For cases where you need the guard to outlive its creation scope
 * but still want deterministic unlock behavior.
 */
template<typename T>
using MutexGuard = typename Mutex<T>::Guard;

} // namespace crab
