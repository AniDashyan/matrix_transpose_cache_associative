#ifndef CACHE_INFO_H
#define CACHE_INFO_H

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#else
#error "Unsupported platform"
#endif

struct CacheInfo {
    long l1d_size;     // L1D size in bytes
    long line_size;    // Cache line size in bytes
    int associativity; // Number of ways (associativity)
};

CacheInfo get_cache_info() {
    CacheInfo info = {-1, -1, -1}; // Default to invalid values

#ifdef _WIN32
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = nullptr;
    DWORD size = 0;
    GetLogicalProcessorInformation(nullptr, &size);
    buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(size);
    if (!buffer) return info;
    if (GetLogicalProcessorInformation(buffer, &size)) {
        for (DWORD i = 0; i < size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
            if (buffer[i].Relationship == RelationCache && 
                buffer[i].Cache.Level == 1 && 
                buffer[i].Cache.Type == CacheData) {
                info.l1d_size = buffer[i].Cache.Size;
                info.line_size = buffer[i].Cache.LineSize;
                // Associativity is not directly provided; estimate it
                // Total lines = size / line_size, assume power-of-2 sets
                long total_lines = info.l1d_size / info.line_size;
                info.associativity = buffer[i].Cache.Associativity; // Often 0 (unknown) or actual value
                if (info.associativity == 0 || info.associativity == 0xFF) {
                    // Fallback: assume 8-way if unknown (common for modern CPUs)
                    info.associativity = 8;
                }
                break;
            }
        }
    }
    free(buffer);
#elif defined(__linux__)
#ifdef _SC_LEVEL1_DCACHE_SIZE
    info.l1d_size = sysconf(_SC_LEVEL1_DCACHE_SIZE);
    info.line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    long assoc = sysconf(_SC_LEVEL1_DCACHE_ASSOC);
    if (assoc > 0) {
        info.associativity = static_cast<int>(assoc);
    } else {
        // Fallback: Estimate from /proc/cpuinfo
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("cache_alignment") != std::string::npos && info.line_size <= 0) {
                info.line_size = std::stol(line.substr(line.find(":") + 1));
            }
            if (line.find("L1d cache") != std::string::npos && info.l1d_size <= 0) {
                std::string size_str = line.substr(line.find(":") + 1);
                long size = std::stol(size_str) * 1024; // Assume KB
                info.l1d_size = size;
            }
        }
        cpuinfo.close();
        if (info.l1d_size > 0 && info.line_size > 0) {
            long total_lines = info.l1d_size / info.line_size;
            // Assume associativity is a power of 2, common values: 4, 8, 12
            info.associativity = 8; // Default guess
            if (total_lines % 12 == 0) info.associativity = 12;
            else if (total_lines % 8 == 0) info.associativity = 8;
            else if (total_lines % 4 == 0) info.associativity = 4;
        }
    }
#endif
#elif defined(__APPLE__)
    size_t len = sizeof(info.l1d_size);
    if (sysctlbyname("hw.l1dcachesize", &info.l1d_size, &len, nullptr, 0) == -1) {
        info.l1d_size = -1;
    }
    len = sizeof(info.line_size);
    if (sysctlbyname("hw.cachelinesize", &info.line_size, &len, nullptr, 0) == -1) {
        info.line_size = -1;
    }
    int assoc;
    len = sizeof(assoc);
    if (sysctlbyname("hw.cacheconfig", &assoc, &len, nullptr, 0) == 0) {
        info.associativity = assoc; // hw.cacheconfig[1] for L1D, but simplified here
    } else {
        info.associativity = 8;
    }
#endif

    if (info.l1d_size <= 0) info.l1d_size = 32768; // 32 KB
    if (info.line_size <= 0) info.line_size = 64;
    if (info.associativity <= 0) info.associativity = 8;

    return info;
}
#endif