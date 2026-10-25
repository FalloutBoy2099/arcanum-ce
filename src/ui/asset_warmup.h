#ifndef ARCANUM_UI_ASSET_WARMUP_H_
#define ARCANUM_UI_ASSET_WARMUP_H_

#include <stdbool.h>

#define UI_ASSET_WARMUP_CAPACITY 128

void ui_asset_warmup_clear(void);
void ui_asset_warmup_rewind(void);
void ui_asset_warmup_register(int art_number);
int ui_asset_warmup_startup(void);
void ui_asset_warmup_tick(bool allowed);

#endif
