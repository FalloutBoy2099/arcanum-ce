#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tig/tig.h>
#include <tig/art_decode.h>
#include <tig/art_frame.h>

static int failures;
static unsigned int rng = 0xAC2026;
static unsigned int random_value(void) { rng = rng * 1664525u + 1013904223u; return rng; }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

// Independent reference follows the old stream decoder's packet semantics.
static size_t reference(const uint8_t* source, size_t size, uint8_t* output)
{
    size_t input = 0, count = 0;
    while (input < size) {
        unsigned int control = source[input++];
        unsigned int length = control & 127;
        if (control & 128) for (unsigned int i = 0; i < length; i++) output[count++] = source[input++];
        else { uint8_t color = source[input++]; for (unsigned int i = 0; i < length; i++) output[count++] = color; }
    }
    return count;
}

static void packets(void)
{
    uint8_t source[32768], expected[32768], decoded[32770];
    for (int trial = 0; trial < 1000; trial++) {
        size_t input = 0;
        int commands = 1 + random_value() % 150;
        for (int packet = 0; packet < commands; packet++) {
            int size = random_value() % 128;
            bool literal = (random_value() & 4) != 0;
            source[input++] = size | (literal ? 128 : 0);
            if (literal) for (int i = 0; i < size; i++) source[input++] = random_value() >> 24;
            else source[input++] = random_value() >> 24;
        }
        size_t output = reference(source, input, expected);
        memset(decoded, 0xA5, sizeof(decoded));
        CHECK(tig_art_decode_rle(source, input, decoded + 1, output));
        CHECK(memcmp(decoded + 1, expected, output) == 0);
        CHECK(decoded[0] == 0xA5 && decoded[output + 1] == 0xA5);
        if (output != 0) {
            memset(decoded, 0xA5, sizeof(decoded));
            CHECK(!tig_art_decode_rle(source, input, decoded + 1, output - 1));
            CHECK(decoded[0] == 0xA5 && decoded[output] == 0xA5);
        }
    }
    const uint8_t truncated_literal[] = {0x83, 1, 2};
    const uint8_t truncated_run[] = {3};
    const uint8_t overflow[] = {127, 4};
    const uint8_t short_output[] = {1, 4};
    CHECK(!tig_art_decode_rle(truncated_literal, 3, decoded, 3));
    CHECK(!tig_art_decode_rle(truncated_run, 1, decoded, 3));
    CHECK(!tig_art_decode_rle(overflow, 2, decoded, 2));
    CHECK(!tig_art_decode_rle(short_output, 2, decoded, 2));
    CHECK(tig_art_decode_rle(NULL, 0, NULL, 0));
    CHECK(!tig_art_decode_rle(NULL, 1, decoded, 1));
    CHECK(!tig_art_decode_rle(short_output, 2, NULL, 1));
}

static int old_stream(TigFile* stream, size_t size, uint8_t* output)
{
    size_t input = 0;
    int calls = 0;
    while (input < size) {
        uint8_t control, color;
        CHECK(tig_file_fread(&control, 1, 1, stream) == 1); calls++;
        int length = control & 127;
        if (control & 128) {
            CHECK(tig_file_fread(output, 1, length, stream) == (size_t)length); calls++;
            input += 1 + length;
        } else {
            CHECK(tig_file_fread(&color, 1, 1, stream) == 1); calls++;
            memset(output, color, length); input += 2;
        }
        output += length;
    }
    return calls;
}

static const char* artwork;
static int art_path(tig_art_id_t aid, char* path, size_t size)
{
    (void)aid; SDL_strlcpy(path, artwork, size); return TIG_OK;
}
static tig_art_id_t reset_art(tig_art_id_t aid) { return tig_art_id_frame_set(aid, 0); }

