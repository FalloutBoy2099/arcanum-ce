#ifndef ARCANUM_UI_BENCHMARK_UI_H_
#define ARCANUM_UI_BENCHMARK_UI_H_

#include "game/context.h"

bool benchmark_ui_init(GameInitInfo* info);
void benchmark_ui_exit(void);
void benchmark_ui_resize(GameResizeInfo* info);
void benchmark_ui_begin_frame(void);
void benchmark_ui_end_frame(void);
bool benchmark_ui_is_active(void);

#endif
