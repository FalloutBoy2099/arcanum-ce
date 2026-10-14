#include "tig/font.h"

#include "tig/art.h"
#include "tig/color.h"
#include "tig/memory.h"
#include "tig/rect.h"
#include "tig/video.h"

#include <SDL3_ttf/SDL_ttf.h>

#define FONT_STACK_SIZE 20

static int sub_535850(TigVideoBuffer* video_buffer, const char* str, int length, TigArtBlitInfo* blit_info, bool shadow, bool* in_bracket);
static int sub_535C40(tig_art_id_t font_art_id, const char* str, int max_width, int* width_ptr);
static bool tig_font_glyph_data(tig_art_id_t font_art_id, int ch, int* width_ptr, int* height_ptr, int* dx_ptr, int* dy_ptr);
static bool tig_font_modern_open(TigFont* font);
static void tig_font_modern_close(TigFont* font);
static bool tig_font_modern_measure(TigFont* font);
static int tig_font_modern_write(TigVideoBuffer* video_buffer, const char* str, const TigRect* rect, TigRect* dirty_rect);
static SDL_Color tig_font_modern_color(tig_color_t color);
static SDL_Color tig_font_modern_shadow_color(TigFont* font);
static SDL_Surface* tig_font_modern_render(TigFont* font, const char* str, int width, tig_color_t color);
static void tig_font_modern_dim_brackets(TigFont* font, const char* str, int width, SDL_Surface* surface);
static SDL_Surface* tig_font_modern_render_color(TigFont* font, const char* str, int width, SDL_Color color);
static char* tig_font_modern_to_utf8(const char* str);
static int tig_font_modern_cp1252_to_codepoint(unsigned char ch);
static char* tig_font_modern_utf8_append(char* p, int codepoint);

// 0x630CFC
static int tig_font_stack_index;

// 0x630D00
static TigFont* tig_font_stack[FONT_STACK_SIZE];

// 0x630D50
static tig_color_t tig_font_shadow_color;

// 0x630D54
static tig_font_handle_t tig_font_default_font;

// 0x5351D0
int tig_font_init(TigInitInfo* init_info)
{
    TigFont font_data;

    (void)init_info;

    tig_font_stack_index = -1;
    tig_font_shadow_color = tig_color_make(10, 10, 10);

    font_data.flags = TIG_FONT_SHADOW;
    tig_art_misc_id_create(TIG_ART_SYSTEM_FONT, 0, &(font_data.art_id));
    font_data.scale = 1.5f;
    font_data.str = NULL;
    font_data.color = tig_color_make(255, 255, 255);
    font_data.underline_color = font_data.color;
    font_data.strike_through_color = font_data.color;
    tig_font_create(&font_data, &tig_font_default_font);
    tig_font_push(tig_font_default_font);

    return TIG_OK;
}

// 0x5352C0
void tig_font_exit(void)
{
    tig_font_pop();
    tig_font_destroy(tig_font_default_font);
}

// 0x5352E0
void tig_font_create(TigFont* font_data, tig_font_handle_t* font_handle_ptr)
{
    TigFont* copy;
    TigArtAnimData art_anim_data;

    copy = (TigFont*)MALLOC(sizeof(TigFont));
    memcpy(copy, font_data, sizeof(TigFont));

    copy->modern_font = NULL;
    if ((copy->flags & TIG_FONT_DIM_BRACKETS) != 0
        && (copy->bracket_dim_percent < 0 || copy->bracket_dim_percent > 100)) {
        copy->bracket_dim_percent = 60;
    }
    if (copy->modern_shadow_opacity < 0 || copy->modern_shadow_opacity > 255) {
        copy->modern_shadow_opacity = 255;
    }
    if (copy->modern_shadow_distance < 0) {
        copy->modern_shadow_distance = 0;
    }
    if ((copy->flags & TIG_FONT_MODERN) != 0) {
        if (!tig_font_modern_open(copy)) {
            copy->flags &= ~TIG_FONT_MODERN;
        }
    }

    if ((copy->flags & TIG_FONT_MODERN) == 0 && tig_art_anim_data(font_data->art_id, &art_anim_data) != TIG_OK) {
        // Fatal error - missing font art.
        exit(EXIT_FAILURE);
    }

    *font_handle_ptr = (tig_font_handle_t)copy;
}