static void artwork_pixels(const char* path, unsigned int id)
{
    FILE* stream = fopen(path, "rb");
    CHECK(stream != NULL); if (stream == NULL) return;
    uint32_t header[33], palette[256] = {0};
    CHECK(fread(header, sizeof(header), 1, stream) == 1);
    CHECK(header[2] == 8);
    int rotations = (header[0] & 1) != 0 ? 1 : 8;
    int frames = header[8];
    CHECK(frames > 0 && frames <= 256);
    if (frames <= 0 || frames > 256) { fclose(stream); return; }
    for (int i = 0; i < 4; i++) if (header[3 + i]) {
        uint32_t next[256]; CHECK(fread(next, sizeof(next), 1, stream) == 1);
        if (i == 0) memcpy(palette, next, sizeof(palette));
    }
    int32_t (*data)[7] = malloc(frames * rotations * sizeof(*data));
    CHECK(fread(data, sizeof(*data), frames * rotations, stream) == (size_t)(frames * rotations));
    artwork = path;
    TigFile* probe = tig_file_fopen(path, "rb");
    CHECK(probe != NULL);
    Uint64 old_ticks = 0, new_ticks = 0;
    unsigned int old_calls = 0, new_calls = 0;
    for (int slot = 0; slot < frames * rotations; slot++) {
        int frame = slot % frames, rotation = slot / frames;
        size_t count = (size_t)data[slot][0] * data[slot][1];
        size_t size = data[slot][2];
        uint8_t* source = malloc(size ? size : 1);
        uint8_t* expected = calloc(count ? count : 1, 1);
        long position = ftell(stream);
        CHECK(fread(source, 1, size, stream) == size);
        if (size == count) memcpy(expected, source, count);
        else if (size != 0) CHECK(reference(source, size, expected) == count);
        if (probe != NULL && size != 0 && size != count) {
            uint8_t* result = malloc(count ? count : 1);
            // Alternate the order to avoid favoring the warm-file-cache reader.
            for (int trial = 0; trial < 10; trial++) for (int pass = 0; pass < 2; pass++) {
                bool buffered = ((trial + pass) & 1) != 0;
                CHECK(tig_file_fseek(probe, (int)position, SEEK_SET) == 0);
                Uint64 start = SDL_GetPerformanceCounter();
                if (buffered) {
                    CHECK(tig_file_fread(source, 1, size, probe) == size);
                    CHECK(tig_art_decode_rle(source, size, result, count));
                    new_ticks += SDL_GetPerformanceCounter() - start;
                    new_calls++;
                } else {
                    int calls = old_stream(probe, size, result);
                    old_ticks += SDL_GetPerformanceCounter() - start;
                    old_calls += calls;
                }
                CHECK(memcmp(result, expected, count) == 0);
            }
            free(result);
        }
        tig_art_id_t aid;
        if (rotations == 1) CHECK(tig_art_interface_id_create(id, frame, 0, 0, &aid) == TIG_OK);
        else CHECK(tig_art_scenery_id_create(id % 64, 0, frame, rotation, 0, &aid) == TIG_OK);
        TigArtFrameData actual;
        CHECK(tig_art_frame_data(aid, &actual) == TIG_OK);
        CHECK(actual.width == data[slot][0] && actual.height == data[slot][1]);
        for (int y = 0; y < actual.height; y++) for (int x = 0; x < actual.width; x++) {
            unsigned int color;
            CHECK(tig_art_pixel_color_get(aid, x, y, &color) == TIG_OK);
            CHECK(color == tig_color_index_of(palette[expected[y * actual.width + x]]));
        }
        free(expected); free(source);
    }
    if (probe != NULL) tig_file_fclose(probe);
    printf("Read+decode probe (10 repetitions/frame): %s old=%.4fms new=%.4fms reads=%u/%u\n",
        path, old_ticks * 1000.0 / SDL_GetPerformanceFrequency(), new_ticks * 1000.0 / SDL_GetPerformanceFrequency(), old_calls, new_calls);
    free(data); fclose(stream);
}

static void synthetic_art(void)
{
    uint32_t header[33] = {0}; header[0] = 1; header[1] = 10; header[2] = 8; header[3] = 1; header[8] = 4;
    uint32_t palette[256];
    for (int i = 0; i < 256; i++) palette[i] = i * 0x010101;
    int32_t frames[4][7] = {{4,3,4,0,0,0,0}, {4,3,5,0,0,0,0}, {4,3,12,0,0,0,0}, {0,0,0,0,0,0,0}};
    const uint8_t pixels[] = {6,2,6,3, 0x82,4,5,10,6, 2,3,4,5,6,7,8,9,10,11,12,13};
    FILE* stream = fopen("art-decode-fixture.art", "wb");
    CHECK(stream != NULL); if (stream == NULL) return;
    CHECK(fwrite(header, sizeof(header), 1, stream) == 1);
    CHECK(fwrite(palette, sizeof(palette), 1, stream) == 1);
    CHECK(fwrite(frames, sizeof(frames), 1, stream) == 1);
    CHECK(fwrite(pixels, sizeof(pixels), 1, stream) == 1);
    fclose(stream);
    artwork_pixels("art-decode-fixture.art", 950);
    remove("art-decode-fixture.art");
}

static void serialized_headers(void)
{
    const char* path = "art-header-fixture.art";
    for (unsigned int mask = 1; mask < 16; mask += 2) {
        uint32_t header[33] = { 0 };
        header[0] = 1; header[1] = 10; header[2] = 8; header[8] = 1;
        for (int index = 0; index < 4; index++) header[3 + index] = (mask & (1u << index)) != 0 ? UINT32_MAX - index : 0;
        for (int index = 0; index < 8; index++) {
            header[9 + index] = UINT32_MAX - index;
            header[25 + index] = UINT32_C(0x80000000) + index;
        }
        uint32_t palette[256];
        for (int index = 0; index < 256; index++) palette[index] = index * 0x010101u;
        int32_t frame[7] = { 2, 2, 4, 0, 0, 0, 0 };
        uint8_t pixels[4] = { 2, 3, 4, 5 };
        tig_art_flush();
        FILE* stream = fopen(path, "wb");
        CHECK(stream != NULL); if (stream == NULL) return;
        CHECK(fwrite(header, sizeof(header), 1, stream) == 1);
        for (int index = 0; index < 4; index++) if (header[3 + index] != 0) CHECK(fwrite(palette, sizeof(palette), 1, stream) == 1);
        CHECK(fwrite(frame, sizeof(frame), 1, stream) == 1);
        CHECK(fwrite(pixels, sizeof(pixels), 1, stream) == 1);
        fclose(stream);
        artwork_pixels(path, 952);
        remove(path);
    }
    uint32_t truncated_header[33] = { 1, 10, 8, UINT32_MAX };
    for (size_t length = 0; length < sizeof(truncated_header); length++) {
        tig_art_flush();
        FILE* stream = fopen(path, "wb");
        CHECK(stream != NULL); if (stream == NULL) return;
        CHECK(fwrite(truncated_header, 1, length, stream) == length);
        fclose(stream);
        artwork = path;
        tig_art_id_t id;
        TigArtFrameData data;
        CHECK(tig_art_interface_id_create(952, 0, 0, 0, &id) == TIG_OK);
        CHECK(tig_art_frame_data(id, &data) != TIG_OK);
        remove(path);
    }
}

