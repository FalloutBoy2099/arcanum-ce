#ifndef ARCANUM_GAME_NAME_H_
#define ARCANUM_GAME_NAME_H_

#include "game/context.h"

// Optional loose-file artwork used only by options-page navigation.
#define NAME_OPTIONS_MENU_LEFT_BUTTON 4094
#define NAME_OPTIONS_MENU_RIGHT_BUTTON 4095
#define NAME_MENU_WOOD_MASK 4093

bool name_init(GameInitInfo* init_info);
void name_exit(void);
tig_art_id_t name_normalize_aid(tig_art_id_t aid);
int name_resolve_path(tig_art_id_t aid, char* path, size_t maxlen);
bool sub_41F090(int a1, int* a2, int* a3);

#endif /* ARCANUM_GAME_NAME_H_ */