// 0x535330
void tig_font_destroy(tig_font_handle_t font_handle)
{
    TigFont* font = (TigFont*)font_handle;

    tig_font_modern_close(font);
    FREE(font);
}

// 0x535340
int tig_font_push(tig_font_handle_t font_handle)
{
    TigFont* font = (TigFont*)font_handle;

    tig_font_stack_index++;
    if (tig_font_stack_index < FONT_STACK_SIZE) {
        tig_font_stack[tig_font_stack_index] = font;
    } else {
        // Font #0 is the default and therefore should be skipped.
        tig_font_stack_index = 1;
        tig_font_stack[tig_font_stack_index] = font;
    }

    return tig_font_stack_index;
}

// 0x535380
void tig_font_pop(void)
{
    if (tig_font_stack_index > 0) {
        tig_font_stack_index--;
    }
}

// 0x535390
void tig_font_measure(TigFont* font)
{
    const char* str;
    int width;
    int height;
    int glyph_width;
    int glyph_height;
    int glyph_dx;
    int glyph_dy;
    int pos;
    int lines;
    int line_length;
    int line_width;

    str = font->str;
    width = font->width;
    height = font->height;
    memcpy(font, tig_font_stack[tig_font_stack_index], sizeof(TigFont));
    font->str = str;
    font->width = width;
    font->height = height;

    if ((font->flags & TIG_FONT_MODERN) != 0 && font->modern_font != NULL) {
        tig_font_modern_measure(font);
        return;
    }

    width = 0;
    height = 0;

    if (str != NULL) {
        glyph_height = 0;
        lines = 1;
        if (font->width != 0) {
            if (!tig_font_glyph_data(tig_font_stack[tig_font_stack_index]->art_id, str[0], &glyph_width, &glyph_height, &glyph_dx, &glyph_dy)) {
                font->height = 0;
                return;
            }

            pos = 0;
            for (;;) {
                line_length = sub_535C40(tig_font_stack[tig_font_stack_index]->art_id, &(str[pos]), font->width, &line_width);
                if (line_length == -1) {
                    font->height = 0;
                    return;
                }

                pos += line_length;
                if (str[pos] == '\0') {
                    break;
                }

                lines++;
                pos++;
            }
        } else {
            pos = 0;
            while (str[pos] != '\0') {
                if (!tig_font_glyph_data(tig_font_stack[tig_font_stack_index]->art_id, str[pos], &glyph_width, &glyph_height, &glyph_dx, &glyph_dy)) {
                    font->height = 0;
                    return;
                }

                if (str[pos + 1] != '\0') {
                    width += glyph_dx;
                } else {
                    width += glyph_width;
                }

                pos++;
            }
        }

        height = glyph_height * lines;

        if ((font->flags & TIG_FONT_SHADOW) != 0) {
            ++height;
            ++width;
        }

        if ((font->flags & TIG_FONT_SCALE) != 0) {
            width = (int)((float)width * font->scale);
            height = (int)((float)height * font->scale);
        }
    }

    if (font->width == 0 || str == NULL) {
        font->width = width;
    }

    font->height = height;
}