static void malformed_art(void)
{
    for (int trial = 0; trial < 5; trial++) {
        uint32_t header[33] = {0}; header[0] = 1; header[1] = 10; header[2] = 8; header[3] = 1; header[8] = trial == 3 ? 0 : 1;
        uint32_t palette[256] = {0};
        int32_t frame[7] = {4,3,2,0,0,0,0};
        uint8_t payload[2] = {127, 2};
        if (trial == 0) frame[0] = frame[1] = 50000;
        if (trial == 2) payload[0] = 1;
        if (trial == 4) frame[0] = -4;
        FILE* stream = fopen("art-decode-invalid.art", "wb");
        CHECK(stream != NULL); if (stream == NULL) return;
        CHECK(fwrite(header, sizeof(header), 1, stream) == 1);
        CHECK(fwrite(palette, sizeof(palette), 1, stream) == 1);
        CHECK(fwrite(frame, sizeof(frame), 1, stream) == 1);
        CHECK(fwrite(payload, sizeof(payload), 1, stream) == 1);
        fclose(stream);
        artwork = "art-decode-invalid.art";
        tig_art_id_t aid;
        CHECK(tig_art_interface_id_create(951, 0, 0, 0, &aid) == TIG_OK);
        TigArtFrameData data;
        CHECK(tig_art_frame_data(aid, &data) != TIG_OK);
        remove("art-decode-invalid.art");
    }
}

static void frame_conversion(void)
{
    uint8_t source[15] = { 0, 1, 2, 3, 4, 5, 0, 7, 8, 9, 10, 11, 12, 0, 14 };
    uint32_t palette[256];
    for (int index = 0; index < 256; index++) palette[index] = 0x123456u + index * 0x010301u;
    for (int mirrored = 0; mirrored < 2; mirrored++) for (int padding = 0; padding < 3; padding++) {
        int pitch = 28 + padding * 4;
        uint8_t storage[2 + 5 * 36];
        memset(storage, 0xa5, sizeof(storage));
        CHECK(tig_art_frame_convert(source, sizeof(source), 5, 3, palette, mirrored, storage + 1, 7, 5, pitch));
        for (size_t offset = 0; offset < sizeof(storage); offset++) {
            size_t relative = offset - 1;
            bool written = offset > 0 && relative / pitch < 3 && relative % pitch < 20;
            uint8_t expected = 0xa5;
            if (written) {
                int y = (int)(relative / pitch), x = (int)(relative % pitch / 4);
                uint8_t index = source[y * 5 + (mirrored ? 4 - x : x)];
                uint32_t color = index == 0 ? 0 : palette[index];
                memcpy(&expected, (uint8_t*)&color + relative % 4, 1);
            }
            CHECK(storage[offset] == expected);
        }
    }
    uint8_t target[120], before[120];
    memset(target, 0xa5, sizeof(target)); memcpy(before, target, sizeof(target));
    CHECK(!tig_art_frame_convert(NULL, 15, 5, 3, palette, false, target, 5, 3, 20));
    CHECK(!tig_art_frame_convert(source, 14, 5, 3, palette, false, target, 5, 3, 20));
    CHECK(!tig_art_frame_convert(source, 15, 5, 3, NULL, false, target, 5, 3, 20));
    CHECK(!tig_art_frame_convert(source, 15, 5, 3, palette, false, NULL, 5, 3, 20));
    CHECK(!tig_art_frame_convert(source, 15, -5, 3, palette, false, target, 5, 3, 20));
    CHECK(!tig_art_frame_convert(source, 15, 5, 3, palette, false, target, 4, 3, 20));
    CHECK(!tig_art_frame_convert(source, 15, 5, 3, palette, false, target, 5, 2, 20));
    CHECK(!tig_art_frame_convert(source, 15, 5, 3, palette, false, target, 5, 3, 19));
    CHECK(!tig_art_frame_convert(source, 15, 5, 3, palette, false, target, INT_MAX, INT_MAX, INT_MAX));
    CHECK(memcmp(target, before, sizeof(target)) == 0);
}

