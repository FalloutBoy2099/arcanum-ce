#include "tig/scale_region.h"

#if defined(_M_IX86) || defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#define TIG_SCALE_SSE2
#endif

// Match SDL's fixed-point pixel-center convention (16 bits) and linear weights
// (7 bits). SDL's SIMD paths retain the vertical fractional part; its scalar
// path rounds it first. A small calibration chooses the installed SDL behavior,
// rather than guessing which optional SIMD paths the library was built with.
static int linear_precision = -1;
static bool use_sse2;

static void axis(Sint64 position, int size, int* first, int* second, int* fraction)
{
    if (position < 0) { *first = *second = 0; *fraction = 0; }
    else if ((position >> 16) >= size - 1) { *first = *second = size - 1; *fraction = 0; }
    else { *first = (int)(position >> 16); *second = *first + 1; *fraction = (int)((position >> 9) & 127); }
}

static Uint32 interpolate(Uint32 a, Uint32 b, Uint32 c, Uint32 d, int x, int y, int precision)
{
#ifdef TIG_SCALE_SSE2
    if (use_sse2) {
        __m128i zero = _mm_setzero_si128();
        __m128i top = _mm_unpacklo_epi8(_mm_set_epi32(0, 0, b, a), zero);
        __m128i bottom = _mm_unpacklo_epi8(_mm_set_epi32(0, 0, d, c), zero);
        __m128i vertical = _mm_add_epi16(_mm_mullo_epi16(top, _mm_set1_epi16((short)(128 - y))),
            _mm_mullo_epi16(bottom, _mm_set1_epi16((short)y)));
        if (precision == 7) vertical = _mm_srli_epi16(vertical, 7);
        __m128i pairs = _mm_unpacklo_epi16(vertical, _mm_srli_si128(vertical, 8));
        __m128i weights = _mm_set_epi16((short)x, (short)(128 - x), (short)x, (short)(128 - x),
            (short)x, (short)(128 - x), (short)x, (short)(128 - x));
        __m128i channels = _mm_madd_epi16(pairs, weights);
        channels = precision == 7 ? _mm_srli_epi32(channels, 7) : _mm_srli_epi32(channels, 14);
        __m128i shorts = _mm_packs_epi32(channels, zero);
        return (Uint32)_mm_cvtsi128_si32(_mm_packus_epi16(shorts, zero));
    }
#endif
    Uint32 pixel = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        unsigned int left = ((a >> shift) & 255) * (128 - y) + ((c >> shift) & 255) * y;
        unsigned int right = ((b >> shift) & 255) * (128 - y) + ((d >> shift) & 255) * y;
        if (precision == 7) { left >>= 7; right >>= 7; }
        unsigned int value = (left * (128 - x) + right * x) >> precision;
        pixel |= value << shift;
    }
    return pixel;
}

static void scale(SDL_Surface* src, SDL_Rect source, SDL_Surface* dst,
    SDL_Rect destination, SDL_Rect region, SDL_ScaleMode mode, int precision)
{
    Uint64 step_x = ((Uint64)source.w << 16) / destination.w;
    Uint64 step_y = ((Uint64)source.h << 16) / destination.h;
    Sint64 start_x = mode == SDL_SCALEMODE_LINEAR ? (Sint64)((step_x + 1) / 2) - 32768 : step_x / 2;
    Sint64 start_y = mode == SDL_SCALEMODE_LINEAR ? (Sint64)((step_y + 1) / 2) - 32768 : step_y / 2;
    for (int y = region.y; y < region.y + region.h; y++) {
        Sint64 sy = start_y + (Sint64)(y - destination.y) * step_y;
        Uint32* target = (Uint32*)((Uint8*)dst->pixels + (size_t)y * dst->pitch) + region.x;
        Sint64 sx = start_x + (Sint64)(region.x - destination.x) * step_x;
        if (mode == SDL_SCALEMODE_NEAREST) {
            const Uint32* row = (const Uint32*)((const Uint8*)src->pixels + (size_t)(source.y + (sy >> 16)) * src->pitch);
            for (int x = 0; x < region.w; x++, sx += step_x) target[x] = row[source.x + (sx >> 16)];
        } else {
            int y0, y1, fy;
            axis(sy, source.h, &y0, &y1, &fy);
            const Uint32* row0 = (const Uint32*)((const Uint8*)src->pixels + (size_t)(source.y + y0) * src->pitch) + source.x;
            const Uint32* row1 = (const Uint32*)((const Uint8*)src->pixels + (size_t)(source.y + y1) * src->pitch) + source.x;
            for (int x = 0; x < region.w; x++, sx += step_x) {
                int x0, x1, fx;
                axis(sx, source.w, &x0, &x1, &fx);
                target[x] = interpolate(row0[x0], row0[x1], row1[x0], row1[x1], fx, fy, precision);
            }
        }
    }
}

