#include "movie_decoder.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

struct MovieDecoder {
    AVFormatContext* format;
    AVCodecContext* video;
    AVCodecContext* audio;
    AVPacket* packet;
    AVFrame* video_frame;
    AVFrame* audio_frame;
    struct SwsContext* colors;
    SwrContext* resampler;
    uint8_t* pixels;
    float* samples;
    size_t sample_capacity;
    int video_index;
    int audio_index;
    int width;
    int height;
    int pitch;
    int64_t frames;
    bool input_finished;
};

static char last_error[256];

static int fail(const char* operation, int result)
{
    char reason[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(result, reason, sizeof(reason));
    snprintf(last_error, sizeof(last_error), "%s: %s", operation, reason);
    return -1;
}

const char* movie_decoder_error(void)
{
    return last_error;
}

void movie_decoder_close(MovieDecoder* decoder)
{
    if (decoder == NULL) return;
    av_packet_free(&decoder->packet);
    av_frame_free(&decoder->video_frame);
    av_frame_free(&decoder->audio_frame);
    avcodec_free_context(&decoder->video);
    avcodec_free_context(&decoder->audio);
    avformat_close_input(&decoder->format);
    sws_freeContext(decoder->colors);
    swr_free(&decoder->resampler);
    av_free(decoder->pixels);
    av_free(decoder->samples);
    free(decoder);
}

static AVCodecContext* open_codec(AVStream* stream)
{
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == NULL) return NULL;
    AVCodecContext* context = avcodec_alloc_context3(codec);
    if (context == NULL) return NULL;
    if (avcodec_parameters_to_context(context, stream->codecpar) < 0
        || avcodec_open2(context, codec, NULL) < 0) {
        avcodec_free_context(&context);
        return NULL;
    }
    return context;
}

MovieDecoder* movie_decoder_open(const char* path, int sound_track, int maximum_width, int maximum_height)
{
    last_error[0] = '\0';
    if (path == NULL || path[0] == '\0' || sound_track < 0 || maximum_width < 0 || maximum_height < 0
        || ((maximum_width == 0) != (maximum_height == 0))) {
        fail("Movie parameters", AVERROR(EINVAL));
        return NULL;
    }
    MovieDecoder* decoder = calloc(1, sizeof(*decoder));
    if (decoder == NULL) { fail("Movie allocation", AVERROR(ENOMEM)); return NULL; }
    decoder->video_index = -1;
    decoder->audio_index = -1;
    const AVInputFormat* bink = av_find_input_format("bink");
    int result = avformat_open_input(&decoder->format, path, bink, NULL);
    if (result < 0) { fail("Opening Bink", result); goto error; }
    for (unsigned i = 0; i < decoder->format->nb_streams; i++) {
        AVStream* stream = decoder->format->streams[i];
        if (stream->codecpar->codec_id == AV_CODEC_ID_BINKVIDEO) decoder->video_index = (int)i;
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && stream->id == sound_track
            && decoder->audio_index < 0) decoder->audio_index = (int)i;
    }
    if (decoder->video_index < 0) { fail("Bink video stream", AVERROR_INVALIDDATA); goto error; }
    decoder->video = open_codec(decoder->format->streams[decoder->video_index]);
    if (decoder->video == NULL) { fail("Opening Bink video decoder", AVERROR_INVALIDDATA); goto error; }
    decoder->width = decoder->video->width;
    decoder->height = decoder->video->height;
    if (decoder->width <= 0 || decoder->height <= 0 || decoder->width > 7680 || decoder->height > 4800) {
        fail("Bink dimensions", AVERROR_INVALIDDATA); goto error;
    }
    if (maximum_width > 0 && (decoder->width > maximum_width || decoder->height > maximum_height)) {
        double scale_width = (double)maximum_width / decoder->width;
        double scale_height = (double)maximum_height / decoder->height;
        double scale = scale_width < scale_height ? scale_width : scale_height;
        decoder->width = (int)(decoder->width * scale);
        decoder->height = (int)(decoder->height * scale);
        if (decoder->width < 1) decoder->width = 1;
        if (decoder->height < 1) decoder->height = 1;
    }
    decoder->pitch = decoder->width * 4;
    decoder->pixels = av_malloc((size_t)decoder->pitch * decoder->height);
    decoder->packet = av_packet_alloc();
    decoder->video_frame = av_frame_alloc();
    decoder->audio_frame = av_frame_alloc();
    if (decoder->pixels == NULL || decoder->packet == NULL || decoder->video_frame == NULL || decoder->audio_frame == NULL) {
        fail("Bink frame allocation", AVERROR(ENOMEM)); goto error;
    }
    if (decoder->audio_index >= 0) {
        decoder->audio = open_codec(decoder->format->streams[decoder->audio_index]);
        if (decoder->audio == NULL || decoder->audio->sample_rate <= 0
            || decoder->audio->ch_layout.nb_channels < 1 || decoder->audio->ch_layout.nb_channels > 8) {
            fail("Opening Bink audio decoder", AVERROR_INVALIDDATA); goto error;
        }
        result = swr_alloc_set_opts2(&decoder->resampler, &decoder->audio->ch_layout, AV_SAMPLE_FMT_FLT, decoder->audio->sample_rate,
            &decoder->audio->ch_layout, decoder->audio->sample_fmt, decoder->audio->sample_rate, 0, NULL);
        if (result < 0 || (result = swr_init(decoder->resampler)) < 0) { fail("Bink audio conversion", result); goto error; }
    }
    return decoder;
