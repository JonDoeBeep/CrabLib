#pragma once

/**
 * @file prelude.h
 * @brief Single-include header for all CrabLib types.
 * 
 * Just add: #include <crab/prelude.h>
 */

// Core types
#include "crab/result.h"
#include "crab/option.h"
#include "crab/slice.h"

// Containers
#include "crab/static_vector.h"
#include "crab/ring_buffer.h"

// Synchronization
#include "crab/mutex.h"

// Utilities
#include "crab/macros.h"
#include "crab/error_types.h"

/**
 * @namespace crab
 * @brief CrabLib: Rust-inspired memory safety for C++17
 * 
 * CrabLib provides runtime memory safety guarantees for C++17 codebases
 * where exceptions are disabled (-fno-exceptions) and real-time 
 * performance is critical.
 * 
 * ## Core Types
 * 
 * - `crab::Result<T, E>`: Error handling without exceptions
 * - `crab::Option<T>`: Nullable values with monadic interface
 * - `crab::Slice<T>`: Bounds-checked non-owning view
 * - `crab::StaticVector<T, N>`: Fixed-capacity vector (no heap)
 * - `crab::Mutex<T>`: Data-owning mutex (Rust pattern)
 * 
 * ## Quick Start
 * 
 * @code{cpp}
 * #include <crab/prelude.h>
 * 
 * crab::Result<int, crab::OutOfBounds> safe_access(crab::Slice<int> data, size_t i) {
 *     auto elem = data.get(i);
 *     if (elem.is_err()) {
 *         return crab::Err(elem.unwrap_err());
 *     }
 *     return crab::Ok(elem.unwrap().get());
 * }
 * 
 * // Or with CRAB_TRY macro:
 * crab::Result<int, crab::OutOfBounds> safe_access_v2(crab::Slice<int> data, size_t i) {
 *     auto elem = CRAB_TRY(data.get(i));
 *     return crab::Ok(elem.get());
 * }
 * @endcode
 */
