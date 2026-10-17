#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <tig/performance.h>

#include "game/gamelib.h"
#include "game/benchmark.h"
#include "game/map_marker.h"
#include "ui/map_marker_input.h"
#include "ui/search_bar.h"
#include "game/journal_memory.h"
#include "game/journal_text.h"
#include "game/conversation_history.h"
#include "game/crafting.h"
#include "game/ui.h"
#include "game/text_bubble_layout.h"
#include "game/broadcast.h"
#include "game/save_slot.h"
#include "game/lighting_options.h"
#include "game/terrain_encoding.h"
#include "game/sector_script_list.h"
#include "game/encounter_options.h"
#include "game/highres_config.h"
#include "game/input_bindings.h"
#include "game/controller.h"
#include "game/modern_font.h"
#include "game/iso_zoom.h"
#include "game/language.h"
#include "game/mes.h"
#include "game/obj_id.h"

Settings settings;
static int invalidations;
static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)
static const char* button_art_directory;

static void test_journal_index_knowledge(void)
{
    CHECK(journal_text_mentions("I met Gilbert Bates in Tarant.", "Gilbert Bates"));
    CHECK(journal_text_mentions("Meet VIRGIL, then Magnus.", "Virgil"));
    CHECK(journal_text_mentions("Visit \xc9glise.", "\xe9glise"));
    CHECK(journal_text_mentions("Go to T'sen-Ang.", "T'sen-Ang"));
    CHECK(!journal_text_mentions("Debates continue.", "Bates"));
    CHECK(!journal_text_mentions("Tarantian guard", "Tarant"));
    CHECK(!journal_text_mentions("Unknown destination", ""));
    CHECK(!journal_text_mentions("Magnus", "Magnus Shale Fist"));
    CHECK(journal_text_compare("Virgil", "VIRGIL") == 0);

    CHECK(SDL_CreateDirectory("Save"));
    CHECK(SDL_CreateDirectory("Save/Current"));
    CHECK(tig_file_repository_add("."));
    tig_file_remove("Save\\Current\\journal_index.bin");
    journal_memory_reset();
    journal_memory_remember(30037);
    journal_memory_remember(31073);
    journal_memory_remember(30037);
    journal_memory_remember(0);
    CHECK(journal_memory_count() == 2);
    CHECK(!journal_memory_knows(30045)); // A catalog candidate is not knowledge.
    CHECK(journal_memory_save());
    journal_memory_reset();
    CHECK(!journal_memory_knows(30037));
    journal_memory_load();
    CHECK(journal_memory_count() == 2);
    CHECK(journal_memory_knows(30037));
    CHECK(journal_memory_knows(31073));
    CHECK(!journal_memory_knows(30045));
    // Truncated optional data and original saves must never retain knowledge
    // from a different save/game or prevent an otherwise valid save loading.
    TigFile* file = tig_file_fopen("Save\\Current\\journal_index.bin", "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        int bad[] = { 1, 4097 };
        tig_file_fwrite(bad, sizeof(bad), 1, file);
        tig_file_fclose(file);
    }
    journal_memory_load();
    CHECK(journal_memory_count() == 0);
    journal_memory_remember(30037);
    tig_file_remove("Save\\Current\\journal_index.bin");
    journal_memory_load();
    CHECK(journal_memory_count() == 0);
    CHECK(tig_file_repository_remove("."));
}

static void test_conversation_history(void)
{
    DateTime date = { 0 };
    date.days = 20;
    date.milliseconds = 123456;
    uint64_t revision = conversation_history_revision();
    conversation_history_reset();
    CHECK(conversation_history_revision() != revision);
    revision = conversation_history_revision();
    conversation_history_add("Virgil", "", date);
    conversation_history_add("Virgil", NULL, date);
    CHECK(conversation_history_revision() == revision);
    conversation_history_add("Virgil", "A full spoken line.", date);
    CHECK(conversation_history_revision() != revision);
    conversation_history_add("Player", "A selected reply.", date);
    CHECK(conversation_history_count() == 2);
    CHECK(strcmp(conversation_history_get(0)->speaker, "Virgil") == 0);
    CHECK(strcmp(conversation_history_get(1)->text, "A selected reply.") == 0);
    CHECK(conversation_history_get(1)->date.milliseconds == date.milliseconds);
    CHECK(tig_file_repository_add("."));
    revision = conversation_history_revision();
    CHECK(conversation_history_save());
    CHECK(conversation_history_revision() == revision);
    conversation_history_reset();
    revision = conversation_history_revision();
    conversation_history_load();
    CHECK(conversation_history_revision() != revision);
    CHECK(conversation_history_count() == 2);
    CHECK(strcmp(conversation_history_get(0)->text, "A full spoken line.") == 0);
    CHECK(conversation_history_get(0)->date.days == 20);
    for (int i = 0; i < CONVERSATION_HISTORY_CAPACITY; i++) {
        char line[80];
        snprintf(line, sizeof(line), "Message %d", i);
        conversation_history_add("Speaker", line, date);
    }
    CHECK(conversation_history_count() == CONVERSATION_HISTORY_CAPACITY);
    CHECK(strcmp(conversation_history_get(0)->text, "Message 0") == 0);
    CHECK(conversation_history_get(-1) == NULL);
    CHECK(conversation_history_get(CONVERSATION_HISTORY_CAPACITY) == NULL);
    CHECK(conversation_history_save());
    conversation_history_reset();
    conversation_history_load();
    CHECK(conversation_history_count() == CONVERSATION_HISTORY_CAPACITY);
    CHECK(strcmp(conversation_history_get(CONVERSATION_HISTORY_CAPACITY - 1)->text, "Message 255") == 0);
    revision = conversation_history_revision();
    conversation_history_add("Speaker", "Replacement", date);
    CHECK(conversation_history_count() == CONVERSATION_HISTORY_CAPACITY);
    CHECK(conversation_history_revision() != revision);
    TigFile* file = tig_file_fopen("Save\\Current\\conversations.bin", "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        unsigned int invalid[] = { 1, CONVERSATION_HISTORY_CAPACITY + 1 };
        tig_file_fwrite(invalid, sizeof(invalid), 1, file);
        tig_file_fclose(file);
    }
    conversation_history_load();
    CHECK(conversation_history_count() == 0);
    conversation_history_add("Speaker", "Must not leak to an original save", date);
    tig_file_remove("Save\\Current\\conversations.bin");
    conversation_history_load();
    CHECK(conversation_history_count() == 0);
    CHECK(tig_file_repository_remove("."));
    CHECK(language_text_contains("Charged Accelerator Gun", "ACCELERATOR", GAME_LANGUAGE_ENGLISH));
    CHECK(language_text_contains("Herbologie", "herbo", GAME_LANGUAGE_FRENCH));
    const char* accented = "Un \xc9quipement, puis \xe9quipement.";
    CHECK(language_text_find(accented, "\xe9quip", GAME_LANGUAGE_FRENCH) == accented + 3);
    CHECK(language_text_find(accented + 4, "\xc9QUIP", GAME_LANGUAGE_FRENCH) == accented + 20);
    CHECK(language_text_find("abc", "x", GAME_LANGUAGE_ENGLISH) == NULL);
    CHECK(language_text_contains("\xc9quipement", "\xe9quip", GAME_LANGUAGE_FRENCH));
    CHECK(!language_text_contains("Herbologie", "electri", GAME_LANGUAGE_FRENCH));
    char legacy[128];
    language_input_to_legacy("\xc3\x89quipement", legacy, sizeof(legacy), GAME_LANGUAGE_FRENCH);
    CHECK(strcmp(legacy, "\xc9quipement") == 0);
    language_input_to_legacy("\xd0\xa7\xd0\xb0\xd1\x81\xd1\x82\xd1\x8c", legacy, sizeof(legacy), GAME_LANGUAGE_RUSSIAN);
    CHECK(strcmp(legacy, "\xd7\xe0\xf1\xf2\xfc") == 0);
    CHECK(language_text_contains(legacy, "\xf7\xe0\xf1", GAME_LANGUAGE_RUSSIAN));
    language_input_to_legacy("\xc5\x92il", legacy, sizeof(legacy), GAME_LANGUAGE_FRENCH);
    CHECK(language_text_contains(legacy, "\x9cil", GAME_LANGUAGE_FRENCH));
}