static int calibrate(void)
{
#ifdef TIG_SCALE_SSE2
    use_sse2 = SDL_HasSSE2();
#endif
    SDL_Surface* src = SDL_CreateSurface(3, 3, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface* reference = SDL_CreateSurface(5, 5, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface* candidate = SDL_CreateSurface(5, 5, SDL_PIXELFORMAT_XRGB8888);
    int result = 0;
    if (src != NULL && reference != NULL && candidate != NULL) {
        for (int y = 0; y < 3; y++) for (int x = 0; x < 3; x++) {
            *(Uint32*)((Uint8*)src->pixels + y * src->pitch + x * 4) = (Uint32)(x * 157 + y * 91) * 0x103091U;
        }
        SDL_Rect source = { 0, 0, 3, 3 }, destination = { 0, 0, 5, 5 };
        if (SDL_BlitSurfaceScaled(src, &source, reference, &destination, SDL_SCALEMODE_LINEAR)) {
            const int precisions[] = { 14, 7 };
            for (int i = 0; i < 2 && result == 0; i++) {
                scale(src, source, candidate, destination, destination, SDL_SCALEMODE_LINEAR, precisions[i]);
                bool same = true;
                for (int y = 0; y < 5; y++) {
                    if (SDL_memcmp((Uint8*)reference->pixels + y * reference->pitch,
                        (Uint8*)candidate->pixels + y * candidate->pitch, 5 * 4) != 0) same = false;
                }
                if (same) result = precisions[i];
            }
        }
    }
    SDL_DestroySurface(candidate);
    SDL_DestroySurface(reference);
    SDL_DestroySurface(src);
    return result;
}

bool tig_scale_region(SDL_Surface* src, SDL_Rect source, SDL_Surface* dst,
    SDL_Rect destination, SDL_Rect region, SDL_ScaleMode mode)
{
    if (src == NULL || dst == NULL || src == dst || src->format != SDL_PIXELFORMAT_XRGB8888
        || dst->format != src->format || source.w < 2 || source.h < 2 || source.w > 65535 || source.h > 65535
        || destination.w <= 0 || destination.h <= 0 || source.x < 0 || source.y < 0
        || (Sint64)source.x + source.w > src->w || (Sint64)source.y + source.h > src->h
        || destination.x < 0 || destination.y < 0 || (Sint64)destination.x + destination.w > dst->w
        || (Sint64)destination.y + destination.h > dst->h
        || (mode != SDL_SCALEMODE_LINEAR && mode != SDL_SCALEMODE_NEAREST)) return false;
    SDL_BlendMode blend;
    Uint8 red, green, blue, alpha;
    if (!SDL_GetSurfaceBlendMode(src, &blend) || blend != SDL_BLENDMODE_NONE || SDL_SurfaceHasColorKey(src)
        || !SDL_GetSurfaceColorMod(src, &red, &green, &blue) || red != 255 || green != 255 || blue != 255
        || !SDL_GetSurfaceAlphaMod(src, &alpha) || alpha != 255) return false;
    if (mode == SDL_SCALEMODE_LINEAR && (((Uint64)source.w << 16) / destination.w > 2147483647
        || ((Uint64)source.h << 16) / destination.h > 2147483647)) return false;
    SDL_Rect clip;
    SDL_GetSurfaceClipRect(dst, &clip);
    if (!SDL_GetRectIntersection(&region, &clip, &region)
        || !SDL_GetRectIntersection(&region, &destination, &region)) return true;
    if (mode == SDL_SCALEMODE_LINEAR) {
        if (linear_precision == -1) linear_precision = calibrate();
        if (linear_precision == 0) return false;
    }
    if (!SDL_LockSurface(src)) return false;
    if (!SDL_LockSurface(dst)) { SDL_UnlockSurface(src); return false; }
    scale(src, source, dst, destination, region, mode, linear_precision);
    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
    return true;
}
