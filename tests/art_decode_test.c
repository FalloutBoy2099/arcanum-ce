#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tig/tig.h>
#include <tig/art_decode.h>

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

int main(int argc, char** argv)
{
    CHECK(SDL_Init(0)); packets();
    CHECK(tig_color_init(NULL) == TIG_OK);
    tig_color_set_rgba_settings(0x00FF0000,0x0000FF00,0x000000FF,0xFF000000);
    TigInitInfo info = {0}; info.bpp = 32; info.art_file_path_resolver = art_path; info.art_id_reset_func = reset_art;
    CHECK(tig_file_repository_add("."));
    CHECK(tig_palette_init(&info) == TIG_OK); CHECK(tig_art_init(&info) == TIG_OK);
    synthetic_art();
    malformed_art();
    for (int i = 1; i < argc; i++) artwork_pixels(argv[i], 900 + i);
    CHECK(argc > 1);
    tig_art_exit(); tig_file_repository_remove_all(); tig_palette_exit(); tig_color_exit(); SDL_Quit();
    printf("ART RLE oracle and real artwork pixels: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