static void test_map_markers(void)
{
    TigMessage gesture = { 0 };
    gesture.type = TIG_MESSAGE_MOUSE;
    gesture.data.mouse.event = TIG_MESSAGE_MOUSE_RIGHT_BUTTON_DOWN;
    CHECK(map_marker_edit_gesture(&gesture, true));
    CHECK(!map_marker_edit_gesture(&gesture, false));
    CHECK(map_marker_delete_gesture(&gesture, true));
    CHECK(!map_marker_delete_gesture(&gesture, false));
    gesture.data.mouse.repeat = true;
    CHECK(!map_marker_edit_gesture(&gesture, true));
    CHECK(!map_marker_delete_gesture(&gesture, true));
    gesture.data.mouse.repeat = false;
    gesture.data.mouse.event = TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN;
    CHECK(!map_marker_edit_gesture(&gesture, true));
    gesture.data.mouse.event = TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP;
    CHECK(map_marker_release_gesture(&gesture, TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP));
    CHECK(!map_marker_release_gesture(&gesture, TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP));
    CHECK(!map_marker_release_gesture(&gesture, -1));
    char text[MAP_MARKER_TEXT_SIZE] = "unchanged";
    CHECK(!map_marker_description_valid(NULL));
    CHECK(!map_marker_description_valid(""));
    CHECK(!map_marker_description_valid(" \t\r\n"));
    CHECK(map_marker_description_valid("\xc9glise"));
    char description[MAP_MARKER_TEXT_SIZE + 1];
    memset(description, 'x', sizeof(description));
    description[MAP_MARKER_TEXT_SIZE - 1] = '\0';
    CHECK(map_marker_description_valid(description));
    description[MAP_MARKER_TEXT_SIZE - 1] = 'x';
    description[MAP_MARKER_TEXT_SIZE] = '\0';
    CHECK(!map_marker_description_valid(description));
    CHECK(!map_marker_compose(text, sizeof(text), "   ", "body"));
    CHECK(strcmp(text, "unchanged") == 0);
    CHECK(!map_marker_compose(text, sizeof(text), "bad\ntitle", ""));
    CHECK(!map_marker_compose(text, 4, "Title", "body"));
    CHECK(map_marker_compose(text, sizeof(text), "\xc9glise", "Revenir avec Magnus."));
    CHECK(language_text_contains(text, "\xe9GLISE", GAME_LANGUAGE_FRENCH));
    char title[MAP_MARKER_TITLE_SIZE], body[MAP_MARKER_BODY_SIZE];
    map_marker_split(text, title, body);
    CHECK(strcmp(title, "\xc9glise") == 0 && strcmp(body, "Revenir avec Magnus.") == 0);
    map_marker_split("An old note", title, body);
    CHECK(strcmp(title, "An old note") == 0 && body[0] == '\0');
    memset(title, 'a', sizeof(title) - 1); title[sizeof(title) - 1] = '\0';
    memset(body, 'b', sizeof(body) - 1); body[sizeof(body) - 1] = '\0';
    CHECK(map_marker_compose(text, sizeof(text), title, body));
    CHECK(strlen(text) == 245);
    for (int type = 0; type < 8; type++) CHECK(map_marker_serialized_valid(type, 249));
    CHECK(!map_marker_serialized_valid(8, 1));
    CHECK(!map_marker_serialized_valid(-1, 1));
    CHECK(!map_marker_serialized_valid(0, -1));
    CHECK(!map_marker_serialized_valid(0, 250));

    CHECK(tig_file_repository_add("."));
    MapMarkerRecord record = { 901, MAP_MARKER_PERSONAL | MAP_MARKER_COMPLETED | 0x4, 1234, 5678, { 1234, 5678, 220, 50 }, 2 };
    CHECK(map_marker_compose(record.text, sizeof(record.text), "Treasure", "Locked chest"));
    TigFile* stream = tig_file_fopen("marker-test.bin", "wb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK(map_marker_record_save(&record, stream));
        CHECK(map_marker_record_save(&record, stream));
        tig_file_fclose(stream);
    }
    stream = tig_file_fopen("marker-test.bin", "rb");
    CHECK(stream != NULL);
    MapMarkerRecord loaded = { 0 };
    if (stream != NULL) {
        CHECK(map_marker_record_load(&loaded, stream));
        CHECK(loaded.id == 901 && loaded.x == 1234 && loaded.y == 5678 && loaded.type == 2);
        CHECK(loaded.flags == (MAP_MARKER_PERSONAL | MAP_MARKER_COMPLETED));
        CHECK(strcmp(loaded.text, record.text) == 0);
        CHECK(memcmp(loaded.bounds, record.bounds, sizeof(record.bounds)) == 0);
        CHECK(map_marker_record_load(&loaded, stream));
        CHECK(!map_marker_record_load(&loaded, stream));
        tig_file_fclose(stream);
    }
    // Old saves use the same ten 32-bit fields followed by an unterminated
    // payload; exercise that format without using the new writer.
    int sizes[] = { 5, -1, 250, 1000000 };
    for (int i = 0; i < SDL_arraysize(sizes); i++) {
        int fields[] = { 202, 4, 10, 20, 10, 20, 220, 50, 0, sizes[i] };
        stream = tig_file_fopen("marker-test.bin", "wb");
        CHECK(stream != NULL);
        if (stream == NULL) continue;
        tig_file_fwrite(fields, sizeof(int), 10, stream);
        tig_file_fwrite("Hello", 1, 5, stream);
        tig_file_fclose(stream);
        stream = tig_file_fopen("marker-test.bin", "rb");
        CHECK(stream != NULL);
        if (stream == NULL) continue;
        MapMarkerRecord previous = loaded;
        CHECK(map_marker_record_load(&loaded, stream) == (i == 0));
        if (i == 0) CHECK(loaded.id == 202 && loaded.flags == 0 && strcmp(loaded.text, "Hello") == 0);
        else CHECK(memcmp(&loaded, &previous, sizeof(loaded)) == 0);
        tig_file_fclose(stream);
    }
    tig_file_remove("marker-test.bin");
    CHECK(tig_file_repository_remove("."));
}

static void test_journal_catalogs(const char* resources)
{
    char path[TIG_MAX_PATH];
    mes_file_handle_t catalog;
    MesFileEntry entry;
    const char* languages[] = { "english", "french" };
    for (int i = 0; i < SDL_arraysize(languages); i++) {
        snprintf(path, sizeof(path), "%s/%s", resources, languages[i]);
        CHECK(tig_file_repository_add(path));
        CHECK(mes_load("mes\\map_markers.mes", &catalog));
        for (int id = 0; id <= 28; id++) { entry.num = id; CHECK(mes_search(catalog, &entry)); CHECK(entry.str[0] != '\0'); }
        mes_unload(catalog);
        CHECK(mes_load("mes\\journal_index.mes", &catalog));
        for (int id = 0; id <= 6; id++) {
            entry.num = id;
            CHECK(mes_search(catalog, &entry));
            CHECK(entry.str[0] != '\0');
        }
        for (int id = 10; id <= 12; id++) { entry.num = id; CHECK(mes_search(catalog, &entry)); }
        for (int id = 20; id <= 22; id++) { entry.num = id; CHECK(mes_search(catalog, &entry)); }
        mes_unload(catalog);
        CHECK(mes_load("mes\\crafting_ui.mes", &catalog));
        for (int id = 0; id <= 12; id++) { entry.num = id; CHECK(mes_search(catalog, &entry)); }
        mes_unload(catalog);
        CHECK(mes_load("mes\\options_ui.mes", &catalog));
        entry.num = 1; CHECK(mes_search(catalog, &entry)); CHECK(strcmp(entry.str, "6") == 0);
        int option_ids[] = { 115, 1115, 304, 1304, 312, 1312, 313, 1313, 401, 1401, 406, 1406, 407, 1407, 408, 1408, 409, 1409, 410, 1410, 411, 1411, 2070, 2071, 2072 };
        for (int n = 0; n < SDL_arraysize(option_ids); n++) {
            entry.num = option_ids[n]; CHECK(mes_search(catalog, &entry)); CHECK(entry.str[0] != '\0');
        }
        entry.num = 116; CHECK(mes_search(catalog, &entry));
        entry.num = 1116; CHECK(mes_search(catalog, &entry));
        mes_unload(catalog);
        CHECK(mes_load("mes\\journal_people.mes", &catalog));
        CHECK(mes_num_entries(catalog) > 300);
        entry.num = 31073; // Virgil is a character even before his map loads.
        CHECK(mes_search(catalog, &entry));
        entry.num = 30037; // Gilbert Bates.
        CHECK(mes_search(catalog, &entry));
        int items[] = { 30013, 30014, 30034, 30055 }; // Statue, poison, journal, shovel.
        for (int item = 0; item < SDL_arraysize(items); item++) {
            entry.num = items[item];
            CHECK(!mes_search(catalog, &entry));
        }
        mes_unload(catalog);
        CHECK(tig_file_repository_remove(path));
    }
}

static void test_sector_script_dif_persistence(void)
{
    SectorScriptList list;
    SectorScriptList loaded;
    Script script = { 0 };
    script.num = 123;
    CHECK(tig_file_repository_add("."));
    CHECK(sector_script_list_init(&list));
    CHECK(sector_script_list_set(&list, &script));
    for (int pass = 0; pass < 3; pass++) {
        if (pass == 2) CHECK(sector_script_list_remove(&list));
        TigFile* stream = tig_file_fopen("sector-script-test.dif", "wb");
        CHECK(stream != NULL);
        if (stream == NULL) break;
        CHECK(sector_script_list_save_with_dif(&list, stream));
        tig_file_fclose(stream);
        CHECK(sector_script_list_is_modified(&list));
        CHECK(sector_script_list_init(&loaded));
        stream = tig_file_fopen("sector-script-test.dif", "rb");
        CHECK(stream != NULL);
        if (stream == NULL) break;
        CHECK(sector_script_list_load_with_dif(&loaded, stream));
        tig_file_fclose(stream);
        CHECK(loaded.scr.num == (pass == 2 ? 0 : 123));
        CHECK(sector_script_list_is_modified(&loaded));
    }
    CHECK(tig_file_repository_remove("."));
}

static void test_terrain_encoding(void)
{
    for (int from = 0; from < 32; from++) {
        for (int to = 0; to < 32; to++) {
            for (int mask = 0; mask < 16; mask++) {
                for (int variant = 0; variant < 4; variant++) {
                    uint16_t value = terrain_encode(from, to, mask, variant);
                    CHECK(((value >> 11) & 31) == from);
                    CHECK(((value >> 6) & 31) == to);
                    CHECK(((value >> 2) & 15) == mask);
                    CHECK((value & 3) == variant);
                }
            }
        }
    }
    CHECK(terrain_encode(-1, -1, -1, -1) == 0);
    CHECK(terrain_encode(32, 32, 16, 4) == 0xFFFF);
}

