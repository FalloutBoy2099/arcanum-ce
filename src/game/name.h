#ifndef ARCANUM_GAME_NAME_H_
#define ARCANUM_GAME_NAME_H_

#include "game/context.h"

// Custom interface artwork resolved without modifying interface.mes.
#define NAME_OPTIONS_MENU_LEFT_BUTTON 4094
#define NAME_OPTIONS_MENU_RIGHT_BUTTON 4095
#define NAME_MENU_WOOD_MASK 4093
#define NAME_JOURNAL_INDEX_BUTTON 4092
#define NAME_SCHEMATIC_BASE_XL 4091
#define NAME_BOOK_MAIN_XL 4090
#define NAME_BOOK_SIDE_XL 4089
#define NAME_MAP_MAIN_XL 4088

bool name_init(GameInitInfo* init_info);
void name_exit(void);
tig_art_id_t name_normalize_aid(tig_art_id_t aid);
int name_resolve_path(tig_art_id_t aid, char* path, size_t maxlen);
bool sub_41F090(int a1, int* a2, int* a3);

#endif /* ARCANUM_GAME_NAME_H_ */
