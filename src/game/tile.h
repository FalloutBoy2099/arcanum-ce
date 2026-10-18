#ifndef ARCANUM_GAME_TILE_H_
#define ARCANUM_GAME_TILE_H_

#include "game/context.h"

bool tile_init(GameInitInfo* init_info);
void tile_exit(void);
void tile_resize(GameResizeInfo* resize_info);
void tile_update_view(ViewOptions* view_options);
void tile_toggle_visibility(void);
void tile_draw(GameDrawInfo* draw_info);
int tile_id_from_loc(int64_t loc);
tig_art_id_t tile_art_id_at(int64_t loc);
bool tile_is_blocking(int64_t loc, bool a2);
bool tile_is_soundproof(int64_t loc);
bool tile_is_sinkable(int64_t loc);
bool tile_is_slippery(int64_t loc);
void tile_cycle_variant(int64_t loc);
tig_art_id_t tile_transition_art_id_create_random(tig_art_id_t art_id, int num2, bool flippable2, int a4);
void tile_render_topdown_buffer(tig_art_id_t art_id, TigVideoBuffer* video_buffer);
void tile_set_render_target(TigVideoBuffer* vb);

#define TILE_X(tile) ((tile) & 0x3F)
#define TILE_Y(tile) (((tile) >> 6) & 0x3F)
#define TILE_MAKE(x, y) ((x) | ((y) << 6))

#endif /* ARCANUM_GAME_TILE_H_ */