// 0x535570
int tig_font_write(TigVideoBuffer* video_buffer, const char* str, const TigRect* rect, TigRect* dirty_rect)
{
    TigArtBlitInfo blit_info;
    TigRect dst_rect;
    TigRect src_rect;
    int glyph_dx;
    int glyph_dy;
    int glyph_height;
    int glyph_width;
    int max_width;
    int max_y;
    int min_dx;
    int num_passes;
    int pass;
    bool shadow;
    int dst_rect_x;
    const char* remainder;

    if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_MODERN) != 0
        && tig_font_stack[tig_font_stack_index]->modern_font != NULL) {
        return tig_font_modern_write(video_buffer, str, rect, dirty_rect);
    }

    // NOTE: Initialize to keep compiler happy.
    max_y = 0;

    if (str == NULL) {
        return TIG_OK;
    }

    if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_SHADOW) != 0) {
        num_passes = 2;
        shadow = true;
    } else {
        num_passes = 1;
        shadow = false;
    }

    blit_info.dst_rect = &dst_rect;
    blit_info.src_rect = &src_rect;
    blit_info.dst_video_buffer = video_buffer;

    src_rect.x = 0;
    src_rect.y = 0;

    min_dx = rect->width;
    max_width = 0;

    for (pass = 0; pass < num_passes; pass++) {
        blit_info.flags = 0;

        if (!shadow) {
            blit_info.flags |= TIG_ART_BLT_BLEND_COLOR_CONST;

            if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_BLEND_ADD) != 0) {
                blit_info.flags |= TIG_ART_BLT_BLEND_ADD;
            }

            if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_NO_ALPHA_BLEND) == 0) {
                blit_info.flags |= TIG_ART_BLT_BLEND_ALPHA_SRC;
            }

            blit_info.color = tig_font_stack[tig_font_stack_index]->color;
        }

        dst_rect.x = rect->x;
        dst_rect.y = rect->y;

        if (shadow) {
            blit_info.flags |= TIG_ART_BLT_BLEND_COLOR_CONST;
            ++dst_rect.x;
            ++dst_rect.y;
            blit_info.color = tig_font_shadow_color;
        }

        remainder = str;

        if (!tig_font_glyph_data(tig_font_stack[tig_font_stack_index]->art_id, ' ', &glyph_width, &glyph_height, &glyph_dx, &glyph_dy)) {
            return TIG_ERR_GENERIC;
        }

        max_y = rect->y;
        dst_rect_x = dst_rect.x;

        if (glyph_height <= rect->height) {
            int line_length;
            int line_width;
            int dx;
            int rc;
            float sc;
            int wrap_width;
            int line_advance;
            bool in_bracket = false;

            if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_SCALE) != 0) {
                sc = tig_font_stack[tig_font_stack_index]->scale;
            } else {
                sc = 1.0f;
            }

            // Wrapping and line advance are measured in unscaled glyph metrics,
            // so translate the destination rect into unscaled space for the
            // wrap width and scale the resulting line height back up.
            wrap_width = (int)((float)rect->width / sc);
            line_advance = (int)((float)glyph_height * sc);

            while (1) {
                line_length = sub_535C40(tig_font_stack[tig_font_stack_index]->art_id, remainder, wrap_width, &line_width);
                if (line_length == -1) {
                    break;
                }

                if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_CENTERED) != 0) {
                    dx = (rect->width - (int)((float)line_width * sc)) / 2;
                } else if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_RIGHT) != 0) {
                    dx = rect->width - (int)((float)line_width * sc);
                } else {
                    dx = 0;
                }

                if (dx < min_dx) {
                    min_dx = dx;
                }

                if ((int)((float)line_width * sc) > max_width) {
                    max_width = (int)((float)line_width * sc);
                }

                dst_rect.x += dx;

                if (dx != 0 && !shadow) {
                    dst_rect.x = dst_rect_x + dx;
                }

                rc = sub_535850(video_buffer, remainder, line_length + 1, &blit_info, shadow, &in_bracket);
                if (rc != TIG_OK) {
                    return rc;
                }

                dst_rect.y += line_advance;
                dst_rect.x = dst_rect_x;

                max_y += line_advance;

                if (remainder[line_length] == '\0') {
                    break;
                }

                remainder += line_length + 1;
                if (rect->y + rect->height - max_y < line_advance) {
                    break;
                }
            }
        }

        shadow = false;
    }

    if (dirty_rect != NULL) {
        dirty_rect->x = rect->x + min_dx;
        dirty_rect->y = rect->y;
        dirty_rect->width = max_width;
        dirty_rect->height = max_y - rect->y + 1;

        if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_SHADOW) != 0) {
            ++dirty_rect->width;
            ++dirty_rect->height;
        }
    }

    return TIG_OK;
}

