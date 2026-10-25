#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <movie_decoder.h>

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s (%s)\n", __LINE__, #value, movie_decoder_error()); failures++; } } while (0)

typedef struct AudioCheck {
    FILE* reference;
    size_t frames;
    bool nonzero;
} AudioCheck;

static bool check_audio(void* context, const float* samples, int frames, int channels, int frequency)
{
    AudioCheck* check = context;
    CHECK(channels == 2 && frequency == 44100);
    CHECK(frames > 0);
    size_t count = (size_t)frames * channels;
    for (size_t i = 0; i < count; i++) {
        CHECK(isfinite(samples[i]));
        if (samples[i] != 0) check->nonzero = true;
    }
    if (check->reference != NULL) {
        float* expected = malloc(count * sizeof(float));
        CHECK(expected != NULL);
        if (expected != NULL) {
            CHECK(fread(expected, sizeof(float), count, check->reference) == count);
            CHECK(memcmp(expected, samples, count * sizeof(float)) == 0);
            free(expected);
        }
    }
    check->frames += frames;
    return true;
}

static bool reject_audio(void* context, const float* samples, int frames, int channels, int frequency)
{
    (void)context; (void)samples; (void)frames; (void)channels; (void)frequency;
    return false;
}

static void compare_first_frame(const MovieFrame* frame, const char* path)
{
    FILE* stream = fopen(path, "rb");
    CHECK(stream != NULL);
    if (stream == NULL) return;
    size_t bytes = (size_t)frame->width * 4;
    unsigned char* row = malloc(bytes);
    CHECK(row != NULL);
    if (row != NULL) {
        for (int y = 0; y < frame->height; y++) {
            CHECK(fread(row, 1, bytes, stream) == bytes);
            CHECK(memcmp(row, frame->pixels + (size_t)y * frame->pitch, bytes) == 0);
        }
        CHECK(fgetc(stream) == EOF);
        free(row);
    }
    fclose(stream);
}

static void real_movie(const char* path, const char* pixels, const char* audio)
{
    MovieDecoder* decoder = movie_decoder_open(path, 0, 0, 0);
    CHECK(decoder != NULL);
    if (decoder == NULL) return;
    AudioCheck samples = { 0 };
    if (audio != NULL) { samples.reference = fopen(audio, "rb"); CHECK(samples.reference != NULL); }
    MovieFrame frame;
    int count = 0;
    int result;
    double previous = -1;
    while ((result = movie_decoder_next(decoder, &frame, check_audio, &samples)) > 0) {
        CHECK(frame.width == 800 && frame.height == 400 && frame.pitch >= 3200);
        CHECK(frame.seconds > previous && frame.duration > 0);
        CHECK(fabs(frame.seconds - count * (241.0 / 6700)) < 0.000001);
        if (count == 0 && pixels != NULL) compare_first_frame(&frame, pixels);
        previous = frame.seconds;
        count++;
    }
    CHECK(result == 0 && count == 201);
    CHECK(samples.nonzero && samples.frames == 320640);
    CHECK(movie_decoder_next(decoder, &frame, NULL, NULL) == 0);
    if (samples.reference != NULL) { CHECK(fgetc(samples.reference) == EOF); fclose(samples.reference); }
    movie_decoder_close(decoder);
    decoder = movie_decoder_open(path, 0, 320, 240);
    CHECK(decoder != NULL);
    if (decoder != NULL) {
        CHECK(movie_decoder_next(decoder, &frame, NULL, NULL) == 1);
        CHECK(frame.width == 320 && frame.height == 160);
        movie_decoder_close(decoder);
    }
    decoder = movie_decoder_open(path, 12345, 0, 0);
    CHECK(decoder != NULL);
    if (decoder != NULL) {
        AudioCheck muted = { 0 };
        CHECK(movie_decoder_next(decoder, &frame, check_audio, &muted) == 1);
        CHECK(muted.frames == 0);
        movie_decoder_close(decoder);
    }
    decoder = movie_decoder_open(path, 0, 0, 0);
    CHECK(decoder != NULL);
    if (decoder != NULL) {
        CHECK(movie_decoder_next(decoder, &frame, reject_audio, NULL) == -1);
        movie_decoder_close(decoder);
    }
    FILE* original = fopen(path, "rb");
    FILE* altered = fopen("track-id.bik", "wb");
    CHECK(original != NULL && altered != NULL);
    if (original != NULL && altered != NULL) {
        unsigned char bytes[4096];
        size_t copied;
        while ((copied = fread(bytes, 1, sizeof(bytes), original)) > 0) CHECK(fwrite(bytes, 1, copied, altered) == copied);
        fclose(altered); altered = fopen("track-id.bik", "r+b");
        CHECK(altered != NULL);
        if (altered != NULL) {
            const unsigned char track_id[] = { 7, 0, 0, 0 };
            CHECK(fseek(altered, 52, SEEK_SET) == 0);
            CHECK(fwrite(track_id, sizeof(track_id), 1, altered) == 1);
            fclose(altered); altered = NULL;
            decoder = movie_decoder_open("track-id.bik", 7, 0, 0);
            CHECK(decoder != NULL);
            if (decoder != NULL) {
                AudioCheck selected = { 0 };
                CHECK(movie_decoder_next(decoder, &frame, check_audio, &selected) == 1);
                CHECK(selected.frames > 0);
                movie_decoder_close(decoder);
            }
        }
        rewind(original);
        altered = fopen("truncated.bik", "wb");
        CHECK(altered != NULL);
        if (altered != NULL) {
            copied = fread(bytes, 1, sizeof(bytes), original);
            fwrite(bytes, 1, copied, altered); fclose(altered); altered = NULL;
            decoder = movie_decoder_open("truncated.bik", 0, 0, 0);
            if (decoder != NULL) {
                int remaining = 0;
                while ((result = movie_decoder_next(decoder, &frame, NULL, NULL)) > 0 && remaining < 202) remaining++;
                CHECK(result < 0 && remaining < 201);
                movie_decoder_close(decoder);
            }
        }
    }
    if (original != NULL) fclose(original);
    if (altered != NULL) fclose(altered);
    remove("track-id.bik"); remove("truncated.bik");
    printf("Bink reference: %d frames, %zu audio frames\n", count, samples.frames);
}

