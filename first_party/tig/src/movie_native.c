#include "tig/movie.h"

#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <movie_decoder.h>

#include "tig/core.h"
#include "tig/debug.h"
#include "tig/kb.h"
#include "tig/message.h"
#include "tig/video.h"
#include "tig/window.h"

static bool movie_playing;
static bool movie_sound_enabled;
static int movie_screen_width;
static int movie_screen_height;

typedef struct MoviePlayback {
    SDL_AudioStream* audio;
    bool audio_unavailable;
    bool quit;
    int skip_key;
    bool ignore_keyboard;
    uint64_t queued_audio_frames;
} MoviePlayback;

int tig_movie_init(TigInitInfo* init_info)
{
    if (init_info == NULL || init_info->width <= 0 || init_info->height <= 0) return TIG_ERR_INVALID_PARAM;
    movie_screen_width = init_info->width;
    movie_screen_height = init_info->height;
    movie_sound_enabled = (init_info->flags & TIG_INITIALIZE_NO_SOUND) == 0;
    movie_playing = false;
    return TIG_OK;
}

void tig_movie_exit(void)
{
    movie_playing = false;
}

bool tig_movie_is_playing(void)
{
    return movie_playing;
}

static bool output_audio(void* context, const float* samples, int frames, int channels, int frequency)
{
    MoviePlayback* playback = context;
    if (!movie_sound_enabled || playback->audio_unavailable) return true;
    if (frames <= 0 || channels <= 0 || frames > INT_MAX / channels / (int)sizeof(float)) return false;
    if (playback->audio == NULL) {
        SDL_AudioSpec spec = { SDL_AUDIO_F32, channels, frequency };
        playback->audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (playback->audio == NULL) {
            tig_debug_printf("Movie audio unavailable: %s\n", SDL_GetError());
            playback->audio_unavailable = true;
            return true;
        }
    }
    if (!SDL_PutAudioStreamData(playback->audio, samples, frames * channels * (int)sizeof(float))) return false;
    playback->queued_audio_frames += frames;
    return true;
}

static bool pump_messages(MoviePlayback* playback)
{
    tig_ping();
    TigMessage message;
    while (tig_message_dequeue(&message) == TIG_OK) {
        if (message.type == TIG_MESSAGE_QUIT) {
            tig_message_enqueue(&message);
            playback->quit = true;
            return false;
        }
        if (!playback->ignore_keyboard && message.type == TIG_MESSAGE_KEYBOARD && message.data.keyboard.pressed) {
            playback->skip_key = message.data.keyboard.key;
            return false;
        }
    }
    return true;
}

static bool wait_until(MoviePlayback* playback, Uint64 start, double seconds)
{
    if (!isfinite(seconds) || seconds < 0 || seconds > 86400) return false;
    for (;;) {
        if (!pump_messages(playback)) return false;
        double elapsed = (double)(SDL_GetTicksNS() - start) / 1000000000.0;
        if (elapsed >= seconds) return true;
        SDL_Delay(1);
    }
}

int tig_movie_play(const char* path, TigMovieFlags flags, int sound_track)
{
    if (path == NULL || path[0] == '\0' || sound_track < 0) return TIG_ERR_INVALID_PARAM;
    if (movie_playing) return TIG_ERR_ALREADY_INITIALIZED;
    MovieDecoder* decoder = movie_decoder_open(path, sound_track, movie_screen_width, movie_screen_height);
    if (decoder == NULL) {
        tig_debug_printf("Movie open failed: %s (%s)\n", path, movie_decoder_error());
        tig_video_fade(0, 0, 0.0f, 1);
        return TIG_ERR_IO;
    }
    MoviePlayback playback = { .skip_key = -1, .ignore_keyboard = (flags & TIG_MOVIE_IGNORE_KEYBOARD) != 0 };
    TigVideoBuffer* buffer = NULL;
    movie_playing = true;
    int result = TIG_OK;
    if ((flags & TIG_MOVIE_FADE_IN) != 0) tig_video_fade(0, 64, 3.0f, 0);
    tig_video_fill(NULL, 0);
    tig_video_flip();
    tig_video_fade(0, 0, 0.0f, 1);
    Uint64 start = 0;
    int decoded = 0;
    int presented_frames = 0;
    MovieFrame frame;
    double end_seconds = 0;
    bool ignore_keyboard = playback.ignore_keyboard;
    playback.ignore_keyboard = true;
    pump_messages(&playback);
    playback.ignore_keyboard = ignore_keyboard;
    while (!playback.quit && (decoded = movie_decoder_next(decoder, &frame, output_audio, &playback)) > 0) {
        if (buffer == NULL) {
            TigVideoBufferCreateInfo info = { 0 };
            info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
            info.width = frame.width;
            info.height = frame.height;
            if (tig_video_buffer_create(&info, &buffer) != TIG_OK) { result = TIG_ERR_OUT_OF_MEMORY; break; }
            start = SDL_GetTicksNS();
            if (playback.audio != NULL) SDL_ResumeAudioStreamDevice(playback.audio);
        }
        if (!wait_until(&playback, start, frame.seconds)) break;
        if (playback.audio != NULL) SDL_ResumeAudioStreamDevice(playback.audio);
        result = tig_video_buffer_lock(buffer);
        if (result != TIG_OK) break;
        TigVideoBufferData pixels;
        result = tig_video_buffer_data(buffer, &pixels);
        if (result == TIG_OK && !SDL_ConvertPixels(frame.width, frame.height, SDL_PIXELFORMAT_BGRA32,
                frame.pixels, frame.pitch, SDL_PIXELFORMAT_XRGB8888, pixels.pixels, pixels.pitch)) result = TIG_ERR_BLIT;
        tig_video_buffer_unlock(buffer);
        if (result != TIG_OK) break;
        TigRect source = { 0, 0, frame.width, frame.height };
        TigRect destination = { (movie_screen_width - frame.width) / 2, (movie_screen_height - frame.height) / 2, frame.width, frame.height };
        result = tig_video_blit(buffer, &source, &destination);
        if (result != TIG_OK || (result = tig_video_flip()) != TIG_OK) break;
        presented_frames++;
        end_seconds = frame.seconds + frame.duration;
    }
    if (decoded < 0) {
        tig_debug_printf("Movie decoding failed: %s (%s)\n", path, movie_decoder_error());
        result = TIG_ERR_IO;
    }
    if (decoded == 0 && result == TIG_OK && start != 0) {
        if (playback.audio != NULL) SDL_FlushAudioStream(playback.audio);
        wait_until(&playback, start, end_seconds);
    }
    tig_debug_printf("Movie: frames=%d queued_audio_frames=%" PRIu64 " audio=%d skipped=%d quit=%d result=%d.\n", presented_frames, playback.queued_audio_frames, playback.audio != NULL, playback.skip_key >= 0, playback.quit, result);
    if (playback.audio != NULL) SDL_DestroyAudioStream(playback.audio);
    if (buffer != NULL) tig_video_buffer_destroy(buffer);
    movie_decoder_close(decoder);
    movie_playing = false;
    if (!playback.quit && playback.skip_key >= 0) {
        while (tig_kb_is_key_pressed(playback.skip_key)) {
            if (!pump_messages(&playback)) break;
            SDL_Delay(1);
        }
    }
    tig_window_invalidate_rect(NULL);
    if (!playback.quit) {
        if ((flags & TIG_MOVIE_FADE_OUT) != 0) tig_video_fade(0, 0, 0.0f, 0);
        if ((flags & TIG_MOVIE_NO_FINAL_FLIP) == 0) { tig_video_fill(NULL, 0); tig_video_flip(); }
    }
    return result;
}
