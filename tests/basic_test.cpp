/**
 * @file basic_test.cpp
 * @brief Basic compilation test for CrabLib types.
 * 
 * This file ensures all headers compile and basic usage patterns work.
 * Run with: g++ -std=c++17 -I../src -c basic_test.cpp
 */

#include <crab/prelude.h>
#include <vector>
#include <cassert>

// ============================================================================
// Result Tests
// ============================================================================

crab::Result<int, crab::OutOfBounds> test_result(int i) {
    if (i < 0) {
        return crab::Err(crab::OutOfBounds{static_cast<size_t>(-i), 0});
    }
    return crab::Ok(i * 2);
}

void result_tests() {
    // Ok case
    auto r1 = test_result(5);
    assert(r1.is_ok());
    assert(r1.unwrap() == 10);
    
    // Err case
    auto r2 = test_result(-3);
    assert(r2.is_err());
    assert(r2.unwrap_err().index == 3);
    
    // map
    auto r3 = test_result(3).map([](int v) { return v + 1; });
    assert(r3.is_ok());
    assert(r3.unwrap() == 7);
    
    // unwrap_or
    auto r4 = test_result(-1).unwrap_or(42);
    assert(r4 == 42);
}

// ============================================================================
// Slice Tests
// ============================================================================

void slice_tests() {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    
    // From lvalue container
    crab::Slice<int> slice(vec);
    assert(slice.size() == 5);
    
    // Safe access
    auto elem = slice.get(2);
    assert(elem.is_ok());
    assert(elem.unwrap().get() == 3);
    
    // Out of bounds
    auto bad = slice.get(10);
    assert(bad.is_err());
    
    // Subslicing
    auto sub = slice.subslice(1, 4);
    assert(sub.is_ok());
    assert(sub.unwrap().size() == 3);
    
    // Copy
    std::vector<int> dest(5, 0);
    crab::Slice<int> dest_slice(dest);
    auto copy_result = slice.copy_to(dest_slice);
    assert(copy_result.is_ok());
    assert(dest[2] == 3);
}

// ============================================================================
// Option Tests
// ============================================================================

void option_tests() {
    crab::Option<int> some = crab::Some(42);
    assert(some.is_some());
    assert(some.unwrap() == 42);
    
    crab::Option<int> none = crab::None;
    assert(none.is_none());
    
    // map
    auto mapped = crab::Some(10).map([](int v) { return v * 2; });
    assert(mapped.is_some());
    assert(mapped.unwrap() == 20);
    
    // unwrap_or
    crab::Option<int> empty;
    auto val = std::move(empty).unwrap_or(99);
    assert(val == 99);
}

// ============================================================================
// StaticVector Tests
// ============================================================================

void static_vector_tests() {
    crab::StaticVector<int, 8> vec;
    
    // try_push_back
    auto r1 = vec.try_push_back(10);
    assert(r1.is_ok());
    assert(vec.size() == 1);
    
    // Safe access
    auto elem = vec.get(0);
    assert(elem.is_ok());
    assert(elem.unwrap().get() == 10);
    
    // pop_back returns Option
    auto popped = vec.pop_back();
    assert(popped.is_some());
    assert(popped.unwrap() == 10);
    assert(vec.empty());
    
    // Fill to capacity
    for (int i = 0; i < 8; ++i) {
        vec.try_push_back(i);
    }
    assert(vec.is_full());
    
    // Try push when full
    auto r2 = vec.try_push_back(99);
    assert(r2.is_err());
}

// ============================================================================
// Mutex Tests
// ============================================================================

void mutex_tests() {
    crab::Mutex<std::vector<int>> data(std::vector<int>{1, 2, 3});
    
    // Lock and access
    {
        auto guard = data.lock();
        guard->push_back(4);
        assert(guard->size() == 4);
    }
    
    // try_lock
    auto maybe_guard = data.try_lock();
    assert(maybe_guard.is_some());
}

// ============================================================================
// RingBuffer Tests
// ============================================================================

void ring_buffer_tests() {
    crab::StaticRingBuffer<int, 4> buffer;
    
    // Push
    assert(buffer.try_push(1));
    assert(buffer.try_push(2));
    assert(buffer.try_push(3));
    assert(!buffer.try_push(4)); // Full (capacity is N-1)
    
    // Pop
    auto v1 = buffer.try_pop();
    assert(v1.is_some());
    assert(v1.unwrap() == 1);
    
    // Now can push again
    assert(buffer.try_push(4));
    
    // Empty out
    buffer.try_pop();
    buffer.try_pop();
    buffer.try_pop();
    
    auto empty = buffer.try_pop();
    assert(empty.is_none());
}

// ============================================================================
// Main
// ============================================================================

int main() {
    result_tests();
    slice_tests();
    option_tests();
    static_vector_tests();
    mutex_tests();
    ring_buffer_tests();
    
    return 0;
}