static void test_lighting_reduction(void)
{
    CHECK(lighting_adjust_channel(0, 25) == 64);
    CHECK(lighting_adjust_channel(0, 50) == 128);
    CHECK(lighting_adjust_channel(64, 25) == 112);
    CHECK(lighting_adjust_channel(64, 50) == 160);
    for (int channel = 0; channel <= 255; channel++) {
        CHECK(lighting_adjust_channel(channel, 0) == channel);
        CHECK(lighting_adjust_channel(channel, 25) >= channel);
        CHECK(lighting_adjust_channel(channel, 50) >= lighting_adjust_channel(channel, 25));
        CHECK(lighting_adjust_channel(channel, 50) <= 255);
        CHECK(lighting_adjust_channel(channel, 99) == channel);
    }
    CHECK(lighting_adjust_channel(255, 25) == 255);
    CHECK(lighting_adjust_channel(255, 50) == 255);
    CHECK(lighting_reduction_percent(-1) == 0);
}

static void test_stable_order_and_save_ids(void)
{
    CHECK(broadcast_follower_shortcut_command(0) == BROADCAST_CMD_TYPE_WALK_HERE);
    CHECK(broadcast_follower_shortcut_command(1) == BROADCAST_CMD_TYPE_ATTACK);
    CHECK(broadcast_follower_shortcut_command(2) == BROADCAST_CMD_TYPE_STAY_CLOSE);
    CHECK(broadcast_follower_shortcut_command(3) == BROADCAST_CMD_TYPE_SPREAD_OUT);
    CHECK(broadcast_follower_shortcut_command(4) == BROADCAST_CMD_TYPE_BACK_OFF);
    CHECK(broadcast_follower_shortcut_command(5) == BROADCAST_CMD_TYPE_MOVE);
    CHECK(broadcast_follower_shortcut_command(6) == BROADCAST_CMD_TYPE_NONE);
    CHECK(broadcast_follower_shortcut_command(UINT_MAX) == BROADCAST_CMD_TYPE_NONE);

    const char* names[] = { "SlotAuto", "SlotAutoAuto-Save",
        "SlotAutoSauvegarde automatique", "slotautoRussian", "Slot0000Manual",
        "SlotAUTOCustom title", "Slot0001SlotAuto", "SlotAut", "", NULL };
    for (int i = 0; i < SDL_arraysize(names); i++) {
        CHECK(save_slot_is_quicksave(names[i]) == (i < 4 || i == 5));
        for (int j = 0; j < SDL_arraysize(names); j++) {
            CHECK(save_slot_compare_priority(names[i], names[j])
                == -save_slot_compare_priority(names[j], names[i]));
        }
        CHECK(save_slot_compare_priority(names[i], names[i]) == 0);
    }
    CHECK(save_slot_compare_priority("SlotAutoFR", "Slot0000Manual") < 0);
    CHECK(save_slot_compare_priority("SlotAutoFR", "SlotAutoEN") == 0);
}

static void test_localized_catalog_fallback(void)
{
    CHECK(SDL_CreateDirectory("mes"));
    CHECK(SDL_CreateDirectory("mes/ce_fallback"));
    FILE* file = fopen("mes/savegame.mes", "wb");
    CHECK(file != NULL);
    if (file == NULL) return;
    fputs("{100}{Voyage local}\n", file);
    fclose(file);
    file = fopen("mes/ce_fallback/savegame.mes", "wb");
    CHECK(file != NULL);
    if (file == NULL) return;
    fputs("{100}{Travel fallback}\n{101}{Sleep fallback}\n{102}{Quick fallback}\n", file);
    fclose(file);
    CHECK(tig_file_repository_add("."));
    CHECK(strcmp(mes_localized_get("savegame", 100), "Voyage local") == 0);
    CHECK(strcmp(mes_localized_get("savegame", 101), "Sleep fallback") == 0);
    CHECK(strcmp(mes_fallback_get("savegame", 100), "Travel fallback") == 0);
    CHECK(strcmp(mes_localized_get("savegame", 9999), "?") == 0);
    // A cleanup/reload must not retain handles to the previous catalogs.
    mes_localized_exit();
    CHECK(strcmp(mes_localized_get("savegame", 102), "Quick fallback") == 0);
    mes_localized_exit();
    CHECK(tig_file_repository_remove("."));
}

static void test_mes_duplicate_groups(void)
{
    CHECK(SDL_CreateDirectory("mes"));
    FILE* file = fopen("mes/duplicates-test.mes", "wb");
    CHECK(file != NULL);
    if (file == NULL) return;
    fputs("{30}{last}\n{10}{first}\n{20}{same}\n{20}{same}\n{20}{same}\n{20}{same}\n{15}{middle}\n", file);
    fclose(file);
    CHECK(tig_file_repository_add("."));
    mes_file_handle_t catalog;
    CHECK(mes_load("mes\\duplicates-test.mes", &catalog));
    CHECK(mes_entries_count(catalog) == 7);
    MesFileEntry entry = { 20, NULL };
    CHECK(mes_search(catalog, &entry) && strcmp(entry.str, "same") == 0);
    entry.num = 15;
    CHECK(mes_search(catalog, &entry) && strcmp(entry.str, "middle") == 0);
    entry.num = 30;
    CHECK(mes_search(catalog, &entry) && strcmp(entry.str, "last") == 0);
    entry.num = 99;
    CHECK(!mes_search(catalog, &entry));
    CHECK(mes_unload(catalog));
    CHECK(tig_file_repository_remove("."));
    remove("mes/duplicates-test.mes");
}

static void test_large_floating_dialogue(const char* path)
{
    CHECK(TTF_Init());
    TTF_Font* font = TTF_OpenFont(path, 28.0f);
    CHECK(font != NULL);
    if (font != NULL) {
        const char* text = "Je pense que nous devrions essayer de retrouver ce Schuyler & Fils a Tarante. La-bas, ils pourront peut-etre nous dire a qui appartient cet objet.";
        int width, height;
        text_bubble_dimensions(28.0f / 12.0f, 1280, 720, &width, &height);
        SDL_Color white = { 255, 255, 255, 255 };
        SDL_Surface* narrow = TTF_RenderText_Blended_Wrapped(font, text, 0, white, 200);
        SDL_Surface* wide = TTF_RenderText_Blended_Wrapped(font, text, 0, white, width);
        CHECK(narrow != NULL && wide != NULL);
        if (narrow != NULL && wide != NULL) {
            CHECK(wide->h < narrow->h);
            CHECK(wide->h <= height);
        }
        SDL_DestroySurface(narrow);
        SDL_DestroySurface(wide);
        TTF_CloseFont(font);
    }
    TTF_Quit();
}

static void test_text_bubble_dimensions(void)
{
    int width, height;
    text_bubble_dimensions(1.0f, 1280, 720, &width, &height);
    CHECK(width == 320 && height == 200);
    text_bubble_dimensions(2.0f, 1280, 720, &width, &height);
    CHECK(width == 640 && height == 400);
    text_bubble_dimensions(1.5f, 1280, 720, &width, &height);
    CHECK(width == 480 && height == 300);
    text_bubble_dimensions(0.5f, 1280, 720, &width, &height);
    CHECK(width == 320);
    text_bubble_dimensions(6.0f, 800, 600, &width, &height);
    CHECK(width <= 600 && height <= 580);
    text_bubble_dimensions(6.0f, 640, 480, &width, &height);
    CHECK(width <= 480 && height <= 460);
}

static void test_random_encounter_modes(void)
{
    const int frequencies[] = { 0, 1, 5, 25, 100, 500 };
    for (int i = 0; i < SDL_arraysize(frequencies); i++) {
        int normal = 0, rare = 0;
        int threshold = encounter_frequency_threshold(frequencies[i]);
        for (int roll = 1; roll <= encounter_frequency_roll_max(RANDOM_ENCOUNTERS_NORMAL); roll++) normal += roll <= threshold;
        for (int roll = 1; roll <= encounter_frequency_roll_max(RANDOM_ENCOUNTERS_LESS_FREQUENT); roll++) rare += roll <= threshold;
        CHECK(normal == rare);
        CHECK(encounter_frequency_roll_max(RANDOM_ENCOUNTERS_NORMAL) == 100);
        CHECK(encounter_frequency_roll_max(RANDOM_ENCOUNTERS_LESS_FREQUENT) == 270);
    }
    CHECK(encounter_frequency_roll_max(RANDOM_ENCOUNTERS_INTERESTING) == 100);
    CHECK(encounter_frequency_roll_max(99) == 100);
    CHECK(encounter_frequency_threshold(-1) == 0);
    CHECK(encounter_group_is_trivial(20, 40, 5, 10));
    CHECK(!encounter_group_is_trivial(5, 40, 1, 2));
    CHECK(!encounter_group_is_trivial(20, 40, 11, 11));
    CHECK(!encounter_group_is_trivial(20, 40, 5, 25));
    CHECK(!encounter_group_is_trivial(20, 40, 0, 0));
    CHECK(encounter_group_is_trivial(20, 40, 10, 20));
}

