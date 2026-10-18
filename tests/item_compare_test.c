#include "ui/item_compare.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

static void fixed_descriptions(void)
{
    ItemCompareStats boots = {0}, worn = {0};
    const char* text = "CA: 5 RD: +6 PB: -10 (Vitesse +5)";
    CHECK(item_compare_parse_stat(text, "CA", 50, false, &boots));
    CHECK(item_compare_parse_stat(text, "RD", 51, false, &boots));
    CHECK(item_compare_parse_stat(text, "PB", 56, false, &boots));
    CHECK(item_compare_parse_stat(text, "Vitesse", 8, true, &boots));
    CHECK(item_compare_parse_stat("RD:+3", "RD", 51, false, &worn));
    ItemCompareRange ranges[ITEM_COMPARE_RANGE_CAPACITY];
    CHECK(item_compare_stats(&boots, &worn, ranges) == 4);
    CHECK(ranges[0].direction == 1 && ranges[1].direction == 1);
    CHECK(ranges[2].direction == -1 && ranges[3].direction == 1);
    CHECK(ranges[0].start == 4 && ranges[0].length == 1);
    CHECK(ranges[1].start == 10 && ranges[1].length == 2);
    CHECK(ranges[2].start == 17 && ranges[2].length == 3);
    CHECK(ranges[3].start == (int)(strstr(text, "+5") - text) && ranges[3].length == 2);
    ItemCompareStats shield = {0}, wood = {0};
    CHECK(item_compare_parse_stat("CA: 30 DE: 1-10 pour l'assaillant", "CA", 50, false, &shield));
    CHECK(item_compare_parse_stat("CA:10", "CA", 50, false, &wood));
    CHECK(item_compare_stats(&shield, &wood, ranges) == 1 && ranges[0].direction == 1);
    // Fixed descriptions show totals; generated stats may split base and bonus.
    wood.entries[0] = (ItemCompareStat){50, 20, 0, 15, 0, 0};
    CHECK(item_compare_stats(&shield, &wood, ranges) == 1 && ranges[0].direction == -1);
    CHECK(item_compare_parse_stat("CA:30", "CA", 50, false, &wood));
    CHECK(item_compare_stats(&shield, &wood, ranges) == 1 && ranges[0].direction == 0);
    CHECK(wood.count == 1); // Overrides rather than duplicating the raw field.
    ItemCompareStats english = {0};
    CHECK(item_compare_parse_stat("AC: 5 DR: +6 (Speed +5)", "AC", 50, false, &english));
    CHECK(item_compare_parse_stat("AC: 5 DR: +6 (Speed +5)", "DR", 51, false, &english));
    CHECK(item_compare_parse_stat("AC: 5 DR: +6 (Speed +5)", "Speed", 8, true, &english));
    CHECK(!item_compare_parse_stat("CAC:30", "CA", 50, false, &english));
    CHECK(!item_compare_parse_stat("CA 30", "CA", 50, false, &english));
    CHECK(!item_compare_parse_stat("CA:999999999999999999999", "CA", 50, false, &english));
    CHECK(!item_compare_parse_stat("CA:5foo", "CA", 50, false, &english));
    CHECK(!item_compare_parse_stat("CA:5.5", "CA", 50, false, &english));
    CHECK(item_compare_parse_stat("CA:0", "CA", 50, false, &english));
    CHECK(item_compare_stats(&english, &wood, ranges) == 3 && ranges[0].direction == -1);
}

static void category_descriptions(void)
{
    const char* fields[] = {"CA: 5", "CA: 9 RD: 18 RF: 5 PB: -5", "DX: +2", "BE: +2",
        "CA: 30 DE: 1-10 pour l'assaillant", "RD: +5 DX: +2", "CA: 5 RD: +6 PB: -10 (Vitesse +5)"};
    // Helmet, torso, ring, medallion, shield, gauntlets and boots use the same
    // recognized armor fields; unrelated reflected damage is deliberately excluded.
    for (int i = 0; i < 7; i++) {
        ItemCompareStats values = {0}, base = {0};
        item_compare_parse_stat(fields[i], "CA", 50, true, &values);
        item_compare_parse_stat(fields[i], "RD", 51, true, &values);
        item_compare_parse_stat(fields[i], "RF", 53, true, &values);
        item_compare_parse_stat(fields[i], "PB", 56, true, &values);
        item_compare_parse_stat(fields[i], "DX", 101, true, &values);
        item_compare_parse_stat(fields[i], "BE", 103, true, &values);
        item_compare_parse_stat(fields[i], "Vitesse", 8, true, &values);
        ItemCompareRange ranges[ITEM_COMPARE_RANGE_CAPACITY];
        CHECK(item_compare_stats(&values, &base, ranges) > 0);
    }
    ItemCompareStats values = {0}, base = {0};
    CHECK(item_compare_parse_stat("DGT: 1-8(+12) FT: 1-4(+2) PORT: 15", "DGT", 43, true, &values));
    CHECK(item_compare_parse_stat("DGT: 1-8(+12) FT: 1-4(+2) PORT: 15", "FT", 44, true, &values));
    CHECK(item_compare_parse_stat("DGT: 1-8(+12) FT: 1-4(+2) PORT: 15", "PORT", 46, true, &values));
    CHECK(values.entries[0].adj == 12 && values.entries[1].adj == 2);
    CHECK(item_compare_parse_stat("D: 2-6(+4)", "D", 43, true, &base));
    ItemCompareRange ranges[ITEM_COMPARE_RANGE_CAPACITY];
    CHECK(item_compare_stats(&values, &base, ranges) == 7);
    CHECK(ranges[0].direction == -1 && ranges[1].direction == 1 && ranges[2].direction == 1);
    CHECK(ranges[2].start == 9 && ranges[2].length == 3);
    values = (ItemCompareStats){0};
    CHECK(item_compare_parse_stat("CA 12(+15)", "CA", 50, true, &values));
    CHECK(values.entries[0].adj == 15 && !values.entries[0].total);
    CHECK(!item_compare_parse_stat("RD/RF: +5", "RF", 53, true, &values));
    CHECK(!item_compare_parse_stat("RD/RF: +5", "RD", 51, true, &values));
    CHECK(item_compare_parse_stat("D: 1-12 (+10 Morts-vivants)", "D", 43, true, &values));
    CHECK(values.entries[1].adj == 0); // Conditional damage is not universal damage.
    values = (ItemCompareStats){0};
    CHECK(item_compare_parse_stat("CA:+000", "CA", 50, false, &values));
    CHECK(item_compare_stats(&values, &base, ranges) == 1);
    CHECK(ranges[0].length == 4 && ranges[0].direction == 0);
    values = (ItemCompareStats){0};
    CHECK(item_compare_parse_stat("D:0-0", "D", 43, false, &values));
    CHECK(item_compare_stats(&values, &base, ranges) == 2);
}

