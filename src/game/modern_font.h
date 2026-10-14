#ifndef ARCANUM_GAME_MODERN_FONT_H_
#define ARCANUM_GAME_MODERN_FONT_H_

#include "tig/types.h"

bool modern_font_enabled(void);
const char* modern_font_path(void);
float modern_font_size(void);
const char* modern_font_path_for(const char* key);
float modern_font_size_for(const char* key);

#endif /* ARCANUM_GAME_MODERN_FONT_H_ */