typedef struct AssetAudio {
    size_t frames;
    int frequency;
    int channels;
} AssetAudio;

static bool asset_audio(void* context, const float* samples, int frames, int channels, int frequency)
{
    AssetAudio* audio = context;
    CHECK(frames > 0 && channels > 0 && channels <= 8 && frequency > 0);
    for (size_t i = 0; i < (size_t)frames * channels; i += 31) CHECK(isfinite(samples[i]));
    audio->frames += frames;
    audio->channels = channels;
    audio->frequency = frequency;
    return true;
}

static void asset_movie(const char* path)
{
    FILE* file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file == NULL) return;
    unsigned char header[44];
    CHECK(fread(header, 1, sizeof(header), file) == sizeof(header));
    fclose(file);
    unsigned expected_frames = (unsigned)header[8] | (unsigned)header[9] << 8 | (unsigned)header[10] << 16 | (unsigned)header[11] << 24;
    MovieDecoder* decoder = movie_decoder_open(path, 0, 640, 480);
    CHECK(decoder != NULL);
    if (decoder == NULL) return;
    MovieFrame frame;
    AssetAudio audio = { 0 };
    int result;
    unsigned count = 0;
    double last = -1;
    double end = 0;
    while ((result = movie_decoder_next(decoder, &frame, asset_audio, &audio)) > 0) {
        CHECK(frame.width <= 640 && frame.height <= 480 && frame.width > 0 && frame.height > 0);
        CHECK(frame.pitch >= frame.width * 4 && frame.pixels != NULL);
        CHECK(frame.seconds > last && frame.duration > 0);
        last = frame.seconds;
        end = frame.seconds + frame.duration;
        count++;
        if (count > expected_frames) { CHECK(false); break; }
    }
    CHECK(result == 0 && count == expected_frames);
    if (audio.frequency > 0) CHECK(fabs((double)audio.frames / audio.frequency - end) < 1.0);
    printf("Movie asset: %s frames=%u/%u seconds=%.6f audio=%zu/%d channels=%d\n", path, count, expected_frames, end, audio.frames, audio.frequency, audio.channels);
    movie_decoder_close(decoder);
}

int main(int argc, char** argv)
{
    if (argc == 3 && strcmp(argv[1], "--asset") == 0) { asset_movie(argv[2]); return failures != 0; }
    CHECK(movie_decoder_open(NULL, 0, 0, 0) == NULL);
    CHECK(movie_decoder_open("", 0, 0, 0) == NULL);
    CHECK(movie_decoder_open("missing.bik", 0, 0, 0) == NULL);
    CHECK(movie_decoder_open("missing.bik", -1, 0, 0) == NULL);
    CHECK(movie_decoder_open("missing.bik", 0, 320, 0) == NULL);
    MovieFrame frame;
    CHECK(movie_decoder_next(NULL, &frame, NULL, NULL) == -1);
    movie_decoder_close(NULL);
    FILE* broken = fopen("broken.bik", "wb");
    CHECK(broken != NULL);
    if (broken != NULL) {
        fwrite("BIKi", 1, 4, broken); fclose(broken);
        CHECK(movie_decoder_open("broken.bik", 0, 0, 0) == NULL);
        remove("broken.bik");
    }
    if (argc >= 2 && argv[1][0] != '\0') real_movie(argv[1], argc > 2 ? argv[2] : NULL, argc > 3 ? argv[3] : NULL);
    printf("Movie decoder: %d failures\n", failures);
    return failures != 0;
}
