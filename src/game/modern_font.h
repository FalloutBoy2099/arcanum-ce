#ifndef ARCANUM_GAME_MODERN_FONT_H_
#define ARCANUM_GAME_MODERN_FONT_H_

#include "tig/types.h"

bool modern_font_enabled(void);
const char* modern_font_path(void);
float modern_font_size(void);
const char* modern_font_path_for(const char* key);
float modern_font_size_for(const char* key);
tig_color_t modern_font_color_for(const char* key, int default_red, int default_green, int default_blue);
int modern_font_int_for(const char* key, int default_value, int min_value, int max_value);

#endif /* ARCANUM_GAME_MODERN_FONT_H_ */