// 0x535850
int sub_535850(TigVideoBuffer* video_buffer, const char* str, int length, TigArtBlitInfo* blit_info, bool shadow, bool* in_bracket)
{
    tig_art_id_t glyph_art_id;
    int blt_dst_rect_x;
    int blt_dst_rect_y;
    int glyph_dx;
    int glyph_dy;
    int glyph_height;
    int glyph_width;
    int pos;
    int rc;

    // NOTE: Initialize to make compiler happy.
    glyph_dy = 0;
    rc = TIG_OK;

    blt_dst_rect_x = blit_info->dst_rect->x;
    blt_dst_rect_y = blit_info->dst_rect->y;
    TigFont* font = tig_font_stack[tig_font_stack_index];
    bool dim = !shadow && (font->flags & TIG_FONT_DIM_BRACKETS) != 0;
    tig_color_t dim_color = dim ? tig_color_blend_alpha(font->color, tig_color_make(0, 0, 0),
        font->bracket_dim_percent * 256 / 100) : font->color;

    for (pos = 0; pos < length; pos++) {
        if (str[pos] == '\0') {
            break;
        }

        if (str[pos] != '\n') {
            glyph_art_id = tig_art_id_frame_set(tig_font_stack[tig_font_stack_index]->art_id,
                (unsigned char)str[pos] - 31);

            if (!tig_font_glyph_data(glyph_art_id, str[pos], &glyph_width, &glyph_height, &glyph_dx, &glyph_dy)) {
                return TIG_ERR_GENERIC;
            }

            if (dim) {
                bool dim_glyph = *in_bracket || str[pos] == '[' || str[pos] == ']';
                blit_info->color = dim_glyph ? dim_color : font->color;
                if (str[pos] == '[') *in_bracket = true;
                if (str[pos] == ']') *in_bracket = false;
            }

            if (str[pos] != '\t') {
                blit_info->art_id = glyph_art_id;
                blit_info->src_rect->width = glyph_width;
                blit_info->src_rect->height = glyph_height;
                blit_info->dst_rect->width = glyph_width;
                blit_info->dst_rect->height = glyph_height;

                if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_SCALE) != 0) {
                    blit_info->dst_rect->width = (int)((float)blit_info->dst_rect->width * tig_font_stack[tig_font_stack_index]->scale);
                    blit_info->dst_rect->height = (int)((float)blit_info->dst_rect->height * tig_font_stack[tig_font_stack_index]->scale);
                }

                rc = tig_art_blit(blit_info);
                if (rc != TIG_OK) {
                    return rc;
                }
            }

            if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_SCALE) != 0) {
                blit_info->dst_rect->x += (int)((float)glyph_dx * tig_font_stack[tig_font_stack_index]->scale);
            } else {
                blit_info->dst_rect->x += glyph_dx;
            }
        }
    }

    if (*str != '\0'
        && (tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_UNDERLINE) != 0) {
        int y = blit_info->dst_rect->y;

        blit_info->dst_rect->width = blit_info->dst_rect->x - blt_dst_rect_x;
        blit_info->dst_rect->height = 1;
        blit_info->dst_rect->x = blt_dst_rect_x;
        blit_info->dst_rect->y = blt_dst_rect_y;

        if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_SCALE) != 0) {
            blit_info->dst_rect->y += (int)((float)glyph_dy * tig_font_stack[tig_font_stack_index]->scale);
        } else {
            blit_info->dst_rect->y += glyph_dy;
        }

        if (shadow) {
            ++blit_info->dst_rect->x;
            ++blit_info->dst_rect->y;
            --blit_info->dst_rect->width;
            tig_video_buffer_fill(video_buffer,
                blit_info->dst_rect,
                tig_color_make(10, 10, 10));
            --blit_info->dst_rect->x;
            --blit_info->dst_rect->y;
            ++blit_info->dst_rect->width;
        }

        rc = tig_video_buffer_fill(video_buffer,
            blit_info->dst_rect,
            tig_font_stack[tig_font_stack_index]->underline_color);

        blit_info->dst_rect->y = y;
    }

    if (*str != '\0'
        && (tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_STRIKE_THROUGH) != 0) {
        int y = blit_info->dst_rect->y;

        blit_info->dst_rect->width = blit_info->dst_rect->x - blt_dst_rect_x;
        blit_info->dst_rect->height = 1;
        blit_info->dst_rect->x = blt_dst_rect_x;
        blit_info->dst_rect->y = blt_dst_rect_y;

        if ((tig_font_stack[tig_font_stack_index]->flags & TIG_FONT_SCALE) != 0) {
            blit_info->dst_rect->y += (int)((float)glyph_dy * tig_font_stack[tig_font_stack_index]->scale / 2.0f);
        } else {
            blit_info->dst_rect->y += glyph_dy / 2;
        }

        if (shadow) {
            ++blit_info->dst_rect->x;
            ++blit_info->dst_rect->y;
            --blit_info->dst_rect->width;
            rc = tig_video_buffer_fill(video_buffer,
                blit_info->dst_rect,
                tig_color_make(10, 10, 10));
            --blit_info->dst_rect->x;
            --blit_info->dst_rect->y;
            ++blit_info->dst_rect->width;
        }

        tig_video_buffer_fill(video_buffer,
            blit_info->dst_rect,
            tig_font_stack[tig_font_stack_index]->strike_through_color);

        blit_info->dst_rect->y = y;
    }

    return rc;
}

