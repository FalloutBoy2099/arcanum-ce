#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "tig/async_asset_reader.h"

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

static bool wait_result(AsyncAssetReader* reader, Uint64 generation, Uint64 tag, AsyncAssetResult* result)
{
    Uint64 deadline = SDL_GetTicks() + 3000;
    while (SDL_GetTicks() < deadline) {
        if (async_asset_reader_take(reader, generation, tag, result)) return true;
        SDL_Delay(1);
    }
    return false;
}

int main(void)
{
    CHECK(SDL_Init(0));
    char* current = SDL_GetCurrentDirectory();
    CHECK(current != NULL);
    if (current == NULL) return 1;
    char path[512], missing[512], empty[512];
    SDL_snprintf(path, sizeof(path), "%sasync-asset-fixture.bin", current);
    SDL_snprintf(missing, sizeof(missing), "%sasync-asset-missing.bin", current);
    SDL_snprintf(empty, sizeof(empty), "%sasync-asset-empty.bin", current);
    SDL_free(current);
    FILE* file = fopen(path, "wb");
    CHECK(file != NULL);
    if (file == NULL) return 1;
    for (int i = 0; i < 64; i++) fputc(i, file);
    fclose(file);
    file = fopen(empty, "wb");
    CHECK(file != NULL);
    if (file != NULL) fclose(file);
    CHECK(async_asset_reader_create(0, 64, 128) == NULL);
    CHECK(async_asset_reader_create(2, 129, 128) == NULL);
    AsyncAssetReader* reader = async_asset_reader_create(2, 64, 128);
    CHECK(reader != NULL);
    if (reader == NULL) return 1;
    CHECK(!async_asset_reader_request(reader, "relative.bin", 0, 1, 0));
    CHECK(!async_asset_reader_request(reader, path, 1, 1, 0));
    CHECK(async_asset_reader_request(reader, path, 0, 1, 0));
    CHECK(async_asset_reader_request(reader, path, 0, 1, 10));
    CHECK(!async_asset_reader_request(reader, empty, 0, 1, 0));
    CHECK(async_asset_reader_request(reader, path, 0, 2, 0));
    CHECK(!async_asset_reader_request(reader, path, 0, 3, 0));
    AsyncAssetResult result = { 0 };
    CHECK(wait_result(reader, 0, 1, &result));
    CHECK(result.success && result.tag == 1 && result.size == 64 && !result.from_archive);
    if (result.success) for (int i = 0; i < 64; i++) CHECK(((unsigned char*)result.bytes)[i] == i);
    async_asset_result_release(&result);
    CHECK(wait_result(reader, 0, 2, &result));
    CHECK(result.success && result.tag == 2);
    async_asset_result_release(&result);
    CHECK(async_asset_reader_request(reader, missing, 0, 4, 0));
    CHECK(wait_result(reader, 0, 4, &result));
    CHECK(!result.success && result.bytes == NULL && result.tag == 4);
    async_asset_result_release(&result);
    CHECK(async_asset_reader_request(reader, empty, 0, 5, 0));
    CHECK(wait_result(reader, 0, 5, &result));
    CHECK(result.success && result.size == 0);
    async_asset_result_release(&result);
    TigAssetSource source = { .archive = true, .offset = 8, .size = 16, .stored_size = 16 };
    SDL_strlcpy(source.path, path, sizeof(source.path));
    CHECK(async_asset_reader_request_sources(reader, &source, 1, 0, 20, 0));
    CHECK(wait_result(reader, 0, 20, &result));
    CHECK(result.success && result.size == 16 && result.from_archive);
    if (result.success) for (int i = 0; i < 16; i++) CHECK(((unsigned char*)result.bytes)[i] == i + 8);
    async_asset_result_release(&result);
    const unsigned char compressed[] = { 0x78, 0x9c, 0x33, 0x36, 0xa6, 0x0c, 0x00, 0x00, 0x9e, 0xaf, 0x0c, 0xc1 };
    file = fopen(empty, "wb");
    CHECK(file != NULL);
    if (file != NULL) { fwrite(compressed, 1, sizeof(compressed), file); fclose(file); }
    SDL_strlcpy(source.path, empty, sizeof(source.path));
    source.offset = 0;
    source.size = 64;
    source.stored_size = sizeof(compressed);
    source.compressed = true;
    CHECK(async_asset_reader_request_sources(reader, &source, 1, 0, 21, 0));
    CHECK(wait_result(reader, 0, 21, &result));
    CHECK(result.success && result.size == 64);
    if (result.success) for (int i = 0; i < 64; i++) CHECK(((unsigned char*)result.bytes)[i] == 51);
    async_asset_result_release(&result);
    source.stored_size--;
    CHECK(async_asset_reader_request_sources(reader, &source, 1, 0, 22, 0));
    CHECK(wait_result(reader, 0, 22, &result));
    CHECK(!result.success && result.bytes == NULL);
    async_asset_result_release(&result);
    source.stored_size++;
    source.size = 63;
    CHECK(async_asset_reader_request_sources(reader, &source, 1, 0, 23, 0));
    CHECK(wait_result(reader, 0, 23, &result));
    CHECK(!result.success && result.bytes == NULL);
    async_asset_result_release(&result);
    unsigned char original[20000], packed[21000];
    uint32_t random_state = 1;
    for (size_t i = 0; i < sizeof(original); i++) {
        random_state ^= random_state << 13;
        random_state ^= random_state >> 17;
        random_state ^= random_state << 5;
        original[i] = (unsigned char)random_state;
    }
    uLongf packed_size = sizeof(packed);
    CHECK(compress(packed, &packed_size, original, sizeof(original)) == Z_OK);
    CHECK(packed_size > 4096);
    file = fopen(empty, "wb");
    CHECK(file != NULL);
    if (file != NULL) { fwrite(packed, 1, packed_size, file); fclose(file); }
    source.size = sizeof(original);
    source.stored_size = (uint32_t)packed_size;
    AsyncAssetReader* large_reader = async_asset_reader_create(1, 32768, 32768);
    CHECK(large_reader != NULL);
    CHECK(async_asset_reader_request_sources(large_reader, &source, 1, 0, 1, 0));
    CHECK(wait_result(large_reader, 0, 1, &result));
    CHECK(result.success && result.size == sizeof(original));
    if (result.success) CHECK(memcmp(result.bytes, original, sizeof(original)) == 0);
    async_asset_result_release(&result);
    async_asset_reader_destroy(large_reader);
    file = fopen(empty, "wb");
    if (file != NULL) fclose(file);
    async_asset_reader_destroy(reader);
    reader = async_asset_reader_create(4, 32, 32);
    CHECK(reader != NULL);
    if (reader != NULL) {
        CHECK(async_asset_reader_request(reader, path, 0, 1, 0));
        CHECK(wait_result(reader, 0, 1, &result));
        CHECK(!result.success && result.bytes == NULL);
        async_asset_result_release(&result);
        for (Uint64 generation = 1; generation < 100; generation++) {
            async_asset_reader_reset(reader, generation);
            async_asset_reader_request(reader, path, generation, generation, 0);
            async_asset_reader_reset(reader, generation + 1);
            CHECK(!async_asset_reader_take(reader, generation, generation, &result));
        }
        async_asset_reader_reset(reader, 1000);
        Uint64 deadline = SDL_GetTicks() + 3000;
        bool accepted = false;
        while (!accepted && SDL_GetTicks() < deadline) {
            accepted = async_asset_reader_request(reader, empty, 1000, 8, 0);
            if (!accepted) SDL_Delay(1);
        }
        CHECK(accepted);
        CHECK(wait_result(reader, 1000, 8, &result));
        CHECK(result.success && result.tag == 8 && result.size == 0);
        async_asset_result_release(&result);
        async_asset_reader_destroy(reader);
    }
    remove(path);
    remove(empty);
    SDL_Quit();
    printf("Asynchronous asset staging: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
