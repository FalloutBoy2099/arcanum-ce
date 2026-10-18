#include "ui/item_compare.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

static bool word_byte(unsigned char ch)
{
    return ch >= 128 || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
        || (ch >= '0' && ch <= '9') || ch == '_';
}

// Match only supplied localized stat labels and an explicit numeric field.
// Arbitrary numbers in prose, spell names or descriptions are never compared.
bool item_compare_parse_stat(const char* text, const char* label, int id, bool allow_colonless, ItemCompareStats* stats)
{
    size_t size = strlen(label);
    if (size == 0 || stats->count >= ITEM_COMPARE_STAT_CAPACITY) return false;
    for (const char* p = text; *p != '\0'; p++) {
        if ((p != text && (word_byte((unsigned char)p[-1]) || p[-1] == '/')) || SDL_strncasecmp(p, label, size) != 0) continue;
        const char* number = p + size;
        if (word_byte((unsigned char)*number) || *number == '/') continue;
        while (*number == ' ') number++;
        if (*number == ':') number++;
        else if (!allow_colonless) continue;
        while (*number == ' ') number++;
        if (!(*number == '+' || *number == '-' || (*number >= '0' && *number <= '9'))) continue;
        errno = 0;
        char* end;
        long min = strtol(number, &end, 10);
        if (end == number || errno == ERANGE || min < INT_MIN || min > INT_MAX) continue;
        int min_length = (int)(end - number);
        long max = 0, adj = 0;
        const char* max_start = NULL;
        if (*end == '-') {
            max_start = end + 1;
            char* upper;
            errno = 0;
            max = strtol(end + 1, &upper, 10);
            if (upper == end + 1 || errno == ERANGE || max < INT_MIN || max > INT_MAX) continue;
            end = upper;
        }
        int max_length = max_start != NULL ? (int)(end - max_start) : 0;
        if (word_byte((unsigned char)*end) || *end == '.') continue;
        const char* adj_start = NULL;
        int adj_length = 0;
        const char* suffix = end;
        while (*suffix == ' ') suffix++;
        if (*suffix == '(') {
            const char* bonus = suffix + 1;
            while (*bonus == ' ') bonus++;
            if (*bonus == '+' || *bonus == '-') {
                char* bonus_end;
                errno = 0;
                long parsed = strtol(bonus, &bonus_end, 10);
                const char* close = bonus_end;
                while (*close == ' ') close++;
                if (bonus_end != bonus && errno != ERANGE && parsed >= INT_MIN && parsed <= INT_MAX && *close == ')') {
                    adj = parsed;
                    adj_start = bonus;
                    adj_length = (int)(bonus_end - bonus);
                    end = (char*)close + 1;
                }
            }
        }
        ItemCompareStat stat = {id, (int)min, (int)max, (int)adj,
            (int)(number - text), (int)(end - number), *number == '+', adj_start == NULL,
            min_length, max_start != NULL ? (int)(max_start - text) : 0, max_length,
            adj_start != NULL ? (int)(adj_start - text) : 0, adj_length};
        // Reference descriptions override the underlying raw field when known.
        for (int i = 0; i < stats->count; i++) {
            if (stats->entries[i].id == id) { stats->entries[i] = stat; return true; }
        }
        stats->entries[stats->count++] = stat;
        return true;
    }
    return false;
}

int item_compare_value(int64_t value, int64_t reference, bool lower_is_better)
{
    int direction = (value > reference) - (value < reference);
    return lower_is_better ? -direction : direction;
}

static int number_length(int value, bool modifier)
{
    char number[32];
    return snprintf(number, sizeof(number), modifier ? "%+d" : "%d", value);
}

