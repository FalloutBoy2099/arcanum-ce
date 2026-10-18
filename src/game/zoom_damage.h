#ifndef ARCANUM_GAME_ZOOM_DAMAGE_H_
#define ARCANUM_GAME_ZOOM_DAMAGE_H_

#include "game/context.h"
#include <tig/video_upload.h>

typedef struct ZoomMapping {
    TigRect source;
    TigRect destination;
    const void* source_buffer;
    const void* destination_buffer;
    int64_t camera_x;
    int64_t camera_y;
    bool linear;
} ZoomMapping;

typedef struct ZoomDamageState {
    bool valid;
    ZoomMapping mapping;
} ZoomDamageState;

void zoom_damage_init(TigVideoUpload* plan, int width, int height);
void zoom_damage_screen(TigVideoUpload* plan, TigRect dirty);
void zoom_damage_world(TigVideoUpload* plan, TigRect source, TigRect destination, TigRect dirty);
// Rebuild the whole output when mapping changes or partial scalar work becomes
// more expensive than the existing SIMD full scaler. Otherwise retain regions.
void zoom_damage_finish(TigVideoUpload* plan, ZoomDamageState* state, const ZoomMapping* mapping);

#endif
