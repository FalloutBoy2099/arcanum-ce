#include "tig/bmp.h"

#include "tig/color.h"
#include "tig/file.h"
#include "tig/memory.h"
#include "tig/pixel_buffer.h"
#include "tig/video.h"
#include "tig/window.h"

// 0x535D80
int tig_bmp_create(TigBmp* bmp)
{
    SDL_IOStream* io;
    SDL_Surface* surface;
    const SDL_PixelFormatDetails* fmt;
    SDL_Palette* palette;
    size_t data_size;
    int index;

    if (bmp == NULL) {
        return TIG_ERR_INVALID_PARAM;
    }

    bmp->pixels = NULL;

    io = tig_file_io_open(bmp->name, "rb");
    if (io == NULL) {
        return TIG_ERR_IO;
    }

    surface = SDL_LoadBMP_IO(io, true);
    if (surface == NULL) {
        return TIG_ERR_GENERIC;
    }

    fmt = SDL_GetPixelFormatDetails(surface->format);
    if (fmt == NULL) {
        SDL_DestroySurface(surface);
        return TIG_ERR_GENERIC;
    }

    if (fmt->bits_per_pixel != 4
        && fmt->bits_per_pixel != 8
        && fmt->bits_per_pixel != 24) {
        SDL_DestroySurface(surface);
        return TIG_ERR_GENERIC;
    }

    bmp->bpp = fmt->bits_per_pixel;

    palette = SDL_GetSurfacePalette(surface);
    if (palette != NULL) {
        for (index = 0; index < palette->ncolors; index++) {
            bmp->palette[index] = tig_color_make(palette->colors[index].r,
                palette->colors[index].g,
                palette->colors[index].b);
        }
    }

    bmp->pitch = surface->pitch;
    bmp->width = surface->w;
    bmp->height = surface->h;

    if (!tig_pixel_buffer_size(bmp->width, bmp->height, bmp->bpp, bmp->pitch, &data_size)) {
        SDL_DestroySurface(surface);
        return TIG_ERR_INVALID_PARAM;
    }
    bmp->pixels = MALLOC(data_size);
    if (bmp->pixels == NULL) {
        SDL_DestroySurface(surface);
        return TIG_ERR_OUT_OF_MEMORY;
    }

    memcpy(bmp->pixels, surface->pixels, data_size);

    SDL_DestroySurface(surface);

    return TIG_OK;
}

// 0x5360D0
int tig_bmp_destroy(TigBmp* bmp)
{
    if (bmp == NULL) return TIG_ERR_INVALID_PARAM;
    if (bmp->pixels != NULL) {
        FREE(bmp->pixels);
        bmp->pixels = NULL;
    }

    return TIG_OK;
}

int tig_bmp_save_surface(SDL_Surface* surface, const TigRect* rect, const char* file_name)
{
    SDL_Surface* intermediate_surface = surface;
    SDL_IOStream* io;
    int rc;
    size_t surface_bytes;
    if (surface == NULL || surface->pixels == NULL || rect == NULL || file_name == NULL
        || file_name[0] == '\0' || SDL_BYTESPERPIXEL(surface->format) == 0
        || SDL_ISPIXELFORMAT_FOURCC(surface->format) || rect->x < 0 || rect->y < 0
        || rect->width <= 0 || rect->height <= 0
        || rect->width > surface->w || rect->height > surface->h
        || rect->x > surface->w - rect->width || rect->y > surface->h - rect->height
        || !tig_pixel_buffer_size(surface->w, surface->h, SDL_BYTESPERPIXEL(surface->format) * 8,
            surface->pitch, &surface_bytes)) return TIG_ERR_INVALID_PARAM;

    if (rect->x != 0 || rect->y != 0 || rect->width != surface->w || rect->height != surface->h) {
        intermediate_surface = SDL_CreateSurface(rect->width, rect->height, surface->format);
        if (intermediate_surface == NULL) return TIG_ERR_GENERIC;
        SDL_Palette* palette = SDL_GetSurfacePalette(surface);
        if ((palette != NULL && !SDL_SetSurfacePalette(intermediate_surface, palette))
            || !SDL_LockSurface(surface)) {
            SDL_DestroySurface(intermediate_surface);
            return TIG_ERR_GENERIC;
        }
        for (int y = 0; y < rect->height; y++) {
            const Uint8* source = (const Uint8*)surface->pixels + (size_t)(rect->y + y) * surface->pitch;
            Uint8* destination = (Uint8*)intermediate_surface->pixels + (size_t)y * intermediate_surface->pitch;
            memcpy(destination, source + (size_t)rect->x * SDL_BYTESPERPIXEL(surface->format),
                (size_t)rect->width * SDL_BYTESPERPIXEL(surface->format));
        }
        SDL_UnlockSurface(surface);
    }

    io = tig_file_io_open(file_name, "wb");
    if (io == NULL) {
        if (intermediate_surface != surface) SDL_DestroySurface(intermediate_surface);
        return TIG_ERR_IO;
    }

    rc = TIG_OK;
    if (!SDL_SaveBMP_IO(intermediate_surface, io, false)) {
        rc = TIG_ERR_IO;
    }
    if (!SDL_CloseIO(io)) rc = TIG_ERR_IO;

    if (intermediate_surface != surface) {
        SDL_DestroySurface(intermediate_surface);
    }

    return rc;
}
