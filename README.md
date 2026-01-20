# CrabLib

Memory safety primitives for C++17 embedded systems.

CrabLib provides runtime safety guarantees for codebases where exceptions are disabled (`-fno-exceptions`) and real-time performance is required. Header-only.

## Usage

```cpp
#include <crab/prelude.h>

crab::Result<int, crab::OutOfBounds> safe_get(crab::Slice<int> data, size_t i) {
    auto elem = CRAB_TRY(data.get(i));
    return crab::Ok(elem.get() * 2);
}
```

## Installation

Add as CMake subdirectory:

```cmake
add_subdirectory(CrabLib)
target_link_libraries(your_target PRIVATE crab::crab)
```

Or copy `src/crab/` to your include path.

## Requirements

- C++17
- Tested with GCC 13.3 and Clang

## Build Modes

| Mode | Assertions | Performance |
|------|------------|-------------|
| Debug | Full bounds checking | Normal |
| Release | Critical checks only | Optimized |
| Release + `CRAB_UNSAFE_FAST` | None | Maximum |

## License

MIT
