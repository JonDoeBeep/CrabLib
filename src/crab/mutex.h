#pragma once

/**
 * @file mutex.h
 * @brief Rust-style data-owning Mutex<T>.
 * 
 * Unlike std::mutex which requires separate data management, crab::Mutex<T>
 * owns the protected data and only allows access through a lock guard.
 * This prevents the common "forgot to lock" anti-pattern.
 * 
 * ## Embedded/RTOS Usage
 * 
 * For bare-metal or RTOS, provide a custom lock type:
 * @code
 * struct FreeRtosLock {
 *     SemaphoreHandle_t sem;
 *     FreeRtosLock() { sem = xSemaphoreCreateMutex(); }
 *     void lock() { xSemaphoreTake(sem, portMAX_DELAY); }
 *     void unlock() { xSemaphoreGive(sem); }
 *     bool try_lock() { return xSemaphoreTake(sem, 0) == pdTRUE; }
 * };
 * 
 * crab::Mutex<Data, FreeRtosLock> my_data;
 * @endcode
 */

#include "crab/option.h"
#include "crab/macros.h"

#include <chrono>
#include <utility>

// Only include std::mutex if not using custom lock
#ifndef CRAB_NO_STD_MUTEX
#include <mutex>
#endif

namespace crab {

/**
 * @brief Default lock type using std::mutex.
 * 
 * Not available if CRAB_NO_STD_MUTEX is defined (for bare-metal).
 */
#ifndef CRAB_NO_STD_MUTEX
struct StdMutexLock {
    std::mutex m_mutex;
    
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    bool try_lock() { return m_mutex.try_lock(); }
    
    template<typename Rep, typename Period>
    bool try_lock_for(std::chrono::duration<Rep, Period>) {
        // std::mutex doesn't support timed locking
        return try_lock();
    }
};
#endif

/**
 * @brief Data-owning mutex (Rust-style).
 * 
 * The mutex owns the data it protects. The only way to access the data
 * is through the Guard returned by lock() or try_lock().
 * 
 * @tparam T Protected data type
 * @tparam LockType Lock implementation (default: StdMutexLock)
 */
template<typename T, 
#ifdef CRAB_NO_STD_MUTEX
         typename LockType  // User must provide lock type
#else
         typename LockType = StdMutexLock
#endif
>
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
            : m_mutex(other.m_mutex), m_data(other.m_data), m_owns_lock(other.m_owns_lock) {
            other.m_owns_lock = false;
            other.m_data = nullptr;
        }
        
        Guard& operator=(Guard&& other) noexcept {
            if (this != &other) {
                if (m_owns_lock) {
                    m_mutex->unlock();
                }
                m_mutex = other.m_mutex;
                m_data = other.m_data;
                m_owns_lock = other.m_owns_lock;
                other.m_owns_lock = false;
                other.m_data = nullptr;
            }
            return *this;
        }
        
        ~Guard() {
            if (m_owns_lock) {
                m_mutex->unlock();
            }
        }
        
        /**
         * @brief Access the protected data.
         */
        [[nodiscard]] T& operator*() noexcept { 
            CRAB_DEBUG_ASSERT(m_data != nullptr, "Dereferencing moved-from Guard");
            return *m_data; 
        }
        [[nodiscard]] const T& operator*() const noexcept { 
            CRAB_DEBUG_ASSERT(m_data != nullptr, "Dereferencing moved-from Guard");
            return *m_data; 
        }
        
        [[nodiscard]] T* operator->() noexcept { 
            CRAB_DEBUG_ASSERT(m_data != nullptr, "Dereferencing moved-from Guard");
            return m_data; 
        }
        [[nodiscard]] const T* operator->() const noexcept { 
            CRAB_DEBUG_ASSERT(m_data != nullptr, "Dereferencing moved-from Guard");
            return m_data; 
        }
        
        /**
         * @brief Get raw pointer to data.
         */
        [[nodiscard]] T* get() noexcept { return m_data; }
        [[nodiscard]] const T* get() const noexcept { return m_data; }
        
    private:
        friend class Mutex;
        
        Guard(LockType& mutex, T& data, bool owns) noexcept
            : m_mutex(&mutex), m_data(&data), m_owns_lock(owns) {}
        
        LockType* m_mutex;
        T* m_data;
        bool m_owns_lock;
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
        m_mutex.lock();
        return Guard(m_mutex, m_data, true);
    }
    
    /**
     * @brief Try to acquire lock without blocking.
     * @return Some(Guard) if lock acquired, None if already locked
     */
    [[nodiscard]] Option<Guard> try_lock() {
        if (m_mutex.try_lock()) {
            return Some(Guard(m_mutex, m_data, true));
        }
        return None;
    }
    
    /**
     * @brief Try to acquire lock with timeout.
     * @param timeout Maximum time to wait for lock
     * @return Some(Guard) if lock acquired, None if timeout expired
     * @note Requires LockType to implement try_lock_for()
     */
    template<typename Rep, typename Period>
    [[nodiscard]] Option<Guard> try_lock_for(std::chrono::duration<Rep, Period> timeout) {
        if (m_mutex.try_lock_for(timeout)) {
            return Some(Guard(m_mutex, m_data, true));
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
    mutable LockType m_mutex;
    T m_data;
};

/**
 * @brief RAII lock guard with scope-based automatic unlock.
 */
template<typename T, typename LockType = 
#ifdef CRAB_NO_STD_MUTEX
    void  // User must specify
#else
    StdMutexLock
#endif
>
using MutexGuard = typename Mutex<T, LockType>::Guard;

} // namespace crab