static void test_language_archives(void)
{
    const char* archives[] = { "Arcanum_en.dat", "Arcanum_fr.dat", "Arcanum_es.dat", "Arcanum_it.dat", "Arcanum_ru.dat" };
    for (int language = 0; language < GAME_LANGUAGE_COUNT; language++) {
        CHECK(language_from_name(language_name((GameLanguage)language)) == language);
        CHECK(language_from_name(language_suffix((GameLanguage)language)) == language);
        CHECK(language_archive_is_enabled("Arcanum1.dat", (GameLanguage)language));
        CHECK(language_archive_load_phase("ArcanumZ.dat", (GameLanguage)language) == 0);
        for (int archive = 0; archive < GAME_LANGUAGE_COUNT; archive++) {
            CHECK(language_archive_language(archives[archive]) == archive);
            CHECK(language_archive_is_enabled(archives[archive], (GameLanguage)language) == (archive == language));
            CHECK(language_archive_load_phase(archives[archive], (GameLanguage)language) == (archive == language ? 1 : -1));
        }
    }
    CHECK(language_from_name(NULL) == GAME_LANGUAGE_ENGLISH);
    CHECK(language_from_name("unsupported") == GAME_LANGUAGE_ENGLISH);
    CHECK(language_from_name("FRENCH") == GAME_LANGUAGE_FRENCH);
    CHECK(language_archive_language("HighRes\\Files\\Arcanum_FR.DAT") == GAME_LANGUAGE_FRENCH);
    CHECK(language_archive_language("Modules/Arcanum_ru.dat") == GAME_LANGUAGE_RUSSIAN);
    CHECK(language_archive_language("arcanum_fr_extra.dat") == GAME_LANGUAGE_NONE);
    CHECK(language_archive_language("arcanum_fr.dat.bak") == GAME_LANGUAGE_NONE);
    CHECK(language_archive_language("arcanum_french.dat") == GAME_LANGUAGE_NONE);
    CHECK(language_archive_load_phase("translations_fr.dat", GAME_LANGUAGE_FRENCH) == 1);
    CHECK(language_archive_load_phase("translations_fr.dat", GAME_LANGUAGE_ENGLISH) == -1);
    CHECK(language_archive_load_phase("tig.dat", GAME_LANGUAGE_ENGLISH) == -1);
    CHECK(language_archive_load_phase("unins000.dat", GAME_LANGUAGE_ENGLISH) == -1);
    CHECK(language_archive_load_phase("ArcanumZ.dat", GAME_LANGUAGE_FRENCH)
        < language_archive_load_phase("Arcanum_fr.dat", GAME_LANGUAGE_FRENCH));
}

static int test_art_path(tig_art_id_t aid, char* path, size_t size)
{
    if (tig_art_num_get(aid) == 4092) {
        SDL_snprintf(path, size, "%s/PageTurn_U.ART", button_art_directory);
        return TIG_OK;
    }
    SDL_snprintf(path, size, "%s/OptionsMenu%sBtn.art", button_art_directory,
        tig_art_num_get(aid) == 4094 ? "Left" : "Right");
    return TIG_OK;
}

static tig_art_id_t test_art_normalize(tig_art_id_t aid)
{
    return tig_art_id_frame_set(aid, 0);
}

static const char* worldmap_font_art;

static int test_worldmap_font_path(tig_art_id_t aid, char* path, size_t size)
{
    (void)aid;
    SDL_strlcpy(path, worldmap_font_art, size);
    return TIG_OK;
}

static void test_worldmap_accents(const char* path)
{
    worldmap_font_art = path;
    TigInitInfo info = { 0 };
    info.bpp = 32;
    info.art_file_path_resolver = test_worldmap_font_path;
    info.art_id_reset_func = test_art_normalize;
    CHECK(tig_palette_init(&info) == TIG_OK);
    CHECK(tig_art_init(&info) == TIG_OK);
    const unsigned char accents[] = { 0xE8, 0xE9, 0xEA, 0xEB };
    tig_art_id_t plain;
    TigArtAnimData anim;
    tig_art_interface_id_create(840, 'e' - 31, 0, 0, &plain);
    CHECK(tig_art_anim_data(plain, &anim) == TIG_OK);
    for (int i = 0; i < SDL_arraysize(accents); i++) {
        tig_art_id_t accented;
        TigArtFrameData data;
        tig_art_interface_id_create(840, accents[i] - 31, 0, 0, &accented);
        CHECK(tig_art_frame_data(accented, &data) == TIG_OK);
        CHECK(data.width == 7 && data.height == 21);
        int marks = 0;
        for (int y = 0; y < data.height; y++) {
            for (int x = 0; x < data.width; x++) {
                unsigned int base, pixel;
                CHECK(sub_502E50(plain, x, y, &base) == TIG_OK);
                CHECK(sub_502E50(accented, x, y, &pixel) == TIG_OK);
                if (y >= 8) CHECK(pixel == base);
                else if (pixel != anim.color_key) marks++;
            }
        }
        CHECK(marks > 0);
    }
    tig_art_exit();
    tig_palette_exit();
}

static int test_black_pixels(tig_window_handle_t window)
{
    TigVideoBuffer* buffer;
    CHECK(tig_window_vbid_get(window, &buffer) == TIG_OK);
    CHECK(tig_video_buffer_lock(buffer) == TIG_OK);
    TigVideoBufferData data;
    CHECK(tig_video_buffer_data(buffer, &data) == TIG_OK);
    int count = 0;
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 99; x++) {
            Uint32 color = *(Uint32*)((Uint8*)data.pixels + y * data.pitch + x * 4);
            if (color == tig_color_make(0, 0, 0)) count++;
        }
    }
    tig_video_buffer_unlock(buffer);
    return count;
}

static void test_plain_stock_label(const char* path)
{
    // The checked-in bitmap font is 21 pixels high. In a 24-pixel cell the
    // old padding leaves 18 pixels and silently suppresses every glyph;
    // plain label padding leaves 22. The game's 15/18-pixel stock case is the
    // same failure, tested here without depending on installed game assets.
    worldmap_font_art = path;
    TigInitInfo info = { 0 };
    info.bpp = 32;
    info.width = 99;
    info.height = 24;
    info.art_file_path_resolver = test_worldmap_font_path;
    info.art_id_reset_func = test_art_normalize;
    CHECK(tig_palette_init(&info) == TIG_OK);
    CHECK(tig_art_init(&info) == TIG_OK);
    CHECK(tig_rect_init(&info) == TIG_OK);
    CHECK(tig_window_init(&info) == TIG_OK);
    TigFont font = { 0 };
    font.color = tig_color_make(0, 0, 0);
    tig_art_interface_id_create(840, 0, 0, 0, &font.art_id);
    tig_font_handle_t font_handle;
    tig_font_create(&font, &font_handle);
    TigWindowData data = { 0 };
    data.rect = (TigRect){ 0, 0, 99, 24 };
    tig_window_handle_t window;
    CHECK(tig_window_create(&data, &window) == TIG_OK);
    ui_search_bar_cell(window, data.rect, font_handle, "3/2", false);
    CHECK(test_black_pixels(window) == 0);
    tig_color_t parchment = tig_color_make(220, 210, 190);
    CHECK(tig_window_fill(window, &data.rect, parchment) == TIG_OK);
    ui_search_bar_label(window, data.rect, font_handle, "3/2");
    CHECK(test_black_pixels(window) > 0);
    TigVideoBuffer* buffer;
    CHECK(tig_window_vbid_get(window, &buffer) == TIG_OK);
    CHECK(tig_video_buffer_lock(buffer) == TIG_OK);
    TigVideoBufferData pixels;
    CHECK(tig_video_buffer_data(buffer, &pixels) == TIG_OK);
    CHECK(*(Uint32*)pixels.pixels == parchment); // No added border or fill.
    CHECK(*(Uint32*)((Uint8*)pixels.pixels + 2 * pixels.pitch + 2 * 4) == parchment);
    tig_video_buffer_unlock(buffer);
    tig_window_destroy(window);
    tig_font_destroy(font_handle);
    tig_window_exit();
    tig_rect_exit();
    tig_art_exit();
    tig_palette_exit();
}

static void test_button_art(const char* directory)
{
    button_art_directory = directory;
    TigInitInfo info = { 0 };
    info.bpp = 32;
    info.art_file_path_resolver = test_art_path;
    info.art_id_reset_func = test_art_normalize;
    CHECK(tig_palette_init(&info) == TIG_OK);
    CHECK(tig_art_init(&info) == TIG_OK);
    tig_art_id_t index_button;
    TigArtAnimData index_anim;
    CHECK(tig_art_interface_id_create(4092, 0, 0, 0, &index_button) == TIG_OK);
    CHECK(tig_art_anim_data(index_button, &index_anim) == TIG_OK);
    CHECK(index_anim.num_frames == 3);
    for (int frame = 0; frame < 3; frame++) {
        TigArtFrameData data;
        CHECK(tig_art_frame_data(tig_art_id_frame_set(index_button, frame), &data) == TIG_OK);
        CHECK(data.width == 22 && data.height == 18);
    }
    for (int side = 0; side < 2; side++) {
        tig_art_id_t ids[3];
        TigArtAnimData anim;
        int pressed_difference = 0;
        int hover_difference = 0;
        int pressed_hover_difference = 0;
        for (int frame = 0; frame < 3; frame++) {
            TigArtFrameData data;
            CHECK(tig_art_interface_id_create(4094 + side, frame, 0, 0, &ids[frame]) == TIG_OK);
            int rc = tig_art_frame_data(ids[frame], &data);
            CHECK(rc == TIG_OK);
            if (rc != TIG_OK) goto finish;
            CHECK(data.width == 40 && data.height == 40);
        }
        CHECK(tig_art_anim_data(ids[0], &anim) == TIG_OK);
        CHECK(anim.num_frames == 3);
        for (int y = 0; y < 40; y++) {
            for (int x = 0; x < 40; x++) {
                unsigned int normal, pressed, hover;
                CHECK(sub_502E50(ids[0], x, y, &normal) == TIG_OK);
                CHECK(sub_502E50(ids[1], x, y, &pressed) == TIG_OK);
                CHECK(sub_502E50(ids[2], x, y, &hover) == TIG_OK);
                double dx = x + 0.5 - 20;
                double dy = y + 0.5 - 20;
                bool opaque = dx * dx + dy * dy <= 400;
                CHECK((normal != anim.color_key) == opaque);
                CHECK((pressed != anim.color_key) == opaque);
                CHECK((hover != anim.color_key) == opaque);
                if (normal != pressed) pressed_difference++;
                if (normal != hover) hover_difference++;
                if (pressed != hover) pressed_hover_difference++;
            }
        }
        CHECK(pressed_difference > 0);
        CHECK(hover_difference > 0);
        CHECK(pressed_hover_difference > 0);
    }
finish:
    tig_art_exit();
    tig_palette_exit();
}