// 0x535C40
int sub_535C40(tig_art_id_t font_art_id, const char* str, int max_width, int* width_ptr)
{
    int glyph_width;
    int glyph_height;
    int glyph_dx;
    int glyph_dy;
    int pos;
    int prev;
    int width;
    int total_width;

    total_width = 0;
    prev = -1;
    pos = 0;
    while (str[pos] != '\0') {
        if (str[pos] == '\n') {
            // Found NL, that's what we were looking for.
            prev = pos;
            *width_ptr = total_width;
            break;
        }

        if (!tig_font_glyph_data(font_art_id, (unsigned char)str[pos], &glyph_width, &glyph_height, &glyph_dx, &glyph_dy)) {
            return -1;
        }

        if (str[pos] == ' ') {
            // Found SPACE, save it as a candidate to be a line break.
            prev = pos;
            *width_ptr = total_width;
        }

        width = str[pos + 1] != '\0' ? glyph_dx : glyph_width;

        if (total_width + width > max_width) {
            break;
        }

        total_width += glyph_dx;

        if (str[pos] == '-') {
            // Found HYPHEN, save it a candidate to be a line break.
            prev = pos;
            *width_ptr = total_width;
        }

        pos++;
    }

    if (str[pos] != '\0') {
        if (prev != pos) {
            pos = prev;
        }
    } else {
        *width_ptr = total_width;
    }

    return pos;
}

// 0x535D10
bool tig_font_glyph_data(tig_art_id_t font_art_id, int ch, int* width_ptr, int* height_ptr, int* dx_ptr, int* dy_ptr)
{
    TigArtFrameData glyph_frame_data;
    tig_art_id_t glyph_art_id;

    glyph_art_id = tig_art_id_frame_set(font_art_id, (unsigned char)ch - 31);
    if (tig_art_frame_data(glyph_art_id, &glyph_frame_data) != TIG_OK) {
        return false;
    }

    *width_ptr = glyph_frame_data.width;
    *height_ptr = glyph_frame_data.height;
    *dx_ptr = glyph_frame_data.hot_x;
    *dy_ptr = glyph_frame_data.hot_y;

    if ((unsigned char)ch == '\t') {
        *dx_ptr += 20;
    }

    return true;
}

bool tig_font_modern_open(TigFont* font)
{
    if (font->modern_font_path == NULL || font->modern_font_path[0] == '\0') {
        return false;
    }

    if (!TTF_Init()) {
        return false;
    }

    font->modern_font = TTF_OpenFont(font->modern_font_path, font->modern_font_size);
    if (font->modern_font == NULL) {
        TTF_Quit();
        return false;
    }

    return true;
}

void tig_font_modern_close(TigFont* font)
{
    if (font->modern_font != NULL) {
        TTF_CloseFont((TTF_Font*)font->modern_font);
        font->modern_font = NULL;
        TTF_Quit();
    }
}

bool tig_font_modern_measure(TigFont* font)
{
    SDL_Surface* surface;
    int width;

    if (font->str == NULL) {
        font->height = 0;
        return true;
    }

    width = font->width;
    surface = tig_font_modern_render(font, font->str, width, font->color);
    if (surface == NULL) {
        font->height = 0;
        return false;
    }

    if (font->width == 0) {
        font->width = surface->w;
    }
    font->height = surface->h;

    SDL_DestroySurface(surface);

    return true;
}