error:
    movie_decoder_close(decoder);
    return NULL;
}

static int decode_audio(MovieDecoder* decoder, const AVPacket* packet, MovieAudioCallback callback, void* context)
{
    int result = avcodec_send_packet(decoder->audio, packet);
    if (result < 0) return fail("Bink audio packet", result);
    while ((result = avcodec_receive_frame(decoder->audio, decoder->audio_frame)) >= 0) {
        AVFrame* frame = decoder->audio_frame;
        int channels = decoder->audio->ch_layout.nb_channels;
        int available = swr_get_out_samples(decoder->resampler, frame->nb_samples);
        if (available < 0 || available > INT_MAX / channels / (int)sizeof(float)) return fail("Bink audio size", AVERROR_INVALIDDATA);
        size_t bytes = (size_t)available * channels * sizeof(float);
        if (bytes > decoder->sample_capacity) {
            float* samples = av_realloc(decoder->samples, bytes);
            if (samples == NULL) return fail("Bink audio allocation", AVERROR(ENOMEM));
            decoder->samples = samples;
            decoder->sample_capacity = bytes;
        }
        uint8_t* output = (uint8_t*)decoder->samples;
        int converted = swr_convert(decoder->resampler, &output, available, (const uint8_t**)frame->extended_data, frame->nb_samples);
        av_frame_unref(frame);
        if (converted < 0) return fail("Bink audio conversion", converted);
        if (callback != NULL && converted > 0 && !callback(context, decoder->samples, converted, channels, decoder->audio->sample_rate)) {
            return fail("Bink audio output", AVERROR_EXTERNAL);
        }
    }
    return result == AVERROR(EAGAIN) || result == AVERROR_EOF ? 0 : fail("Bink audio decoding", result);
}

int movie_decoder_next(MovieDecoder* decoder, MovieFrame* output, MovieAudioCallback callback, void* context)
{
    if (decoder == NULL || output == NULL) return fail("Bink frame parameters", AVERROR(EINVAL));
    *output = (MovieFrame){ 0 };
    for (;;) {
        int result = avcodec_receive_frame(decoder->video, decoder->video_frame);
        if (result >= 0) {
            AVFrame* frame = decoder->video_frame;
            decoder->colors = sws_getCachedContext(decoder->colors, frame->width, frame->height, frame->format,
                decoder->width, decoder->height, AV_PIX_FMT_BGRA, SWS_BILINEAR, NULL, NULL, NULL);
            if (decoder->colors == NULL) return fail("Bink color conversion", AVERROR(ENOMEM));
            uint8_t* destination[] = { decoder->pixels, NULL, NULL, NULL };
            int pitches[] = { decoder->pitch, 0, 0, 0 };
            result = sws_scale(decoder->colors, (const uint8_t* const*)frame->data, frame->linesize, 0, frame->height, destination, pitches);
            if (result != decoder->height) return fail("Bink frame conversion", AVERROR_INVALIDDATA);
            double duration = av_q2d(decoder->format->streams[decoder->video_index]->time_base);
            double seconds = frame->best_effort_timestamp == AV_NOPTS_VALUE ? decoder->frames * duration : frame->best_effort_timestamp * duration;
            *output = (MovieFrame){ decoder->pixels, decoder->width, decoder->height, decoder->pitch, seconds, duration };
            decoder->frames++;
            av_frame_unref(frame);
            return 1;
        }
        if (result == AVERROR_EOF) {
            AVStream* video = decoder->format->streams[decoder->video_index];
            if (video->duration > 0 && decoder->frames < video->duration) return fail("Truncated Bink video", AVERROR_INVALIDDATA);
            return 0;
        }
        if (result != AVERROR(EAGAIN)) return fail("Bink video decoding", result);
        if (decoder->input_finished) return fail("Incomplete Bink decoder flush", AVERROR_INVALIDDATA);
        result = av_read_frame(decoder->format, decoder->packet);
        if (result < 0) {
            if (result != AVERROR_EOF) return fail("Reading Bink packet", result);
            decoder->input_finished = true;
            if (decoder->audio != NULL && decode_audio(decoder, NULL, callback, context) < 0) return -1;
            result = avcodec_send_packet(decoder->video, NULL);
            if (result < 0) return fail("Bink video flush", result);
        } else {
            if (decoder->packet->stream_index == decoder->video_index) result = avcodec_send_packet(decoder->video, decoder->packet);
            else if (decoder->packet->stream_index == decoder->audio_index) result = decode_audio(decoder, decoder->packet, callback, context);
            else result = 0;
            av_packet_unref(decoder->packet);
            if (result < 0) return fail("Bink packet decoding", result);
        }
    }
}
