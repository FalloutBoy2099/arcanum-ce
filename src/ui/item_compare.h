#ifndef ARCANUM_UI_ITEM_COMPARE_H_
#define ARCANUM_UI_ITEM_COMPARE_H_

#include <stdint.h>
#include <tig/tig.h>

#define ITEM_COMPARE_STAT_CAPACITY 32
#define ITEM_COMPARE_RANGE_CAPACITY (ITEM_COMPARE_STAT_CAPACITY * 3)

typedef struct ItemCompareStat {
    int id, min, max, adj;
    int start, length;
    bool modifier;
    bool total; // A fixed effect description gives the displayed total.
    int min_length, max_start, max_length, adj_start, adj_length;
} ItemCompareStat;

typedef struct ItemCompareStats {
    ItemCompareStat entries[ITEM_COMPARE_STAT_CAPACITY];
    int count;
} ItemCompareStats;

typedef struct ItemCompareRange {
    int start, length;
    int direction; // -1 worse, 0 equal, +1 better.
} ItemCompareRange;

int64_t item_compare_reference(int64_t owner, int64_t item);
int64_t item_compare_inventory_owner(int64_t item, int64_t character,
    int64_t target, int64_t stock, bool barter);
int item_compare_value(int64_t value, int64_t reference, bool lower_is_better);
int item_compare_stats(const ItemCompareStats* values, const ItemCompareStats* reference, ItemCompareRange* ranges);
bool item_compare_parse_stat(const char* text, const char* label, int id, bool allow_colonless, ItemCompareStats* stats);
void item_compare_write(tig_window_handle_t window, const char* text, TigRect rect,
    const ItemCompareRange* ranges, int count, const tig_font_handle_t fonts[4], bool right);

#endif