void gamelib_invalidate_rect(TigRect* rect)
{
    (void)rect;
    invalidations++;
}

// Object-valued settings are outside the scope of these tests.
void objid_id_to_str(char* buffer, ObjectID oid)
{
    (void)oid;
    buffer[0] = '\0';
}

bool objid_id_from_str(ObjectID* oid, const char* str)
{
    (void)str;
    memset(oid, 0, sizeof(*oid));
    return false;
}

static void test_ini_round_trip(void)
{
    FILE* stream = fopen("arcanum.ini", "wt");
    CHECK(stream != NULL);
    if (stream == NULL) return;
    fputs("[global]\nlanguage=french\n[video]\ndialog_font=2\nscroll_using_borders=0\ncamera_follows_player=1\nvsync_mode=2\nzoom_min=0.5\nzoom_max=1.75\nzoom_enabled=1\nzoom_filter=nearest\n"
          "[ui]\nhud_stage=2\ndialogue_camera_mode=2\nschematic_color_available=#407090\nschematic_color_missing=160,50,60\nschematic_opacity_available=30\nschematic_opacity_missing=30\n"
          "dialogue_camera_zoom_to_fit=1\ndialogue_camera_tween_back=1\ntranslucent_black_ui=1\n"
          "dialogue_option_numbers=1\ndialogue_emote_dim=1\ndialogue_emote_dim_choices=1\ndialogue_emote_dim_percent=60\nfloating_shadow_opacity=70\n"
          "[game]\nautosave_travel=0\nautosave_sleep=1\naction_tooltips=1\nrandom_encounters=2\nlighting=50\n[bindings]\nzoom_reset=Keypad 0\ncycle_hud=TAB\ninventory=J\n"
          "[fonts]\nmodern_fonts=1\nmodern_font_size=22\nlogbook_font=1\n"
          "[xbox_controller]\ncursor_speed=850\nmouse_left=b|rt\nmouse_right=a|lt\nquick_save=none\n", stream);
    fclose(stream);
    settings_init(&settings, "arcanum.ini");
    settings_load(&settings);
    controller_settings_register(&settings);
    input_bindings_init();
    CHECK(settings_get_value(&settings, "controller_enabled") == 0);
    CHECK(modern_font_int_for(SCHEMATIC_OPACITY_AVAILABLE_KEY, 100, 0, 100) == 30);
    CHECK(modern_font_int_for(SCHEMATIC_OPACITY_MISSING_KEY, 100, 0, 100) == 30);
    settings_set_value(&settings, SCHEMATIC_OPACITY_MISSING_KEY, -10);
    CHECK(modern_font_int_for(SCHEMATIC_OPACITY_MISSING_KEY, 100, 0, 100) == 0);
    settings_set_value(&settings, SCHEMATIC_OPACITY_MISSING_KEY, 150);
    CHECK(modern_font_int_for(SCHEMATIC_OPACITY_MISSING_KEY, 100, 0, 100) == 100);
    settings_set_value(&settings, SCHEMATIC_OPACITY_MISSING_KEY, 30);
    tig_color_t schematic_color;
    CHECK(modern_font_color_read(SCHEMATIC_COLOR_AVAILABLE_KEY, &schematic_color));
    CHECK(schematic_color == tig_color_make(64, 112, 144));
    CHECK(modern_font_color_read(SCHEMATIC_COLOR_MISSING_KEY, &schematic_color));
    CHECK(schematic_color == tig_color_make(160, 50, 60));
    const char* invalid_colors[] = { "original", "", "badval", "#FF000g", "256,0,0", "12,34,56junk" };
    for (int i = 0; i < SDL_arraysize(invalid_colors); i++) {
        settings_set_str_value(&settings, SCHEMATIC_COLOR_MISSING_KEY, invalid_colors[i]);
        schematic_color = tig_color_make(1, 2, 3);
        CHECK(!modern_font_color_read(SCHEMATIC_COLOR_MISSING_KEY, &schematic_color));
        CHECK(schematic_color == tig_color_make(1, 2, 3));
    }
    settings_set_str_value(&settings, SCHEMATIC_COLOR_MISSING_KEY, "160,50,60");
    settings_set_value(&settings, "controller_enabled", 1);
    CHECK(settings_get_value(&settings, "controller_cursor_speed") == 850);
    CHECK(strcmp(settings_get_str_value(&settings, "controller_mouse_left"), "b|rt") == 0);
    CHECK(strcmp(settings_get_str_value(&settings, "controller_quick_save"), "none") == 0);
    CHECK(input_binding_named_scancode("inventory") == SDL_SCANCODE_J);
    CHECK(input_binding_named_scancode("RETURN") == SDL_SCANCODE_RETURN);
    TigControllerConfig controller;
    controller_config_build(&settings, &controller);
    CHECK(controller.buttons[0][SDL_GAMEPAD_BUTTON_EAST] == TIG_CONTROLLER_MOUSE_LEFT);
    CHECK(controller.buttons[0][SDL_GAMEPAD_BUTTON_SOUTH] == TIG_CONTROLLER_MOUSE_RIGHT);
    CHECK(controller.triggers[0] == TIG_CONTROLLER_MOUSE_RIGHT && controller.triggers[1] == TIG_CONTROLLER_MOUSE_LEFT);
    CHECK(controller.buttons[0][SDL_GAMEPAD_BUTTON_DPAD_UP] == SDL_SCANCODE_J);
    CHECK(controller.buttons[1][SDL_GAMEPAD_BUTTON_NORTH] == SDL_SCANCODE_R);
    CHECK(controller.buttons[1][SDL_GAMEPAD_BUTTON_WEST] == SDL_SCANCODE_EQUALS);
    CHECK(controller.buttons[1][SDL_GAMEPAD_BUTTON_EAST] == SDL_SCANCODE_MINUS);
    settings_set_str_value(&settings, "controller_inventory", "  DPAD_UP | lb+a  ");
    controller_config_build(&settings, &controller);
    CHECK(controller.buttons[1][SDL_GAMEPAD_BUTTON_SOUTH] == SDL_SCANCODE_J);
    settings_set_str_value(&settings, "controller_inventory", "none");
    controller_config_build(&settings, &controller);
    CHECK(controller.buttons[0][SDL_GAMEPAD_BUTTON_DPAD_UP] == 0);
    settings_set_str_value(&settings, "controller_inventory", "dpad_up");
    settings_register(&settings, LANGUAGE_KEY, "english", NULL);
    settings_register(&settings, PERFORMANCE_LOGGING_KEY, "0", NULL);
    settings_register(&settings, BENCHMARK_AUTORUN_KEY, "0", NULL);
    settings_register(&settings, SCHEMATIC_TOOLBAR_KEY, "1", NULL);
    settings_register(&settings, LOGBOOK_TOOLBAR_KEY, "1", NULL);
    settings_register(&settings, WORLDMAP_TOOLBAR_KEY, "1", NULL);
    CHECK(settings_get_value(&settings, SCHEMATIC_TOOLBAR_KEY) == 1);
    CHECK(settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY) == 1);
    CHECK(settings_get_value(&settings, WORLDMAP_TOOLBAR_KEY) == 1);
    settings_set_value(&settings, WORLDMAP_TOOLBAR_KEY, 0);
    settings_set_value(&settings, SCHEMATIC_TOOLBAR_KEY, 0);
    CHECK(settings_get_value(&settings, BENCHMARK_AUTORUN_KEY) == 0);
    settings_set_value(&settings, BENCHMARK_AUTORUN_KEY, 1);
    CHECK(settings_get_value(&settings, PERFORMANCE_LOGGING_KEY) == 0);
    settings_set_value(&settings, PERFORMANCE_LOGGING_KEY, 1);
    CHECK(strcmp(settings_get_str_value(&settings, LANGUAGE_KEY), "french") == 0);
    CHECK(settings_get_value(&settings, VSYNC_MODE_KEY) == 2);
    CHECK(settings_get_value(&settings, "camera follows player") == 1);
    CHECK(settings_get_value(&settings, AUTOSAVE_TRAVEL_KEY) == 0);
    CHECK(settings_get_value(&settings, AUTOSAVE_SLEEP_KEY) == 1);
    CHECK(settings_get_value(&settings, ACTION_TOOLTIPS_KEY) == 1);
    CHECK(settings_get_value(&settings, RANDOM_ENCOUNTERS_KEY) == 2);
    CHECK(settings_get_value(&settings, LIGHTING_KEY) == 50);
    CHECK(strcmp(settings_get_str_value(&settings, ZOOM_FILTER_KEY), "nearest") == 0);
    settings_set_value(&settings, VSYNC_MODE_KEY, 1);
    settings_save(&settings);
    CHECK((settings.flags & SETTINGS_CHANGED) == 0);
    stream = fopen("arcanum.ini", "rt");
    CHECK(stream != NULL);
    if (stream != NULL) {
        char line[256];
        bool in_ui = false;
        bool in_controller = false;
        int controller_keys = 0;
        bool in_fonts = false, in_video = false;
        int font_keys = 0;
        int toolbar_keys = 0;
        while (fgets(line, sizeof(line), stream) != NULL) {
            CHECK(line[0] != ';' && line[0] != '#');
            if (line[0] == '[') in_ui = strcmp(line, "[ui]\n") == 0;
            if (strncmp(line, "schematic_color_", 16) == 0) CHECK(in_ui);
            if (strncmp(line, "schematic_opacity_", 18) == 0) CHECK(in_ui);
            if (line[0] == '[') in_controller = strcmp(line, "[xbox_controller]\n") == 0;
            if (line[0] == '[') in_fonts = strcmp(line, "[fonts]\n") == 0;
            if (line[0] == '[') in_video = strcmp(line, "[video]\n") == 0;
            if (strncmp(line, "modern_fonts=", 13) == 0 || strncmp(line, "modern_font_size=", 17) == 0
                || strncmp(line, "logbook_font=", 13) == 0) { CHECK(in_fonts); font_keys++; }
            if (strncmp(line, "lighting=", 9) == 0) CHECK(in_video);
            if (strncmp(line, "dialog_font=", 12) == 0 || strncmp(line, "floating_shadow_opacity=", 24) == 0) CHECK(in_fonts);
            if (strncmp(line, "cursor_speed=", 13) == 0 || strncmp(line, "enabled=", 8) == 0) {
                CHECK(in_controller);
                controller_keys++;
            }
            if (strncmp(line, "schematic_toolbar=", 18) == 0 || strncmp(line, "logbook_toolbar=", 16) == 0
                || strncmp(line, "worldmap_toolbar=", 17) == 0) {
                CHECK(in_ui);
                toolbar_keys++;
            }
        }
        CHECK(toolbar_keys == 3);
        CHECK(controller_keys == 2);
        CHECK(font_keys == 3);
        fclose(stream);
    }
    settings_exit(&settings);
    settings_init(&settings, "arcanum.ini");
    settings_load(&settings);
    CHECK(settings_get_value(&settings, VSYNC_MODE_KEY) == 1);
    CHECK(settings_get_value(&settings, SCHEMATIC_TOOLBAR_KEY) == 0);
    CHECK(settings_get_value(&settings, WORLDMAP_TOOLBAR_KEY) == 0);
    CHECK(strcmp(settings_get_str_value(&settings, SCHEMATIC_COLOR_AVAILABLE_KEY), "#407090") == 0);
    CHECK(strcmp(settings_get_str_value(&settings, SCHEMATIC_COLOR_MISSING_KEY), "160,50,60") == 0);
    CHECK(settings_get_value(&settings, SCHEMATIC_OPACITY_AVAILABLE_KEY) == 30);
    CHECK(settings_get_value(&settings, SCHEMATIC_OPACITY_MISSING_KEY) == 30);
    CHECK(settings_get_value(&settings, "controller_enabled") == 1);
    CHECK(settings_get_value(&settings, "controller_cursor_speed") == 850);
    CHECK(strcmp(settings_get_str_value(&settings, "controller_mouse_left"), "b|rt") == 0);
    CHECK(settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY) == 1);
    CHECK(settings_get_value(&settings, PERFORMANCE_LOGGING_KEY) == 1);
    CHECK(settings_get_value(&settings, BENCHMARK_AUTORUN_KEY) == 1);
    CHECK(strcmp(settings_get_str_value(&settings, LANGUAGE_KEY), "french") == 0);
    CHECK(settings_get_value(&settings, SCROLL_USING_BORDERS_KEY) == 0);
    CHECK(settings_get_value(&settings, RANDOM_ENCOUNTERS_KEY) == 2);
    CHECK(settings_get_value(&settings, LIGHTING_KEY) == 50);
    CHECK(settings_get_value(&settings, "modern_fonts") == 1);
    CHECK(settings_get_value(&settings, "modern_font_size") == 22);
    CHECK(settings_get_value(&settings, LOGBOOK_FONT_KEY) == 1);
    CHECK(settings_get_value(&settings, DIALOG_FONT_KEY) == 2);
    CHECK(settings_get_value(&settings, FLOATING_SHADOW_OPACITY_KEY) == 70);
    CHECK(settings_get_value(&settings, ACTION_TOOLTIPS_KEY) == 1);
    CHECK(settings_get_value(&settings, "hud_stage") == 2);
    CHECK(settings_get_value(&settings, "dialogue_camera_tween_back") == 1);
    CHECK(settings_get_value(&settings, "dialogue_emote_dim_percent") == 60);
    CHECK(strcmp(settings_get_str_value(&settings, "zoom_reset"), "Keypad 0") == 0);
}

