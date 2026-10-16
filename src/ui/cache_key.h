#ifndef ARCANUM_UI_CACHE_KEY_H_
#define ARCANUM_UI_CACHE_KEY_H_

#include <stdint.h>
#include <string.h>

// In-process fingerprints only; never persisted in saves or used for security.
#define UI_CACHE_KEY_INIT UINT64_C(14695981039346656037)

static inline void ui_cache_key_bytes(uint64_t* key, const void* data, size_t length)
{
    const unsigned char* bytes = data;
    for (size_t i = 0; i < length; i++) {
        *key ^= bytes[i];
        *key *= UINT64_C(1099511628211);
    }
}

static inline void ui_cache_key_string(uint64_t* key, const char* text)
{
    // Include the terminator so adjacent strings remain distinct fields.
    ui_cache_key_bytes(key, text, strlen(text) + 1);
}

#endif
