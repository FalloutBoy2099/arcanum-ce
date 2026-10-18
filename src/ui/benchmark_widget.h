#ifndef ARCANUM_UI_BENCHMARK_WIDGET_H_
#define ARCANUM_UI_BENCHMARK_WIDGET_H_

#include <tig/tig.h>

void benchmark_widget_update(TigRect screen, bool visible, tig_font_handle_t font,
    const char* label, const char* notice);
bool benchmark_widget_mouse(TigMessage* message);
bool benchmark_widget_take_request(void);
void benchmark_widget_close(void);

#endif