static void test_startup_resolution(void)
{
    CHECK(SDL_CreateDirectory("HighRes"));
    FILE* stream = fopen("HighRes/config.ini", "wt");
    CHECK(stream != NULL);
    if (stream == NULL) return;
    fputs("Width=1280\nHeight=720\n", stream);
    fclose(stream);
    const char* values[] = { "1920x1080", "0x0", "640x480", "99999x99999", "1920x1080junk" };
    for (int i = 0; i < SDL_arraysize(values); i++) {
        stream = fopen("arcanum.ini", "wt");
        CHECK(stream != NULL);
        if (stream == NULL) return;
        fprintf(stream, "[video]\nresolution=%s\n", values[i]);
        fclose(stream);
        highres_config_load();
        CHECK(highres_config_get()->width == (i == 0 ? 1920 : 1280));
        CHECK(highres_config_get()->height == (i == 0 ? 1080 : 720));
    }
    stream = fopen("arcanum.ini", "wt");
    CHECK(stream != NULL);
    if (stream != NULL) {
        fputs("[ui]\nresolution=1920x1080\n", stream);
        fclose(stream);
    }
    highres_config_load();
    CHECK(highres_config_get()->width == 1280);
    CHECK(highres_config_get()->height == 720);
}

static void test_bindings(void)
{
    TigMessage message = { 0 };
    message.type = TIG_MESSAGE_KEYBOARD;
    input_bindings_init();
    message.data.keyboard.scancode = SDL_SCANCODE_EQUALS;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_ZOOM_IN, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_KP_PLUS;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_ZOOM_IN, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_KP_MINUS;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_ZOOM_OUT, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_KP_0;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_ZOOM_RESET, &message));
    settings_set_str_value(&settings, "cycle_hud", "F11");
    input_bindings_init();
    message.data.keyboard.scancode = SDL_SCANCODE_TAB;
    CHECK(!input_binding_matches_keyboard(INPUT_ACTION_CYCLE_HUD, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_F11;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_CYCLE_HUD, &message));
}

static void test_panel_navigation_bindings(void)
{
    const SDL_Scancode defaults[] = { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
        SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT };
    TigMessage message = { 0 };
    message.type = TIG_MESSAGE_KEYBOARD;
    input_bindings_init();
    for (int i = 0; i < 8; i++) {
        message.data.keyboard.scancode = defaults[i % 4];
        CHECK(input_binding_matches_keyboard((InputAction)(INPUT_ACTION_LOGBOOK_PREVIOUS + i), &message));
    }
    settings_set_str_value(&settings, "logbook_previous", "PAGEUP");
    settings_set_str_value(&settings, "logbook_next", "NONE");
    settings_set_str_value(&settings, "schematics_page_next", "F6|PAGEDOWN");
    settings_save(&settings);
    settings_exit(&settings);
    settings_init(&settings, "arcanum.ini");
    settings_load(&settings);
    input_bindings_init();
    message.data.keyboard.scancode = SDL_SCANCODE_PAGEUP;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_LOGBOOK_PREVIOUS, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_UP;
    CHECK(!input_binding_matches_keyboard(INPUT_ACTION_LOGBOOK_PREVIOUS, &message));
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_SCHEMATICS_PREVIOUS, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_DOWN;
    CHECK(!input_binding_matches_keyboard(INPUT_ACTION_LOGBOOK_NEXT, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_F6;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_SCHEMATICS_PAGE_NEXT, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_PAGEDOWN;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_SCHEMATICS_PAGE_NEXT, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_RIGHT;
    CHECK(!input_binding_matches_keyboard(INPUT_ACTION_SCHEMATICS_PAGE_NEXT, &message));
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_LOGBOOK_PAGE_NEXT, &message));
}