int tig_font_modern_write(TigVideoBuffer* video_buffer, const char* str, const TigRect* rect, TigRect* dirty_rect)
{
    TigFont* font;
    SDL_Surface* text_surface;
    SDL_Surface* shadow_surface;
    TigRect src_rect;
    TigRect dst_rect;
    int dx;
    int rc;

    if (str == NULL) {
        return TIG_OK;
    }

    font = tig_font_stack[tig_font_stack_index];
    text_surface = tig_font_modern_render(font, str, rect->width, font->color);
    if (text_surface == NULL) {
        return TIG_ERR_GENERIC;
    }

    dx = (font->flags & TIG_FONT_CENTERED) != 0
        ? (rect->width - text_surface->w) / 2
        : (font->flags & TIG_FONT_RIGHT) != 0 ? rect->width - text_surface->w : 0;
    if (dx < 0) {
        dx = 0;
    }

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = text_surface->w;
    src_rect.height = text_surface->h;

    dst_rect.x = rect->x + dx;
    dst_rect.y = rect->y;
    dst_rect.width = text_surface->w;
    dst_rect.height = text_surface->h;

    // SDL_ttf treats a zero foreground alpha as opaque, so skip hidden shadows.
    if ((font->flags & TIG_FONT_SHADOW) != 0 && font->modern_shadow_opacity > 0) {
        shadow_surface = tig_font_modern_render_color(font, str, rect->width, tig_font_modern_shadow_color(font));
        if (shadow_surface != NULL) {
            TigRect shadow_rect = dst_rect;
            shadow_rect.x += font->modern_shadow_distance;
            shadow_rect.y += font->modern_shadow_distance;
            rc = tig_video_buffer_blit_sdl_surface(shadow_surface, &src_rect, video_buffer, &shadow_rect);
            SDL_DestroySurface(shadow_surface);
            if (rc != TIG_OK) {
                SDL_DestroySurface(text_surface);
                return rc;
            }
        }
    }

    rc = tig_video_buffer_blit_sdl_surface(text_surface, &src_rect, video_buffer, &dst_rect);
    SDL_DestroySurface(text_surface);
    if (rc != TIG_OK) {
        return rc;
    }

    if (dirty_rect != NULL) {
        dirty_rect->x = dst_rect.x;
        dirty_rect->y = dst_rect.y;
        dirty_rect->width = dst_rect.width;
        dirty_rect->height = dst_rect.height;
        if ((font->flags & TIG_FONT_SHADOW) != 0) {
            dirty_rect->width += font->modern_shadow_distance;
            dirty_rect->height += font->modern_shadow_distance;
        }
    }

    return TIG_OK;
}

SDL_Color tig_font_modern_color(tig_color_t color)
{
    SDL_Color sdl_color;

    sdl_color.r = (Uint8)tig_color_get_red(color);
    sdl_color.g = (Uint8)tig_color_get_green(color);
    sdl_color.b = (Uint8)tig_color_get_blue(color);
    sdl_color.a = 255;

    return sdl_color;
}

static SDL_Color tig_font_modern_shadow_color(TigFont* font)
{
    SDL_Color sdl_color;

    sdl_color = tig_font_modern_color(font->modern_shadow_color != 0
        ? font->modern_shadow_color
        : tig_font_shadow_color);
    sdl_color.a = (Uint8)font->modern_shadow_opacity;

    return sdl_color;
}

static SDL_Surface* tig_font_modern_render(TigFont* font, const char* str, int width, tig_color_t color)
{
    SDL_Surface* surface = tig_font_modern_render_color(font, str, width, tig_font_modern_color(color));
    if (surface != NULL && (font->flags & TIG_FONT_DIM_BRACKETS) != 0 && SDL_strchr(str, '[') != NULL) {
        tig_font_modern_dim_brackets(font, str, width, surface);
    }
    return surface;
}

