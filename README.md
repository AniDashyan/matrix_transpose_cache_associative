# Matrix Transposition Performance Optimization

## Overview

This project implements matrix transposition in C++ with optimizations that exploit cache associativity for improved performance. The code demonstrates two transposition approaches: a naive row-column swap and a cache-aware blocked transposition that aims to improve cache locality and reduce cache misses. The program measures and compares the performance of different block sizes, showcasing the performance gains achieved by optimizing for hardware architecture, particularly cache behavior.

Key features:
- Matrix transposition with naive and blocked implementations.
- Cache-aware optimization using the L1D cache size, cache line size, and cache associativity.
- Measurement of transpose performance for different block sizes.
- Core pinning support for performance testing on specific CPU cores.

## Build & Run

### Requirements
- A C++ compiler (e.g., GCC or MSVC)
- CMake (version 3.0+)
- Compatible with Linux and Windows platforms
- `kaizen.h` and `cache_info.h` headers should be included

### Instructions

1. **Clone the repository**:
   ```bash
   git clone https://github.com/AniDashyan/matrix_transpose_cache_associative
   cd matrix_transpose_cache_associative
   ```

2. **Configure the project using CMake**:
   ```bash
   cmake -S . -B build
   ```

3. **Build the project**:
   ```bash
   cmake --build build --config Release
   ```

4. **Run the executable**:
   ```bash
   ./build/associative --row <rows> --col <columns>
   ```
   You can specify matrix dimensions using `--row` and `--col` flags (default is 1000x1000).

### Example Output

```
Pinned to core  0  (Windows)

=== Cache Information === 
L1D Cache Size:      49152 bytes
Cache Line Size:     64 bytes
Associativity:       12 ways
Predicted Block Size: 64
=====================

=== Performance Comparison === 
Block Size      Time (us)      
------------------------------ 
8               1286
16              1014
32              933
64              872
128             895
256             836
64              875
------------------------------
Naive           852
==============================
```

This output shows the cache information and the time taken (in microseconds) to transpose a matrix using different block sizes, followed by the time taken using a naive transposition method.