static void test_panel_navigation_mouse(void)
{
    const char* keys[] = { "logbook_previous", "logbook_next", "logbook_page_previous", "logbook_page_next",
        "schematics_previous", "schematics_next", "schematics_page_previous", "schematics_page_next" };
    const char* keyboard[] = { "UP", "DOWN", "LEFT", "RIGHT" };
    const SDL_Scancode scancodes[] = { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT };
    const char* mouse[] = { "PREVIOUS_MOUSE", "NEXT_MOUSE", "MOUSE4", "MOUSE5" };
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) settings_set_str_value(&settings, keys[j], "NONE");
        char binding[64];
        snprintf(binding, sizeof(binding), "%s|%s", keyboard[i % 4], mouse[i % 4]);
        settings_set_str_value(&settings, keys[i], binding);
        input_bindings_init();
        InputAction action = (InputAction)(INPUT_ACTION_LOGBOOK_PREVIOUS + i);
        TigMessage message = { 0 };
        message.type = TIG_MESSAGE_KEYBOARD;
        message.data.keyboard.scancode = scancodes[i % 4];
        message.data.keyboard.pressed = true;
        CHECK(input_binding_navigation_event(action, &message) == INPUT_NAVIGATION_CONSUME);
        message.data.keyboard.pressed = false;
        CHECK(input_binding_navigation_event(action, &message) == INPUT_NAVIGATION_ACTIVATE);
        message.type = TIG_MESSAGE_MOUSE;
        message.data.mouse.repeat = false;
        message.data.mouse.event = i % 2 == 0 ? TIG_MESSAGE_MOUSE_X1_BUTTON_DOWN : TIG_MESSAGE_MOUSE_X2_BUTTON_DOWN;
        CHECK(input_binding_navigation_event(action, &message) == INPUT_NAVIGATION_ACTIVATE);
        for (int j = 0; j < 8; j++) {
            if (i != j) CHECK(input_binding_navigation_event((InputAction)(INPUT_ACTION_LOGBOOK_PREVIOUS + j), &message) == INPUT_NAVIGATION_NONE);
        }
        message.data.mouse.repeat = true;
        CHECK(input_binding_navigation_event(action, &message) == INPUT_NAVIGATION_CONSUME);
        message.data.mouse.repeat = false;
        message.data.mouse.event = i % 2 == 0 ? TIG_MESSAGE_MOUSE_X1_BUTTON_UP : TIG_MESSAGE_MOUSE_X2_BUTTON_UP;
        CHECK(input_binding_navigation_event(action, &message) == INPUT_NAVIGATION_CONSUME);
        message.data.mouse.event = i % 2 == 0 ? TIG_MESSAGE_MOUSE_X2_BUTTON_DOWN : TIG_MESSAGE_MOUSE_X1_BUTTON_DOWN;
        CHECK(input_binding_navigation_event(action, &message) == INPUT_NAVIGATION_NONE);
        message.data.mouse.event = TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN;
        CHECK(input_binding_navigation_event(action, &message) == INPUT_NAVIGATION_NONE);
        message.data.mouse.event = TIG_MESSAGE_MOUSE_MOVE;
        CHECK(input_binding_navigation_event(action, &message) == INPUT_NAVIGATION_NONE);
        settings_set_str_value(&settings, keys[i], "NONE");
        input_bindings_init();
        message.data.mouse.event = i % 2 == 0 ? TIG_MESSAGE_MOUSE_X1_BUTTON_DOWN : TIG_MESSAGE_MOUSE_X2_BUTTON_DOWN;
        CHECK(input_binding_navigation_event(action, &message) == INPUT_NAVIGATION_NONE);
    }
}

static void test_zoom(void)
{
    iso_zoom_init();
    for (int i = 0; i < 30; i++) iso_zoom_step_out();
    CHECK(fabsf(iso_zoom_target() - 0.5f) < 0.001f);
    for (int i = 0; i < 100; i++) iso_zoom_ping();
    CHECK(!iso_zoom_is_animating());
    CHECK(fabsf(iso_zoom_current() - 0.5f) < 0.001f);
    CHECK(invalidations > 0);
    for (int i = 0; i < 30; i++) iso_zoom_step_in();
    CHECK(fabsf(iso_zoom_target() - 1.75f) < 0.001f);
    settings_set_value(&settings, ISO_ZOOM_ENABLED_KEY, 0);
    CHECK(!iso_zoom_is_available());
    iso_zoom_step_out();
    iso_zoom_set_target(0.5f);
    CHECK(iso_zoom_target() == 1.0f);
    settings_set_value(&settings, ISO_ZOOM_ENABLED_KEY, 1);
    iso_zoom_set_available(false);
    iso_zoom_step_in();
    CHECK(iso_zoom_current() == 1.0f);
    iso_zoom_set_available(true);
    settings_set_str_value(&settings, ISO_ZOOM_MIN_KEY, "0.1");
    iso_zoom_set_target(0.1f);
    CHECK(iso_zoom_target() >= 0.5f);
    settings_set_str_value(&settings, ISO_ZOOM_MAX_KEY, "nan");
    iso_zoom_step_in();
    CHECK(isfinite(iso_zoom_target()));
    settings_set_str_value(&settings, ISO_ZOOM_MAX_KEY, "99");
    for (int i = 0; i < 50; i++) iso_zoom_step_in();
    CHECK(iso_zoom_target() <= 3.0f);
    iso_zoom_reset();
    CHECK(iso_zoom_current() == 1.0f);
}

static void test_modern_brackets(const char* path, int width)
{
    if (path == NULL || path[0] == '\0') return;
    TigVideoBufferCreateInfo info = { 0 };
    info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    info.width = width;
    info.height = 160;
    TigVideoBuffer* normal = NULL;
    TigVideoBuffer* dimmed = NULL;
    CHECK(tig_video_buffer_create(&info, &normal) == TIG_OK);
    CHECK(tig_video_buffer_create(&info, &dimmed) == TIG_OK);
    if (normal == NULL || dimmed == NULL) return;
    TigFont font = { 0 };
    font.flags = TIG_FONT_MODERN | TIG_FONT_DIM_BRACKETS;
    font.modern_font_path = path;
    font.modern_font_size = 20;
    font.color = tig_color_make(255, 255, 255);
    TigRect rect = { 0, 0, width, 160 };
    TigRect dirty;
    tig_font_handle_t handle;
    font.bracket_dim_percent = 100;
    tig_font_create(&font, &handle);
    tig_font_push(handle);
    CHECK(tig_font_write(normal, "Hello [action with words] world", &rect, &dirty) == TIG_OK);
    tig_font_pop();
    tig_font_destroy(handle);
    font.bracket_dim_percent = 60;
    tig_font_create(&font, &handle);
    tig_font_push(handle);
    CHECK(tig_font_write(dimmed, "Hello [action with words] world", &rect, &dirty) == TIG_OK);
    tig_font_pop();
    tig_font_destroy(handle);
    int darker = 0;
    int unchanged_ink = 0;
    TigVideoBufferData normal_data, dimmed_data;
    CHECK(tig_video_buffer_lock(normal) == TIG_OK);
    CHECK(tig_video_buffer_lock(dimmed) == TIG_OK);
    CHECK(tig_video_buffer_data(normal, &normal_data) == TIG_OK);
    CHECK(tig_video_buffer_data(dimmed, &dimmed_data) == TIG_OK);
    for (int y = 0; y < info.height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned int before = *(Uint32*)((Uint8*)normal_data.pixels + y * normal_data.pitch + x * 4);
            unsigned int after = *(Uint32*)((Uint8*)dimmed_data.pixels + y * dimmed_data.pitch + x * 4);
            if ((before & 0x00FFFFFFu) > (after & 0x00FFFFFFu)) darker++;
            if ((before & 0x00FFFFFFu) != 0 && before == after) unchanged_ink++;
        }
    }
    tig_video_buffer_unlock(normal);
    tig_video_buffer_unlock(dimmed);
    CHECK(darker > 0);
    CHECK(unchanged_ink > 0);
    tig_video_buffer_destroy(normal);
    tig_video_buffer_destroy(dimmed);
}

static void test_search_underlines(const char* path, int width)
{
    if (path == NULL || path[0] == '\0') return;
    TigVideoBufferCreateInfo info = { 0 };
    info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    info.width = width + 10;
    info.height = 180;
    TigVideoBuffer* buffer = NULL;
    CHECK(tig_video_buffer_create(&info, &buffer) == TIG_OK);
    if (buffer == NULL) return;
    TigFont font = { 0 };
    font.flags = TIG_FONT_MODERN;
    font.modern_font_path = path;
    font.modern_font_size = 20;
    font.color = tig_color_make(255, 255, 255);
    tig_font_handle_t handle;
    tig_font_create(&font, &handle);
    tig_font_push(handle);
    const char* text = "\xc9quipement bleu\nEncore \xe9quipement";
    const char* second = strrchr(text, (char)0xe9);
    CHECK(second != NULL);
    TigTextRange ranges[] = { { 0, 10 }, { (int)(second - text), 10 } };
    TigRect bounds = { 0, 0, info.width, info.height };
    TigRect rect = { 5, 7, width, 160 };
    CHECK(tig_video_buffer_fill(buffer, &bounds, tig_color_make(0, 0, 0)) == TIG_OK);
    TigFont before = { 0 };
    before.str = text; before.width = width;
    tig_font_measure(&before);
    CHECK(tig_font_underline_ranges(buffer, text, &rect, ranges, 2) == TIG_OK);
    CHECK(tig_font_underline_ranges(buffer, text, &rect, NULL, 0) == TIG_OK);
    TigTextRange invalid = { (int)strlen(text), 1 };
    CHECK(tig_font_underline_ranges(buffer, text, &rect, &invalid, 1) == TIG_ERR_INVALID_PARAM);
    invalid = (TigTextRange){ -1, 2 };
    CHECK(tig_font_underline_ranges(buffer, text, &rect, &invalid, 1) == TIG_ERR_INVALID_PARAM);
    TigFont after = { 0 };
    after.str = text; after.width = width;
    tig_font_measure(&after);
    CHECK(before.width == after.width && before.height == after.height);
    TigVideoBufferData data;
    CHECK(tig_video_buffer_lock(buffer) == TIG_OK);
    CHECK(tig_video_buffer_data(buffer, &data) == TIG_OK);
    int ink = 0;
    int rows = 0;
    for (int y = 0; y < info.height; y++) {
        bool row = false;
        for (int x = 0; x < info.width; x++) {
            Uint32 pixel = *(Uint32*)((Uint8*)data.pixels + y * data.pitch + x * 4);
            if ((pixel & 0xFFFFFFu) != 0) {
                ink++;
                row = true;
                CHECK(x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height);
            }
        }
        if (row) rows++;
    }
    CHECK(ink > 0 && rows >= 2);
    tig_video_buffer_unlock(buffer);
    tig_font_pop();
    tig_font_destroy(handle);
    tig_video_buffer_destroy(buffer);
}