int item_compare_stats(const ItemCompareStats* values, const ItemCompareStats* reference, ItemCompareRange* ranges)
{
    int count = 0;
    for (int i = 0; i < values->count; i++) {
        const ItemCompareStat* value = &values->entries[i];
        ItemCompareStat zero = {0};
        const ItemCompareStat* base = &zero;
        for (int j = 0; j < reference->count; j++) {
            if (reference->entries[j].id == value->id) {
                base = &reference->entries[j];
                break;
            }
        }
        int offset = value->start;
        if (value->total || value->min_length != 0 || value->max != 0 || value->min != 0) {
            int size = value->min_length != 0 ? value->min_length : number_length(value->min, value->max == 0 && value->modifier);
            bool totals = value->total || base->total;
            int64_t min = (int64_t)value->min + (totals ? value->adj : 0);
            int64_t base_min = (int64_t)base->min + (totals ? base->adj : 0);
            ranges[count++] = (ItemCompareRange){offset, size, item_compare_value(min, base_min, false)};
            offset += size;
            if (value->max != 0 || value->max_length != 0) {
                offset = value->max_length != 0 ? value->max_start : offset + 1;
                size = value->max_length != 0 ? value->max_length : number_length(value->max, false);
                int64_t base_max = (base->max != 0 ? (int64_t)base->max : base->min) + (totals ? base->adj : 0);
                int64_t max = (int64_t)value->max + (totals ? value->adj : 0);
                ranges[count++] = (ItemCompareRange){offset, size, item_compare_value(max, base_max, false)};
                offset += size;
            }
        }
        if (value->adj != 0 || value->adj_length != 0) {
            int size = value->adj_length != 0 ? value->adj_length : number_length(value->adj, true);
            int start = value->adj_length != 0 ? value->adj_start : offset + 1;
            ranges[count++] = (ItemCompareRange){start, size, item_compare_value(value->adj, base->adj, false)};
        }
    }
    return count;
}

static int measure(const char* text, int length, int* height)
{
    char part[1024];
    if (length >= sizeof(part)) length = sizeof(part) - 1;
    memcpy(part, text, length);
    part[length] = '\0';
    TigFont desc = {0};
    desc.str = part;
    tig_font_measure(&desc);
    *height = desc.height;
    return desc.width;
}

// Wrap complete words, then split each line at explicit numeric ranges. Labels
// and units never inherit a comparison color; both font backends use this path.
void item_compare_write(tig_window_handle_t window, const char* text, TigRect rect,
    const ItemCompareRange* ranges, int count, const tig_font_handle_t fonts[4], bool right)
{
    int offset = 0, height;
    int length = (int)strlen(text);
    tig_font_push(fonts[0]);
    measure("Ag", 2, &height);
    int line_height = height;
    while (offset < length && line_height > 0 && rect.height >= line_height) {
        int end = offset, last_space = -1;
        while (end < length && text[end] != '\n') {
            if (text[end] == ' ') last_space = end;
            // Measure at word boundaries, avoiding a layout calculation per byte.
            if ((text[end + 1] == ' ' || text[end + 1] == '\n' || text[end + 1] == '\0')
                && measure(text + offset, end - offset + 1, &height) > rect.width) break;
            end++;
        }
        if (end < length && text[end] != '\n' && last_space > offset) end = last_space;
        // A localized label may contain a single word wider than the HUD.
        while (end > offset && measure(text + offset, end - offset, &height) > rect.width) end--;
        if (end == offset) break;
        int line_width = measure(text + offset, end - offset, &height);
        int origin = rect.x + (right ? rect.width - line_width : 0);
        int start = offset;
        while (start < end) {
            int font = 0, stop = end;
            for (int i = 0; i < count; i++) {
                int a = ranges[i].start, b = a + ranges[i].length;
                if (start >= a && start < b) {
                    font = ranges[i].direction < 0 ? 1 : ranges[i].direction > 0 ? 2 : 3;
                    if (b < stop) stop = b;
                } else if (a > start && a < stop) stop = a;
            }
            char part[1024];
            int size = stop - start;
            if (size >= sizeof(part)) size = sizeof(part) - 1;
            memcpy(part, text + start, size);
            part[size] = '\0';
            int x = origin + measure(text + offset, start - offset, &height);
            TigRect run = {x, rect.y, rect.x + rect.width - x, line_height};
            tig_font_push(fonts[font]);
            tig_window_text_write(window, part, &run);
            tig_font_pop();
            start = stop;
        }
        offset = end;
        while (text[offset] == ' ') offset++;
        if (text[offset] == '\n') offset++;
        rect.y += line_height;
        rect.height -= line_height;
    }
    tig_font_pop();
}
