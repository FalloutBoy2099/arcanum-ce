#ifndef ARCANUM_GAME_ITEM_PRIVATE_H_
#define ARCANUM_GAME_ITEM_PRIVATE_H_

#include "game/item.h"

char* item_error_str(int reason);
bool item_cancel_decay(int64_t obj);

#endif
