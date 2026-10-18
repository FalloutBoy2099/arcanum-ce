#ifndef ARCANUM_GAME_BENCHMARK_DATA_H_
#define ARCANUM_GAME_BENCHMARK_DATA_H_

#include <tig/types.h>

typedef enum BenchmarkFingerprintStatus {
    BENCHMARK_FINGERPRINT_MISSING,
    BENCHMARK_FINGERPRINT_ERROR,
    BENCHMARK_FINGERPRINT_OK,
} BenchmarkFingerprintStatus;

BenchmarkFingerprintStatus benchmark_fingerprint(const char* virtual_path, Uint32* crc, Uint64* length);

#endif
