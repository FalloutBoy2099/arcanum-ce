#ifndef ARCANUM_GAME_CRAFTING_H_
#define ARCANUM_GAME_CRAFTING_H_

#include "game/context.h"
#include "game/schematic.h"

#define CRAFTING_PARTY_COMPONENTS_KEY "crafting_party_components"
typedef struct CraftingIngredient {
    int64_t obj;
    int64_t owner;
    int alternative;
} CraftingIngredient;

// A display-only result from one inventory snapshot. Never reuse this to
// consume ingredients: crafting_process rechecks the live inventories.
typedef struct CraftingPreview {
    CraftingIngredient components[2];
    int64_t prototypes[2];
    int stock[2];
    int discipline[2];
    int complexity[2];
    bool component_available[2];
    bool expertise_available[2];
    bool pair_available;
} CraftingPreview;

void crafting_preview(int64_t crafter, const SchematicInfo* schematic, CraftingPreview* preview);

int crafting_component_count(int64_t crafter, const int alternatives[3]);
bool crafting_find_component(int64_t crafter, const int alternatives[3], CraftingIngredient* ingredient);
bool crafting_find_pair(int64_t crafter, const SchematicInfo* schematic, CraftingIngredient ingredients[2]);
bool crafting_process(int64_t crafter, const SchematicInfo* schematic);
bool crafting_products_dropped(void);
bool crafting_learned_schematic_known(int schematic, int degree);

#endif
