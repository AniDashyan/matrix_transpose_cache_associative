#include <vector>
#include <cmath>
#include <format>
#include <string>
#include <algorithm>
#include <utility>
#include "cache_info.h"
#include "kaizen.h"

#ifdef __linux__
    #include <sched.h>
    #include <unistd.h>
#elif _WIN32
    #include <windows.h>
#endif

void transpose_naive(std::vector<std::vector<int>>& src, std::vector<std::vector<int>>& dst, int m, int n) {
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            dst[j][i] = src[i][j];
        }
    }
}

void transpose_blocked(std::vector<std::vector<int>>& src, std::vector<std::vector<int>>& dst, int m, int n, int block_size) {
    for (int i = 0; i < m; i += block_size) {
        for (int j = 0; j < n; j += block_size) {
            int i_max = std::min(i + block_size, m);
            int j_max = std::min(j + block_size, n);
            for (int ii = i; ii < i_max; ++ii) {
                for (int jj = j; jj < j_max; ++jj) {
                    dst[jj][ii] = src[ii][jj];
                }
            }
        }
    }
}

void initialize_matrix(std::vector<std::vector<int>>& matrix, int m, int n) {
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            matrix[i][j] = i * n + j;
        }
    }
}

void pin_to_core(int core_id) {
#ifdef PIN_TO_CORE
    #ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == -1) {
            zen::log(zen::color::red("Failed to pin to core " + std::to_string(core_id) + " (Linux)"));
        } else {
            zen::log("Pinned to core ", core_id, " (Linux)");
        }
    #elif _WIN32
        DWORD_PTR mask = 1ULL << core_id;
        if (SetThreadAffinityMask(GetCurrentThread(), mask) == 0) {
            zen::log(zen::color::red("Failed to pin to core " + std::to_string(core_id) + " (Windows)"));
        } else {
            zen::log("Pinned to core ", core_id, " (Windows)");
        }
    #else
        zen::log(zen::color::red("Core pinning not supported on this platform"));
    #endif
#else
    zen::log("Core pinning disabled");
#endif
}

int calculate_block_size(const CacheInfo& cache, int m, int n) {
    long l1d_size = cache.l1d_size > 0 ? cache.l1d_size : 49152;
    long line_size = cache.line_size > 0 ? cache.line_size : 64;
    int associativity = cache.associativity > 0 ? cache.associativity : 8;

    if (cache.l1d_size <= 0 || cache.line_size <= 0 || cache.associativity <= 0) {
        zen::log(std::format("Using defaults: 48 KB, 64-byte lines, 8-way assoc"));
    }

    long bytes_per_element = sizeof(int);
    long elements_per_line = line_size / bytes_per_element; // 16

   
    int block_size = static_cast<int>(std::sqrt(l1d_size / 8));
    block_size = (block_size / elements_per_line) * elements_per_line;
    block_size = std::min(block_size, std::min(m, n));

    
    long lines_per_block = (block_size * block_size * bytes_per_element) / line_size;
    long total_lines = 2 * lines_per_block;
    long max_lines = l1d_size / line_size;
    if (total_lines > max_lines) {
        block_size = static_cast<int>(std::sqrt((l1d_size / 2) / bytes_per_element));
        block_size = (block_size / elements_per_line) * elements_per_line;
    }

    // Adjust for associativity
    long sets = max_lines / associativity;
    if (lines_per_block > associativity * sets) {
        block_size = static_cast<int>(std::sqrt(associativity * line_size / bytes_per_element));
        block_size = (block_size / elements_per_line) * elements_per_line;
    }

    block_size = std::max(8, std::min(64, block_size));
    return block_size;
}

auto measure_transpose_time(std::vector<std::vector<int>>& src, std::vector<std::vector<int>>& dst, int m, int n, int block_size) {
    zen::timer t;

    t.start();
    transpose_blocked(src, dst, m, n, block_size);
    t.stop();
    return t.duration<zen::timer::usec>().count();
}   

std::pair<int, int> parse_args(int argc, char** argv) {
    zen::cmd_args args(argv, argc);
    
    int rows = 1000, cols = 1000;
    if (!args.is_present("--row") || !args.is_present("--col")) {
        zen::log(zen::color::yellow("either --row or --col, or none of the options is not provided. Using the default value: " + std::to_string(rows) + "x" + std::to_string(cols)));
    } else {
        rows = std::stoi(args.get_options("--row")[0]);
        cols = std::stoi(args.get_options("--col")[0]);
    }
    return {rows, cols};
} 

int main(int argc, char** argv) {
    pin_to_core(0);

    auto [m, n] = parse_args(argc, argv);
    CacheInfo cache = get_cache_info();

    zen::log("\n=== Cache Information ===");
    zen::print(std::format("{:<20} {}\n", "L1D Cache Size:", std::format("{} bytes", cache.l1d_size)));
    zen::print(std::format("{:<20} {}\n", "Cache Line Size:", std::format("{} bytes", cache.line_size)));
    zen::print(std::format("{:<20} {}\n", "Associativity:", std::format("{} ways", cache.associativity)));

    std::vector<std::vector<int>> src(m, std::vector<int>(n));
    std::vector<std::vector<int>> dst(n, std::vector<int>(m));
    
    initialize_matrix(src, m, n);
    
    int predicted_block_size = calculate_block_size(cache, m, n);
    zen::print(std::format("{:<20} {}\n", "Predicted Block Size:", predicted_block_size));
    zen::log("=====================");

    std::vector<int> block_sizes = {8, 16, 32, 64, 128, 256, predicted_block_size};
    zen::log("\n=== Performance Comparison ===");
    zen::print(std::format("{:<15} {:<15}\n", "Block Size", "Time (us)"));
    zen::log("------------------------------");
    for (int bs : block_sizes) {
        std::fill(dst.begin(), dst.end(), std::vector<int>(m, 0));
        auto time = measure_transpose_time(src, dst, m, n, bs);
        zen::print(std::format("{:<15} {:<15}\n", bs, time));
    }

    zen::timer t;

    zen::log("------------------------------");
    std::fill(dst.begin(), dst.end(), std::vector<int>(m, 0));
    t.start();
    transpose_naive(src, dst, m, n);
    t.stop();
    auto naive_time = t.duration<zen::timer::usec>().count();
    zen::print(std::format("{:<15} {:<15}\n", "Naive", naive_time));
    zen::print("==============================");

    return 0;
}