static void tig_font_modern_dim_brackets(TigFont* font, const char* str, int width, SDL_Surface* surface)
{
    char* utf8 = tig_font_modern_to_utf8(str);
    if (utf8 == NULL) return;
    TTF_Text* layout = TTF_CreateText(NULL, (TTF_Font*)font->modern_font, utf8, 0);
    if (layout == NULL) {
        FREE(utf8);
        return;
    }
    if (width > 0) TTF_SetTextWrapWidth(layout, width);
    if (surface->format != SDL_PIXELFORMAT_ARGB8888 || !SDL_LockSurface(surface)) {
        TTF_DestroyText(layout);
        FREE(utf8);
        return;
    }
    char* cursor = utf8;
    while ((cursor = SDL_strchr(cursor, '[')) != NULL) {
        char* end = SDL_strchr(cursor, ']');
        int count = 0;
        TTF_SubString** spans = TTF_GetTextSubStringsForRange(layout, (int)(cursor - utf8),
            end != NULL ? (int)(end - cursor + 1) : -1, &count);
        if (spans != NULL) {
            for (int index = 0; index < count; index++) {
                SDL_Rect span = spans[index]->rect;
                int left = SDL_max(span.x, 0);
                int right = SDL_min(span.x + span.w, surface->w);
                int top = SDL_max(span.y, 0);
                int bottom = SDL_min(span.y + span.h, surface->h);
                for (int y = top; y < bottom; y++) {
                    Uint32* pixels = (Uint32*)((Uint8*)surface->pixels + y * surface->pitch);
                    for (int x = left; x < right; x++) {
                        Uint32 pixel = pixels[x];
                        Uint32 red = ((pixel >> 16) & 255) * font->bracket_dim_percent / 100;
                        Uint32 green = ((pixel >> 8) & 255) * font->bracket_dim_percent / 100;
                        Uint32 blue = (pixel & 255) * font->bracket_dim_percent / 100;
                        pixels[x] = (pixel & 0xFF000000u) | (red << 16) | (green << 8) | blue;
                    }
                }
            }
            SDL_free(spans);
        }
        if (end == NULL) break;
        cursor = end + 1;
    }
    SDL_UnlockSurface(surface);
    TTF_DestroyText(layout);
    FREE(utf8);
}

SDL_Surface* tig_font_modern_render_color(TigFont* font, const char* str, int width, SDL_Color color)
{
    TTF_FontStyleFlags style;
    SDL_Surface* surface;
    char* utf8;

    style = TTF_STYLE_NORMAL;
    if ((font->flags & TIG_FONT_UNDERLINE) != 0) {
        style |= TTF_STYLE_UNDERLINE;
    }
    if ((font->flags & TIG_FONT_STRIKE_THROUGH) != 0) {
        style |= TTF_STYLE_STRIKETHROUGH;
    }
    TTF_SetFontStyle((TTF_Font*)font->modern_font, style);

    utf8 = tig_font_modern_to_utf8(str);
    if (utf8 == NULL) {
        return NULL;
    }

    if (width > 0) {
        surface = TTF_RenderText_Blended_Wrapped((TTF_Font*)font->modern_font, utf8, 0, color, width);
    } else {
        surface = TTF_RenderText_Blended((TTF_Font*)font->modern_font, utf8, 0, color);
    }
    FREE(utf8);
    if (surface != NULL) {
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    }

    return surface;
}

char* tig_font_modern_to_utf8(const char* str)
{
    size_t len;
    const unsigned char* src;
    char* utf8;
    char* dst;

    if (str == NULL) {
        return NULL;
    }

    len = SDL_strlen(str);
    utf8 = (char*)MALLOC(len * 3 + 1);
    if (utf8 == NULL) {
        return NULL;
    }

    src = (const unsigned char*)str;
    dst = utf8;
    while (*src != '\0') {
        dst = tig_font_modern_utf8_append(dst, tig_font_modern_cp1252_to_codepoint(*src));
        src++;
    }
    *dst = '\0';

    return utf8;
}

int tig_font_modern_cp1252_to_codepoint(unsigned char ch)
{
    static const int cp1252_80_9f[32] = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    };

    if (ch >= 0x80 && ch <= 0x9F) {
        return cp1252_80_9f[ch - 0x80];
    }

    return ch;
}

char* tig_font_modern_utf8_append(char* p, int codepoint)
{
    if (codepoint < 0x80) {
        *p++ = (char)codepoint;
    } else if (codepoint < 0x800) {
        *p++ = (char)(0xC0 | (codepoint >> 6));
        *p++ = (char)(0x80 | (codepoint & 0x3F));
    } else {
        *p++ = (char)(0xE0 | (codepoint >> 12));
        *p++ = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        *p++ = (char)(0x80 | (codepoint & 0x3F));
    }

    return p;
}
