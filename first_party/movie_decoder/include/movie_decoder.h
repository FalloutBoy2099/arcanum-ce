#ifndef ARCANUM_MOVIE_DECODER_H_
#define ARCANUM_MOVIE_DECODER_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct MovieDecoder MovieDecoder;

typedef struct MovieFrame {
    const uint8_t* pixels;
    int width;
    int height;
    int pitch;
    double seconds;
    double duration;
} MovieFrame;

typedef bool (*MovieAudioCallback)(void* context, const float* samples, int frames, int channels, int frequency);

MovieDecoder* movie_decoder_open(const char* path, int sound_track, int maximum_width, int maximum_height);
int movie_decoder_next(MovieDecoder* decoder, MovieFrame* frame, MovieAudioCallback audio, void* context);
void movie_decoder_close(MovieDecoder* decoder);
const char* movie_decoder_error(void);

#endif
