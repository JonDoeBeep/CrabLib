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
};

/**
 * @brief Capacity exceeded error.
 */
struct CapacityExceeded {
    std::size_t requested;  ///< Requested size
    std::size_t capacity;   ///< Maximum capacity
};

/**
 * @brief Null pointer access error.
 */
struct NullPointer {};

/**
 * @brief Parse/decode error with position information.
 */
struct ParseError {
    std::size_t offset;   ///< Byte offset where error occurred
    uint8_t expected;     ///< Expected value (if applicable)
    uint8_t found;        ///< Actual value found
};

/**
 * @brief Generic unit type (for Result<void, E> specialization).
 */
struct Unit {};

} // namespace crab
