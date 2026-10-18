#ifndef ARCANUM_GAME_WORLD_COMPOSITION_H_
#define ARCANUM_GAME_WORLD_COMPOSITION_H_

#include <tig/tig.h>
#include <tig/video_upload.h>

int world_composition_present(TigVideoBuffer* world, TigRect source, TigVideoBuffer* output,
    TigRect destination, TigVideoUpload* damage, bool linear);
int world_composition_scroll(TigVideoBuffer* world, TigRect bounds, int dx, int dy);

#endif