static void rendering(const char* font_path)
{
    CHECK(SDL_Init(0));
    tig_color_init(NULL);
    tig_color_set_rgba_settings(0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    tig_rect_init(NULL);
    TigInitInfo init = {0}; init.width = 640; init.height = 480;
    CHECK(tig_window_init(&init) == TIG_OK);
    TigFont font = {0};
    font.flags = TIG_FONT_MODERN;
    font.modern_font_path = font_path; font.modern_font_size = 14;
    tig_font_handle_t fonts[4];
    unsigned int colors[4] = {0xffffff, 0xff0000, 0x00ff00, 0xf5f5f5};
    for (int i = 0; i < 4; i++) {
        font.color = colors[i];
        tig_font_create(&font, &fonts[i]);
    }
    TigWindowData wd = {0}; wd.rect = (TigRect){0, 0, 400, 100};
    tig_window_handle_t window;
    CHECK(tig_window_create(&wd, &window) == TIG_OK);
    TigVideoBuffer* buffer;
    CHECK(tig_window_vbid_get(window, &buffer) == TIG_OK);
    const char* text = "Damage:12 Speed:34 Weight:56";
    ItemCompareRange ranges[] = {{7,2,1}, {16,2,-1}, {26,2,0}};
    // Last numeric offset is calculated explicitly, keeping labels out of ranges.
    ranges[2].start = (int)strlen(text) - 2;
    for (int pass = 0; pass < 3; pass++) {
        TigRect full = {0,0,400,100};
        tig_window_fill(window, &full, 0);
        TigRect area = {3,4,pass == 1 ? 95 : 350,80};
        item_compare_write(window, text, area, ranges, 3, fonts, pass == 2);
        int found[4] = {0};
        CHECK(tig_video_buffer_lock(buffer) == TIG_OK);
        TigVideoBufferData data;
        CHECK(tig_video_buffer_data(buffer, &data) == TIG_OK);
        int outside = 0, colored_label = 0;
        for (int y = 0; y < 100; y++) for (int x = 0; x < 400; x++) {
            unsigned int color = *(Uint32*)((Uint8*)data.pixels + y * data.pitch + x * 4) & 0xffffff;
            if (color && !(x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height)) outside++;
            for (int i = 0; i < 4; i++) if (color == colors[i]) found[i]++;
            // The first label has no colored number before its colon.
            if (pass == 0 && x < 44 && color && ((color >> 16) & 255) != ((color >> 8) & 255)) colored_label++;
        }
        tig_video_buffer_unlock(buffer);
        CHECK(outside == 0);
        CHECK(colored_label == 0);
        for (int i = 0; i < 4; i++) CHECK(found[i] > 0);
    }
    tig_window_destroy(window);
    for (int i = 0; i < 4; i++) tig_font_destroy(fonts[i]);
    tig_window_exit(); tig_rect_exit(); tig_color_exit(); SDL_Quit();
}

int main(int argc, char** argv)
{
    fixed_descriptions();
    category_descriptions();
    CHECK(item_compare_value(12, 8, false) == 1); // Higher speed is faster.
    CHECK(item_compare_value(8, 12, true) == 1); // Lower weight/failure chance.
    CHECK(item_compare_value(8, 12, false) == -1);
    CHECK(item_compare_value(-2, -1, true) == 1);
    CHECK(item_compare_value(12, 12, false) == 0);
    ItemCompareStats values = {{{43,1,9,0,2,3}, {51,15,0,0,9,2}, {45,0,0,2,15,4}},3};
    ItemCompareStats reference = {{{43,3,7,0,0,0}, {51,20,0,0,0,0}},2};
    ItemCompareRange ranges[ITEM_COMPARE_RANGE_CAPACITY];
    CHECK(item_compare_stats(&values, &reference, ranges) == 4);
    CHECK(ranges[0].direction == -1); // The minimum and maximum are independent.
    CHECK(ranges[1].direction == 1);
    CHECK(ranges[2].direction == -1);
    CHECK(ranges[3].direction == 1); // Missing reference stat means zero.
    CHECK(ranges[3].start == 16 && ranges[3].length == 2);
    CHECK(ranges[0].length == 1 && ranges[1].start == 4);
    CHECK(argc > 1);
    if (argc > 1) rendering(argv[1]);
    printf("Equipment comparison: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
