#pragma once

/**
 * @file error_types.h
 * @brief Common error types for CrabLib Result<T, E>.
 * 
 * These lightweight error structs carry just enough information for debugging
 * without heap allocation.
 */

#include <cstddef>
#include <cstdint>

namespace crab {

/**
 * @brief Out of bounds access error.
 */
struct OutOfBounds {
    std::size_t index;    ///< Requested index
    std::size_t size;     ///< Container size
    
    constexpr bool operator==(const OutOfBounds& other) const noexcept {
        return index == other.index && size == other.size;
    }
    constexpr bool operator!=(const OutOfBounds& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief Capacity exceeded error.
 */
struct CapacityExceeded {
    std::size_t requested;  ///< Requested size
    std::size_t capacity;   ///< Maximum capacity
    
    constexpr bool operator==(const CapacityExceeded& other) const noexcept {
        return requested == other.requested && capacity == other.capacity;
    }
    constexpr bool operator!=(const CapacityExceeded& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief Null pointer access error.
 */
struct NullPointer {
    constexpr bool operator==(const NullPointer&) const noexcept { return true; }
    constexpr bool operator!=(const NullPointer&) const noexcept { return false; }
};

/**
 * @brief Parse/decode error with position information.
 */
struct ParseError {
    std::size_t offset;   ///< Byte offset where error occurred
    uint8_t expected;     ///< Expected value (if applicable)
    uint8_t found;        ///< Actual value found
    
    constexpr bool operator==(const ParseError& other) const noexcept {
        return offset == other.offset && expected == other.expected && found == other.found;
    }
    constexpr bool operator!=(const ParseError& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief Generic unit type (for Result<void, E> specialization).
 */
struct Unit {
    constexpr bool operator==(const Unit&) const noexcept { return true; }
    constexpr bool operator!=(const Unit&) const noexcept { return false; }
};

} // namespace crab