static void video_cache_batches(void)
{
    const char* path = "art-cache-fixture.art";
    for (int invalid = 0; invalid < 2; invalid++) {
        tig_art_flush();
        uint32_t header[33] = { 1, 10, 8, 1 }; header[8] = 2;
        uint32_t palette[256] = { 0x00ff00, 0xff0000, 0x0000ff };
        int32_t frames[2][7] = { { 4, 3, 12 }, { 2, 2, 4 } };
        uint8_t source[16] = { 1, 0, 2, 1, 2, 1, 0, 2, 1, 2, 1, 0, 2, 1, 0, 2 };
        if (invalid) frames[1][0] = frames[1][1] = frames[1][2] = 0;
        FILE* stream = fopen(path, "wb"); CHECK(stream != NULL); if (stream == NULL) return;
        CHECK(fwrite(header, sizeof(header), 1, stream) == 1);
        CHECK(fwrite(palette, sizeof(palette), 1, stream) == 1);
        CHECK(fwrite(frames, sizeof(frames), 1, stream) == 1);
        CHECK(fwrite(source, invalid ? 12 : 16, 1, stream) == 1);
        fclose(stream);
        artwork = path;
        tig_art_id_t id; CHECK(tig_art_interface_id_create(954, 0, 1, 0, &id) == TIG_OK);
        TigArtFrameData image; CHECK(tig_art_frame_data(id, &image) == TIG_OK);
        TigArtCacheStatistics loaded; tig_art_cache_statistics(&loaded);
        CHECK(loaded.entries == 1 && loaded.video_tables == 0 && loaded.video_buffers == 0 && loaded.video_bytes == 0);
        TigVideoBuffer* target = NULL;
        TigVideoBufferCreateInfo create = { .width = 8, .height = 7, .background_color = 0x654321 };
        CHECK(tig_video_buffer_create(&create, &target) == TIG_OK);
        if (target == NULL) { tig_art_flush(); remove(path); return; }
        TigRect src = { 0, 0, 4, 3 }, dst = { 1, 1, 4, 3 };
        TigArtBlitInfo blit = { .art_id = id, .src_rect = &src, .dst_rect = &dst, .dst_video_buffer = target };
        TigArtCacheStatistics committed = { 0 };
        for (int attempt = 0; attempt < 4; attempt++) {
            if (attempt == 2) tig_art_cached_palettes_refresh();
            CHECK(tig_video_buffer_fill(target, NULL, 0x654321) == TIG_OK);
            CHECK(tig_art_blit(&blit) == TIG_OK);
            TigArtCacheStatistics statistics; tig_art_cache_statistics(&statistics);
            if (invalid) {
                CHECK(statistics.video_tables == 0 && statistics.video_buffers == 0 && statistics.video_bytes == 0);
                CHECK(statistics.system_bytes == loaded.system_bytes);
            } else {
                CHECK(statistics.video_tables == 1 && statistics.video_buffers == 2 && statistics.video_bytes >= 64);
                CHECK(statistics.system_bytes == loaded.system_bytes + 2 * sizeof(TigVideoBuffer*));
                if (attempt == 0) committed = statistics;
                else CHECK(statistics.video_bytes == committed.video_bytes && statistics.system_bytes == committed.system_bytes);
            }
            TigVideoBufferData pixels; CHECK(tig_video_buffer_lock(target) == TIG_OK);
            CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
            for (int y = 0; y < 7; y++) for (int x = 0; x < 8; x++) {
                uint32_t actual, expected = 0x654321;
                if (x >= 1 && x < 5 && y >= 1 && y < 4) {
                    uint8_t index = source[(y - 1) * 4 + x - 1];
                    if (index != 0) expected = palette[index];
                }
                memcpy(&actual, (uint8_t*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
                CHECK((actual & 0xffffff) == expected);
            }
            CHECK(tig_video_buffer_unlock(target) == TIG_OK);
        }
        tig_video_buffer_destroy(target);
        tig_art_flush();
        TigArtCacheStatistics empty; tig_art_cache_statistics(&empty);
        CHECK(empty.entries == 0 && empty.video_tables == 0 && empty.video_buffers == 0 && empty.system_bytes == 0 && empty.video_bytes == 0);
        remove(path);
    }
}

static void roof_video_batches(void)
{
    const char* path = "art-roof-cache-fixture.art";
    for (int frame_count = 9; frame_count <= 13; frame_count += 4) {
        tig_art_flush();
        uint32_t header[33] = { 1, 10, 8, 1, 1 }; header[8] = frame_count;
        uint32_t palettes[2][256];
        for (int index = 0; index < 256; index++) {
            palettes[0][index] = index << 16;
            palettes[1][index] = index << 8;
        }
        int32_t frames[13][7] = { 0 };
        uint8_t source[256]; size_t offsets[13], total = 0;
        for (int frame = 0; frame < frame_count; frame++) {
            frames[frame][0] = 2 + frame % 3; frames[frame][1] = 2 + frame % 2;
            frames[frame][2] = frames[frame][0] * frames[frame][1];
            offsets[frame] = total;
            for (int pixel = 0; pixel < frames[frame][2]; pixel++) source[total++] = pixel % 4 == 0 ? 0 : frame * 10 + pixel + 1;
        }
        FILE* stream = fopen(path, "wb"); CHECK(stream != NULL); if (stream == NULL) return;
        CHECK(fwrite(header, sizeof(header), 1, stream) == 1);
        CHECK(fwrite(palettes, sizeof(palettes), 1, stream) == 1);
        CHECK(fwrite(frames, sizeof(frames[0]), frame_count, stream) == (size_t)frame_count);
        CHECK(fwrite(source, total, 1, stream) == 1); fclose(stream);
        artwork = path;
        tig_art_id_t id; CHECK(tig_art_roof_id_create(1, 0, 0, 0, &id) == TIG_OK);
        TigVideoBuffer* target = NULL;
        TigVideoBufferCreateInfo create = { .width = 8, .height = 7 };
        CHECK(tig_video_buffer_create(&create, &target) == TIG_OK);
        if (target == NULL) { remove(path); return; }
        TigArtCacheStatistics committed = { 0 };
        for (int iteration = 0; iteration < 14; iteration++) {
            if (iteration == 13) tig_art_cached_palettes_refresh();
            int piece = iteration % 13, frame = piece >= 9 ? piece - 9 : piece;
            int width = frames[frame][0], height = frames[frame][1];
            TigRect src = { 0, 0, width, height }, dst = { 1, 1, width, height };
            TigArtBlitInfo blit = { .art_id = tig_art_roof_id_piece_set(id, piece), .src_rect = &src,
                .dst_rect = &dst, .dst_video_buffer = target };
            CHECK(tig_video_buffer_fill(target, NULL, 0x654321) == TIG_OK);
            CHECK(tig_art_blit(&blit) == TIG_OK);
            TigVideoBufferData pixels; CHECK(tig_video_buffer_lock(target) == TIG_OK);
            CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
            for (int y = 0; y < 7; y++) for (int x = 0; x < 8; x++) {
                uint32_t actual, expected = 0x654321;
                if (x >= 1 && x <= width && y >= 1 && y <= height) {
                    int source_x = piece >= 9 ? width - x : x - 1;
                    uint8_t index = source[offsets[frame] + (y - 1) * width + source_x];
                    if (index != 0) expected = palettes[piece >= 9][index];
                }
                memcpy(&actual, (uint8_t*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
                CHECK((actual & 0xffffff) == expected);
            }
            CHECK(tig_video_buffer_unlock(target) == TIG_OK);
            TigArtCacheStatistics statistics; tig_art_cache_statistics(&statistics);
            CHECK(statistics.entries == 1 && statistics.video_tables == 1 && statistics.video_buffers == 13);
            if (iteration == 0) committed = statistics;
            else CHECK(statistics.system_bytes == committed.system_bytes && statistics.video_bytes == committed.video_bytes);
        }
        tig_video_buffer_destroy(target);
        tig_art_flush(); remove(path);
    }
}

static uint8_t reference_gradient_channel(double value)
{
    return value < 0 ? 0 : value > 255 ? 255 : (uint8_t)value;
}

static int reference_scaled_sample(int64_t offset, int source_extent, int destination_extent)
{
    int low = 0, high = source_extent - 1;
    uint64_t position = (uint64_t)offset * source_extent * 2;
    while (low < high) {
        int middle = low + (high - low) / 2;
        uint64_t boundary = ((uint64_t)middle * 2 + 1) * destination_extent;
        if (position > boundary) low = middle + 1;
        else high = middle;
    }
    return low;
}

static void art_blit_modes(tig_art_id_t id, const uint8_t* image, const uint32_t* palette, TigVideoBuffer* target, bool scaled)
{
    const TigRect copy_sources[] = { { 0, 0, 6, 5 }, { 1, 2, 4, 3 }, { -1, -2, 7, 7 },
        { -1, -1, INT_MAX, INT_MAX }, { INT_MIN, 0, INT_MAX, 5 }, { INT_MAX, 0, INT_MAX, 5 },
        { -INT_MAX + 3, 0, INT_MAX, 5 } };
    const TigRect copy_destinations[] = { { 1, 1, 6, 5 }, { -1, 1, 4, 3 }, { 0, 0, 7, 7 },
        { -2, -2, INT_MAX, INT_MAX }, { 0, 0, INT_MAX, 5 }, { 0, 0, INT_MAX, 5 },
        { -INT_MAX + 4, 0, INT_MAX, 5 } };
    const TigRect scaled_sources[] = {
        { 0, 0, 6, 5 }, { 0, 0, 6, 5 }, { 0, 0, 6, 5 }, { 2, 1, 1, 1 },
        { -2, -1, 8, 7 }, { 2, 1, 7, 6 }, { 1, 1, 4, 3 }, { 0, 0, 6, 5 },
        { INT_MIN, 0, INT_MAX, 5 }, { INT_MAX, 0, INT_MAX, 5 }, { -1, -1, INT_MAX, INT_MAX },
        { 0, 0, 6, 5 }, { -INT_MAX + 3, 0, INT_MAX, 5 }, { -1, -1, 6, 5 },
        { 0, 0, 6, 5 }, { 0, 0, 6, 5 }
    };
    const TigRect scaled_destinations[] = {
        { 0, 0, 12, 10 }, { 1, 1, 3, 2 }, { -3, -2, 24, 20 }, { 0, 0, 7, 4 },
        { -3, -2, 12, 10 }, { 1, 0, 4, 3 }, { -1, 1, 9, 7 }, { INT_MIN, INT_MAX, INT_MAX, INT_MAX },
        { 0, 0, 8, 7 }, { 0, 0, 8, 7 }, { -2, -2, INT_MAX, INT_MAX - 1 },
        { -INT_MAX + 4, 0, INT_MAX, 10 }, { -INT_MAX + 4, 0, INT_MAX, 10 }, { 0, 0, 1, 1 },
        { -4, -3, 9, 8 }, { 0, 0, INT_MAX, INT_MAX }
    };
    const TigRect* sources = scaled ? scaled_sources : copy_sources;
    const TigRect* destinations = scaled ? scaled_destinations : copy_destinations;
    int case_count = scaled ? 16 : 7;
    const unsigned int blends[] = { 0, TIG_ART_BLT_BLEND_ADD, TIG_ART_BLT_BLEND_SUB, TIG_ART_BLT_BLEND_MUL,
        TIG_ART_BLT_BLEND_ALPHA_AVG, TIG_ART_BLT_BLEND_ALPHA_CONST, TIG_ART_BLT_BLEND_ALPHA_SRC,
        TIG_ART_BLT_BLEND_ALPHA_LERP_X, TIG_ART_BLT_BLEND_ALPHA_LERP_Y, TIG_ART_BLT_BLEND_ALPHA_LERP_BOTH,
        TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S, TIG_ART_BLT_BLEND_ALPHA_STIPPLE_D };
    uint32_t masks[6] = { 0x204060, 0xc08040, 0x4080c0, 0xff8040, 0x40ff80, 0x8040ff };
    const uint32_t tint = 0x80c040, background = 0x654321;
    for (int mode = 0; mode < 3; mode++) for (int blend = 0; blend < 12; blend++) {
        for (int flip = 0; flip < 4; flip++) for (int test = 0; test < case_count; test++) {
            TigRect src = sources[test], dst = destinations[test];
            TigArtBlitInfo info = { .art_id = id, .src_rect = &src, .dst_rect = &dst,
                .dst_video_buffer = target, .color = tint, .blend_colors = masks, .alpha = { 96, 96, 96, 96 },
                .flags = TIG_ART_BLT_PALETTE_ORIGINAL | blends[blend]
                    | (mode == 1 ? TIG_ART_BLT_BLEND_COLOR_CONST : mode == 2 ? TIG_ART_BLT_BLEND_COLOR_ARRAY : 0)
                    | (flip & 1 ? TIG_ART_BLT_FLIP_X : 0) | (flip & 2 ? TIG_ART_BLT_FLIP_Y : 0) };
            if (scaled) {
                info.alpha[0] = 30; info.alpha[1] = 210; info.alpha[2] = 180; info.alpha[3] = 60;
            }
            CHECK(tig_video_buffer_fill(target, NULL, background) == TIG_OK);
            CHECK(tig_art_blit(&info) == TIG_OK);
            CHECK(memcmp(&src, &sources[test], sizeof(src)) == 0);
            CHECK(memcmp(&dst, &destinations[test], sizeof(dst)) == 0);
            TigVideoBufferData pixels;
            CHECK(tig_video_buffer_lock(target) == TIG_OK);
            CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
            bool same = true;
            for (int y = 0; y < 7; y++) for (int x = 0; x < 8; x++) {
                int64_t ox = (int64_t)x - dst.x, oy = (int64_t)y - dst.y;
                bool covered = ox >= 0 && oy >= 0 && ox < dst.width && oy < dst.height;
                int64_t sx = (int64_t)src.x + (scaled && covered ? reference_scaled_sample(ox, src.width, dst.width) : ox);
                int64_t sy = (int64_t)src.y + (scaled && covered ? reference_scaled_sample(oy, src.height, dst.height) : oy);
                uint32_t expected = background, actual;
                if (covered && sx >= 0 && sx < 6 && sy >= 0 && sy < 5) {
                    int sample_x = flip & 1 ? 5 - (int)sx : (int)sx;
                    int sample_y = flip & 2 ? 4 - (int)sy : (int)sy;
                    uint8_t index = image[sample_y * 6 + sample_x];
                    bool draw = index != 0 && (blend != 10 || ((sx ^ sy) & 1) != 0)
                        && (blend != 11 || ((x ^ y) & 1) != 0);
                    if (draw) {
                        uint32_t base = tig_color_index_of(palette[index]);
                        uint32_t color = mode == 0 ? base : tig_color_mul(base, mode == 1 ? tint : masks[sx]);
                        if (blend == 1) expected = tig_color_add(background, color);
                        else if (blend == 2) expected = tig_color_sub(color, background);
                        else if (blend == 3) expected = tig_color_mul(color, background);
                        else if (blend == 4) expected = tig_color_blend_alpha(color, background, tig_color_rgb_to_grayscale(color));
                        else if (blend == 5) expected = tig_color_blend_alpha(color, background, info.alpha[0]);
                        else if (blend >= 7 && blend <= 9) {
                            int alpha = 96;
                            if (scaled) {
                                if (blend == 7) alpha = 30 + 30 * (int)sx;
                                else if (blend == 8) alpha = 30 + 6 * (int)sy;
                                else alpha = 30 + 6 * (int)sy + (30 - 2 * (int)sy) * (int)sx;
                            }
                            expected = tig_color_blend_alpha(color, background, alpha);
                        }
                        else if (blend == 6) expected = tig_color_blend_alpha(mode == 0 ? base : mode == 1 ? tint : masks[sx], background, tig_color_alpha(base));
                        else expected = color;
                    }
                }
                memcpy(&actual, (uint8_t*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
                if ((actual & 0xffffff) != (expected & 0xffffff)) same = false;
            }
            CHECK(tig_video_buffer_unlock(target) == TIG_OK);
            if (!same) fprintf(stderr, "ART mismatch: scaled=%d mode=%d blend=%d flip=%d case=%d\n", scaled, mode, blend, flip, test);
            CHECK(same);
        }
    }
}

static void gradient_art(void)
{
    const char* path = "art-gradient-fixture.art";
    uint32_t header[33] = { 1, 10, 8, 1 };
    header[8] = 1;
    uint32_t palette[256];
    TigPalette override_palette;
    for (int index = 0; index < 256; index++) {
        palette[index] = index * 0x010101u;
        override_palette.colors[index] = tig_color_make(index, 255 - index, index / 2);
    }
    int32_t frame[7] = { 6, 5, 30 };
    uint8_t source[30];
    for (int index = 0; index < 30; index++) source[index] = index % 7 == 0 ? 0 : index * 7;
    FILE* stream = fopen(path, "wb");
    CHECK(stream != NULL); if (stream == NULL) return;
    CHECK(fwrite(header, sizeof(header), 1, stream) == 1);
    CHECK(fwrite(palette, sizeof(palette), 1, stream) == 1);
    CHECK(fwrite(frame, sizeof(frame), 1, stream) == 1);
    CHECK(fwrite(source, sizeof(source), 1, stream) == 1);
    fclose(stream);
    artwork = path;
    tig_art_id_t id;
    CHECK(tig_art_interface_id_create(953, 0, 0, 0, &id) == TIG_OK);
    TigVideoBuffer* target = NULL;
    TigVideoBufferCreateInfo create = { .width = 8, .height = 7, .background_color = 0x654321 };
    CHECK(tig_video_buffer_create(&create, &target) == TIG_OK);
    if (target == NULL) { remove(path); return; }
    art_blit_modes(id, source, palette, target, false);
    art_blit_modes(id, source, palette, target, true);
    const TigRect sources[] = { { 0, 0, 6, 5 }, { 1, 2, 4, 3 }, { -1, -2, 7, 7 },
        { -1, -1, INT_MAX, INT_MAX }, { INT_MIN, 0, INT_MAX, 5 }, { INT_MAX, 0, INT_MAX, 5 } };
    const TigRect destinations[] = { { 1, 1, 6, 5 }, { -1, 1, 4, 3 }, { 0, 0, 7, 7 },
        { -2, -2, INT_MAX, INT_MAX }, { 0, 0, INT_MAX, 5 }, { 0, 0, INT_MAX, 5 } };
    const TigRect gradients[] = { { 0, 0, 8, 8 }, { 0, 0, 4, 4 },
        { INT_MIN, 0, 1, 5 }, { INT_MAX, 0, 1, 5 }, { 0, INT_MIN, 6, 1 }, { 0, INT_MAX, 6, 1 },
        { -1, 0, 1, 5 }, { 1, 0, 1, 5 } };
    for (int palette_mode = 0; palette_mode < 3; palette_mode++) for (int flip = 0; flip < 4; flip++) {
        for (int test = 0; test < 6; test++) for (int gradient_index = 0; gradient_index < 8; gradient_index++) {
            TigRect src = sources[test], dst = destinations[test], gradient = gradients[gradient_index];
            uint32_t colors[4] = { 0, 0xc0c0c0, 0xc0c0c0, 0 };
            if (gradient_index == 1) { colors[0] = 0x204060; colors[1] = 0x80c020; colors[2] = 0xf030d0; colors[3] = 0x40a080; }
            if (gradient_index == 4 || gradient_index == 5) { colors[1] = 0; colors[3] = 0xc0c0c0; }
            TigArtBlitInfo info = { .art_id = id, .src_rect = &src, .dst_rect = &dst, .dst_video_buffer = target,
                .flags = TIG_ART_BLT_BLEND_COLOR_LERP | (flip & 1 ? TIG_ART_BLT_FLIP_X : 0) | (flip & 2 ? TIG_ART_BLT_FLIP_Y : 0),
                .blend_rect = &gradient, .blend_colors = colors, .palette = &override_palette };
            if (palette_mode == 1) info.flags |= TIG_ART_BLT_PALETTE_ORIGINAL;
            if (palette_mode == 2) info.flags |= TIG_ART_BLT_PALETTE_OVERRIDE;
            CHECK(tig_video_buffer_fill(target, NULL, 0x654321) == TIG_OK);
            CHECK(tig_art_blit(&info) == TIG_OK);
            CHECK(memcmp(&src, &sources[test], sizeof(src)) == 0);
            CHECK(memcmp(&dst, &destinations[test], sizeof(dst)) == 0);
            CHECK(memcmp(&gradient, &gradients[gradient_index], sizeof(gradient)) == 0);
            TigVideoBufferData pixels;
            CHECK(tig_video_buffer_lock(target) == TIG_OK);
            CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
            bool same = true;
            for (int y = 0; y < 7; y++) for (int x = 0; x < 8; x++) {
                int64_t ox = (int64_t)x - dst.x, oy = (int64_t)y - dst.y;
                int64_t sx = (int64_t)src.x + ox, sy = (int64_t)src.y + oy;
                uint32_t expected = 0x654321, actual;
                if (ox >= 0 && oy >= 0 && ox < src.width && oy < src.height && sx >= 0 && sx < 6 && sy >= 0 && sy < 5) {
                    int sample_x = flip & 1 ? 5 - (int)sx : (int)sx;
                    int sample_y = flip & 2 ? 4 - (int)sy : (int)sy;
                    uint8_t index = source[sample_y * 6 + sample_x];
                    if (index != 0) {
                        double gx = (double)(sx - gradient.x) / gradient.width;
                        double gy = (double)(sy - gradient.y) / gradient.height;
                        uint8_t components[3];
                        for (int channel = 0; channel < 3; channel++) {
                            int shift = 16 - channel * 8;
                            int tl = (colors[0] >> shift) & 255, tr = (colors[1] >> shift) & 255;
                            int br = (colors[2] >> shift) & 255, bl = (colors[3] >> shift) & 255;
                            double left = tl + (bl - tl) * gy;
                            double right = tr + (br - tr) * gy;
                            components[channel] = reference_gradient_channel(left + (right - left) * gx);
                        }
                        uint32_t base = palette_mode == 2 ? override_palette.colors[index] : tig_color_index_of(palette[index]);
                        expected = tig_color_mul(base, tig_color_make(components[0], components[1], components[2]));
                    }
                }
                memcpy(&actual, (uint8_t*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
                if ((actual & 0xffffff) != (expected & 0xffffff)) same = false;
            }
            CHECK(tig_video_buffer_unlock(target) == TIG_OK);
            if (!same) fprintf(stderr, "Gradient mismatch: palette=%d flip=%d case=%d gradient=%d\n", palette_mode, flip, test, gradient_index);
            CHECK(same);
        }
    }
    TigRect rect = { 0, 0, 6, 5 }, gradient = rect;
    uint32_t colors[4] = { 0, 0xffffff, 0xffffff, 0 };
    TigArtBlitInfo info = { .art_id = id, .src_rect = &rect, .dst_rect = &rect, .dst_video_buffer = target,
        .flags = TIG_ART_BLT_BLEND_COLOR_LERP, .blend_rect = &gradient, .blend_colors = colors };
    CHECK(tig_video_buffer_fill(target, NULL, 0x654321) == TIG_OK);
    CHECK(tig_art_blit(NULL) == TIG_ERR_INVALID_PARAM);
    info.blend_colors = NULL; CHECK(tig_art_blit(&info) == TIG_ERR_INVALID_PARAM); info.blend_colors = colors;
    info.blend_rect = NULL; CHECK(tig_art_blit(&info) == TIG_ERR_INVALID_PARAM); info.blend_rect = &gradient;
    gradient.width = 0; CHECK(tig_art_blit(&info) == TIG_ERR_INVALID_PARAM); gradient.width = 6;
    gradient.height = -1; CHECK(tig_art_blit(&info) == TIG_ERR_INVALID_PARAM); gradient.height = 5;
    TigRect scaled = { 0, 0, 12, 10 };
    info.dst_rect = &scaled; CHECK(tig_art_blit(&info) == TIG_ERR_INVALID_PARAM); info.dst_rect = &rect;
    info.flags |= TIG_ART_BLT_PALETTE_OVERRIDE; info.palette = NULL;
    CHECK(tig_art_blit(&info) == TIG_ERR_INVALID_PARAM);
    TigVideoBufferData pixels;
    CHECK(tig_video_buffer_lock(target) == TIG_OK);
    CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
    for (int y = 0; y < 7; y++) for (int x = 0; x < 8; x++) {
        uint32_t actual;
        memcpy(&actual, (uint8_t*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
        CHECK((actual & 0xffffff) == 0x654321);
    }
    CHECK(tig_video_buffer_unlock(target) == TIG_OK);
    tig_video_buffer_destroy(target);
    tig_art_flush();
    remove(path);
}

int main(int argc, char** argv)
{
    CHECK(SDL_Init(0)); packets();
    CHECK(tig_color_init(NULL) == TIG_OK);
    tig_color_set_rgba_settings(0x00FF0000,0x0000FF00,0x000000FF,0xFF000000);
    TigInitInfo info = {0}; info.bpp = 32; info.art_file_path_resolver = art_path; info.art_id_reset_func = reset_art;
    CHECK(tig_file_repository_add("."));
    CHECK(tig_palette_init(&info) == TIG_OK); CHECK(tig_art_init(&info) == TIG_OK);
    frame_conversion();
    video_cache_batches();
    roof_video_batches();
    gradient_art();
    synthetic_art();
    serialized_headers();
    malformed_art();
    for (int i = 1; i < argc; i++) artwork_pixels(argv[i], 900 + i);
    CHECK(argc > 1);
    tig_art_exit(); tig_file_repository_remove_all(); tig_palette_exit(); tig_color_exit(); SDL_Quit();
    printf("ART RLE oracle and real artwork pixels: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