static void test_language_labels(const char* path)
{
    if (path == NULL || path[0] == '\0') return;
    const char* labels[] = { "English", "Fran\xc3\xa7" "ais", "Espa\xc3\xb1ol", "Italiano", "\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9" };
    TigFont font = { 0 };
    font.flags = TIG_FONT_MODERN | TIG_FONT_UTF8;
    font.modern_font_path = path;
    font.modern_font_size = 12;
    font.color = tig_color_make(255, 0, 0);
    tig_font_handle_t handle;
    tig_font_create(&font, &handle);
    tig_font_push(handle);
    for (int index = 0; index < SDL_arraysize(labels); index++) {
        TigFont measure = { 0 };
        measure.str = labels[index];
        tig_font_measure(&measure);
        SDL_Color color = { 255, 0, 0, 255 };
        SDL_Surface* expected = TTF_RenderText_Blended((TTF_Font*)((TigFont*)handle)->modern_font, labels[index], 0, color);
        CHECK(expected != NULL);
        if (expected != NULL) {
            CHECK(measure.width == expected->w);
            CHECK(measure.height == expected->h);
            SDL_DestroySurface(expected);
        }
    }
    tig_font_pop();
    tig_font_destroy(handle);
}

static void test_modern_cache(const char* path)
{
    if (path == NULL || path[0] == '\0') return;
    TigVideoBufferCreateInfo info = { 0 };
    info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    info.width = 320; info.height = 180;
    TigVideoBuffer* buffer = NULL;
    CHECK(tig_video_buffer_create(&info, &buffer) == TIG_OK);
    if (buffer == NULL) return;
    TigFont font = { 0 };
    font.flags = TIG_FONT_MODERN | TIG_FONT_SHADOW | TIG_FONT_DIM_BRACKETS;
    font.modern_font_path = path;
    font.modern_font_size = 20;
    font.modern_shadow_opacity = 180;
    font.modern_shadow_distance = 2;
    tig_font_handle_t handle;
    tig_font_create(&font, &handle);
    tig_font_push(handle);
    CHECK(tig_performance_set_enabled(true, "text-cache-test.csv"));
    Uint32 pixels[320 * 180];
    TigRect bounds = { 0, 0, 320, 180 };
    for (int variant = 0; variant < 12; variant++) {
        TigFont* actual = (TigFont*)handle;
        actual->color = tig_color_make(255, variant * 20, 30);
        actual->modern_shadow_opacity = variant * 20;
        actual->modern_shadow_color = tig_color_make(variant * 10, 20, 0);
        actual->bracket_dim_percent = variant * 8;
        actual->flags = font.flags | (variant % 2 ? TIG_FONT_UNDERLINE : 0)
            | (variant % 3 ? TIG_FONT_UTF8 : 0);
        TigRect rect = { 4, 6, 90 + variant * 15, 160 };
        const char* text = "Dialogue [action] \xc3\xa9quipement";
        TigPerformanceSummary before, cold, hot;
        tig_performance_summary(TIG_PERF_TEXT_RASTER, &before);
        TigRect first_dirty, second_dirty;
        tig_video_buffer_fill(buffer, &bounds, 0);
        CHECK(tig_font_write(buffer, text, &rect, &first_dirty) == TIG_OK);
        tig_performance_summary(TIG_PERF_TEXT_RASTER, &cold);
        CHECK(cold.count > before.count);
        TigVideoBufferData data;
        CHECK(tig_video_buffer_lock(buffer) == TIG_OK);
        tig_video_buffer_data(buffer, &data);
        for (int y = 0; y < 180; y++) memcpy(pixels + y * 320, (Uint8*)data.pixels + y * data.pitch, 320 * 4);
        tig_video_buffer_unlock(buffer);
        tig_video_buffer_fill(buffer, &bounds, 0);
        CHECK(tig_font_write(buffer, text, &rect, &second_dirty) == TIG_OK);
        tig_performance_summary(TIG_PERF_TEXT_RASTER, &hot);
        CHECK(hot.count == cold.count);
        CHECK(memcmp(&first_dirty, &second_dirty, sizeof(TigRect)) == 0);
        CHECK(tig_video_buffer_lock(buffer) == TIG_OK);
        tig_video_buffer_data(buffer, &data);
        for (int y = 0; y < 180; y++) CHECK(memcmp(pixels + y * 320, (Uint8*)data.pixels + y * data.pitch, 320 * 4) == 0);
        tig_video_buffer_unlock(buffer);
    }
    tig_performance_set_enabled(false, NULL);
    remove("text-cache-test.csv");
    tig_font_pop();
    tig_font_destroy(handle);
    tig_video_buffer_destroy(buffer);
}

static void test_modern_measurement(const char* path)
{
    if (path == NULL || path[0] == '\0') return;
    const char* texts[] = {
        "", " ", "\n", "\n\n", "Leading and trailing spaces  ",
        "One\nTwo\n", "A very long word: ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        "[action] Dialogue with accented names: \xc3\x89glise, Fran\xc3\xa7" "ais.",
        "\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9",
    };
    const int widths[] = { 0, 1, 90, 400 };
    const int sizes[] = { 12, 20, 28 };
    const unsigned int styles[] = { 0, TIG_FONT_UNDERLINE, TIG_FONT_STRIKE_THROUGH,
        TIG_FONT_UNDERLINE | TIG_FONT_STRIKE_THROUGH | TIG_FONT_DIM_BRACKETS };
    CHECK(tig_performance_set_enabled(true, "measurement-test.csv"));
    for (int size = 0; size < SDL_arraysize(sizes); size++) {
        for (int style = 0; style < SDL_arraysize(styles); style++) {
            TigFont font = { 0 };
            font.flags = TIG_FONT_MODERN | TIG_FONT_UTF8 | styles[style];
            font.modern_font_path = path;
            font.modern_font_size = (float)sizes[size];
            font.color = tig_color_make(255, 255, 255);
            tig_font_handle_t handle;
            tig_font_create(&font, &handle);
            CHECK(((TigFont*)handle)->modern_font != NULL);
            tig_font_push(handle);
            for (int text = 0; text < SDL_arraysize(texts); text++) {
                for (int width = 0; width < SDL_arraysize(widths); width++) {
                    TigFont measure = { 0 };
                    measure.str = texts[text];
                    measure.width = widths[width];
                    tig_font_measure(&measure);
                    TTF_Font* native = (TTF_Font*)((TigFont*)handle)->modern_font;
                    SDL_Color white = { 255, 255, 255, 255 };
                    SDL_Surface* old = widths[width] > 0
                        ? TTF_RenderText_Blended_Wrapped(native, texts[text], 0, white, widths[width])
                        : TTF_RenderText_Blended(native, texts[text], 0, white);
                    CHECK(measure.height == (old != NULL ? old->h : 0));
                    CHECK(measure.width == (widths[width] != 0 ? widths[width] : old != NULL ? old->w : 0));
                    SDL_DestroySurface(old);
                }
            }
            TigFont empty = { 0 };
            tig_font_measure(&empty);
            CHECK(empty.height == 0);
            tig_font_pop();
            tig_font_destroy(handle);
        }
    }
    // The measurement path must not invoke TIG's rasterisation anymore.
    TigPerformanceSummary raster;
    CHECK(tig_performance_summary(TIG_PERF_TEXT_RASTER, &raster));
    CHECK(raster.count == 0);
    tig_performance_set_enabled(false, NULL);
    remove("measurement-test.csv");

    // The default bitmap-era byte encoding still converts before measuring.
    TigFont font = { 0 };
    font.flags = TIG_FONT_MODERN;
    font.modern_font_path = path;
    font.modern_font_size = 20;
    tig_font_handle_t handle;
    tig_font_create(&font, &handle);
    CHECK(((TigFont*)handle)->modern_font != NULL);
    tig_font_push(handle);
    TigFont measure = { 0 };
    measure.str = "\xc9glise, Fran\xe7" "ais";
    tig_font_measure(&measure);
    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Surface* expected = TTF_RenderText_Blended((TTF_Font*)((TigFont*)handle)->modern_font,
        "\xc3\x89glise, Fran\xc3\xa7" "ais", 0, white);
    CHECK(expected != NULL);
    if (expected != NULL) CHECK(measure.width == expected->w && measure.height == expected->h);
    SDL_DestroySurface(expected);
    tig_font_pop();
    tig_font_destroy(handle);
}

int main(int argc, char** argv)
{
    CHECK(SDL_Init(0));
    tig_memory_init(NULL);
    tig_color_init(NULL);
    tig_color_set_rgba_settings(0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    test_ini_round_trip();
    test_map_markers();
    test_journal_index_knowledge();
    test_conversation_history();
    test_language_archives();
    test_random_encounter_modes();
    test_text_bubble_dimensions();
    test_stable_order_and_save_ids();
    test_lighting_reduction();
    test_terrain_encoding();
    test_sector_script_dif_persistence();
    test_localized_catalog_fallback();
    test_mes_duplicate_groups();
    test_startup_resolution();
    test_bindings();
    test_panel_navigation_bindings();
    test_panel_navigation_mouse();
    test_zoom();
    if (argc > 1) {
        test_language_labels(argv[1]);
        test_modern_measurement(argv[1]);
        test_modern_cache(argv[1]);
        test_large_floating_dialogue(argv[1]);
        test_modern_brackets(argv[1], 400);
        test_modern_brackets(argv[1], 90);
        test_search_underlines(argv[1], 400);
        test_search_underlines(argv[1], 90);
    }
    if (argc > 2) test_button_art(argv[2]);
    if (argc > 3) {
        test_worldmap_accents(argv[3]);
        test_plain_stock_label(argv[3]);
    }
    if (argc > 4) test_journal_catalogs(argv[4]);
    settings_exit(&settings);
    tig_color_exit();
    tig_memory_exit();
    SDL_Quit();
    printf("Community feature regressions: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
