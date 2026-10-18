#ifndef TIG_COLOR_GRADE_H_
#define TIG_COLOR_GRADE_H_

#include "tig/video_upload.h"

typedef struct TigColorGrade {
    float red;
    float green;
    float blue;
    float saturation;
} TigColorGrade;

bool tig_color_grade_apply(SDL_Surface* frame, const TigVideoUpload* damage, const TigColorGrade* grade);
bool tig_water_shader_apply(SDL_Surface* frame, SDL_Surface* mask, const TigVideoUpload* damage,
    float time, float strength, float origin_x, float origin_y);
void tig_color_grade_shutdown(void);

#endif
