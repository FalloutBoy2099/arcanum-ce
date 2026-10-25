#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "game/mes.h"
#include "game/sector_tile_list.h"
#include "game/tile_script_list.h"
#include "game/walkmask.h"
#include "game/townmap_storage.h"

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

void sector_art_cache_register(tig_art_id_t id) { (void)id; }

static TigFile* fixture(const void* bytes, size_t size)
{
    FILE* file = fopen("fixture.bin", "wb");
    CHECK(file != NULL);
    if (file == NULL) return NULL;
    CHECK(fwrite(bytes, 1, size, file) == size);
    fclose(file);
    return tig_file_fopen("fixture.bin", "rb");
}

static void tile_bounds(void)
{
    SectorTileList list;
    tig_art_id_t initial = 17;
    sector_tile_list_init(&list, &initial);
    const int bad_counts[] = { -1, INT_MIN, INT_MAX, 4097 };
    for (int i = 0; i < 4; i++) {
        TigFile* file = fixture(&bad_counts[i], sizeof(int));
        CHECK(!sector_tile_list_load_with_dif(&list, file));
        CHECK(list.art_ids[0] == initial && !list.dif);
        tig_file_fclose(file);
    }
    const int bad_tiles[] = { -1, INT_MIN, 4096, INT_MAX };
    for (int i = 0; i < 4; i++) {
        const uint32_t record[] = { 1, (uint32_t)bad_tiles[i], 42 };
        TigFile* file = fixture(record, sizeof(record));
        CHECK(!sector_tile_list_load_with_dif(&list, file));
        CHECK(list.art_ids[0] == initial && !list.dif);
        tig_file_fclose(file);
    }
    const uint32_t record[] = { 2, 31, 42, 4095, 43 };
    for (size_t prefix = 0; prefix < sizeof(record); prefix++) {
        TigFile* file = fixture(record, prefix);
        CHECK(!sector_tile_list_load_with_dif(&list, file));
        tig_file_fclose(file);
    }
    TigFile* file = fixture(record, sizeof(record));
    CHECK(sector_tile_list_load_with_dif(&list, file));
    CHECK(list.art_ids[31] == 42 && list.art_ids[4095] == 43);
    CHECK(list.difmask[0] == UINT32_C(0x80000000));
    CHECK(list.difmask[127] == UINT32_C(0x80000000));
    tig_file_fclose(file);
}

static void script_lists(void)
{
    TileScriptList list;
    tile_script_node_init();
    tile_script_list_init(&list);
    const uint32_t record[] = { 1, 0, 33, 0, 0, 42, 0 };
    for (size_t prefix = 0; prefix < sizeof(record); prefix++) {
        TigFile* file = fixture(record, prefix);
        CHECK(!tile_script_list_load(&list, file));
        CHECK(list.head == NULL);
        tig_file_fclose(file);
    }
    const int bad[] = { -1, INT_MIN, INT_MAX };
    for (int i = 0; i < 3; i++) {
        TigFile* file = fixture(&bad[i], sizeof(int));
        CHECK(!tile_script_list_load(&list, file));
        tig_file_fclose(file);
        file = fixture(&bad[i], sizeof(int));
        CHECK(!tile_script_list_load_with_dif(&list, file));
        tig_file_fclose(file);
    }
    TigFile* file = fixture(record, sizeof(record));
    CHECK(tile_script_list_load(&list, file));
    CHECK(list.head != NULL && list.head->id == 33 && list.head->next == NULL);
    tig_file_fclose(file);
    tile_script_list_exit(&list);
    tile_script_node_exit();
}

static void facade_bounds(void)
{
    tig_file_mkdir("art");
    tig_file_mkdir("art/Facade");
    const int fields[][6] = {
        { 0, 0, 0, -1, 1, 0 }, { 0, 0, 0, 1, -1, 0 },
        { 0, 0, 0, INT_MAX, INT_MAX, 1 }, { 0, 0, 0, 1, 1, -1 },
        { 0, 0, 0, 1, 1, INT_MAX }, { 0, 0, 0, 1, 1, 1 },
        { 0, 0, 0, 1, 1, 0 }
    };
    for (int i = 0; i < 7; i++) {
        FILE* file = fopen("art/Facade/facwalk.00", "wb");
        CHECK(file != NULL);
        fwrite("FacWalk V101  ", sizeof("FacWalk V101  ") - 1, 1, file);
        fwrite(fields[i], sizeof(fields[i]), 1, file);
        fclose(file);
        tig_art_id_t* ids = NULL;
        int width = 0, height = 0;
        bool loaded = walkmask_load(0, &ids, &width, &height);
        CHECK(loaded == (i == 6));
        if (loaded) {
            CHECK(width == 1 && height == 1 && ids[0] == TIG_ART_ID_INVALID);
            FREE(ids);
        } else CHECK(ids == NULL && width == 0 && height == 0);
    }
    remove("art/Facade/facwalk.00");
}

static void message_merge(void)
{
    FILE* file = fopen("first.mes", "wb");
    fputs("{1}{First}{3}{Third}", file);
    fclose(file);
    file = fopen("second.mes", "wb");
    fputs("{2}{Second}{4}{Fourth}", file);
    fclose(file);
    mes_file_handle_t first, second;
    CHECK(mes_load("first.mes", &first));
    CHECK(mes_load("second.mes", &second));
    mes_merge(first, second);
    mes_unload(second);
    CHECK(mes_entries_count(first) == 4);
    const char* expected[] = { "First", "Second", "Third", "Fourth" };
    for (int i = 0; i < 4; i++) {
        MesFileEntry entry = { .num = i + 1 };
        CHECK(mes_search(first, &entry) && strcmp(entry.str, expected[i]) == 0);
    }
    mes_merge(first, first);
    CHECK(mes_entries_count(first) == 8);
    MesFileEntry entry = { .num = 4 };
    CHECK(mes_search(first, &entry) && strcmp(entry.str, "Fourth") == 0);
    mes_unload(first);
    file = fopen("invalid.mes", "wb");
    fputs("{999999999999999999999999999999999}{Invalid}", file);
    fclose(file);
    CHECK(!mes_load("invalid.mes", &first));
    CHECK(first == MES_FILE_HANDLE_INVALID);
    remove("first.mes"); remove("second.mes"); remove("invalid.mes");
}

static void map_dimensions(void)
{
    TownMapInfo valid = { .version = 2, .width = 4096, .height = 2048, .num_hor_tiles = 16, .num_vert_tiles = 8, .scale = 0.5f };
    CHECK(townmap_storage_valid(&valid));
    CHECK(!townmap_storage_valid(NULL));
    for (int damage = 0; damage < 9; damage++) {
        TownMapInfo invalid = valid;
        if (damage == 0) invalid.num_hor_tiles = 0;
        if (damage == 1) invalid.num_vert_tiles = -1;
        if (damage == 2) invalid.width = INT_MAX, invalid.height = INT_MAX, invalid.num_hor_tiles = INT_MAX, invalid.num_vert_tiles = INT_MAX;
        if (damage == 3) invalid.scale = INFINITY;
        if (damage == 4) invalid.scale = NAN;
        if (damage == 5) invalid.scale = 0;
        if (damage == 6) invalid.scale = -1;
        if (damage == 7) invalid.scale = (float)INT_MAX;
        if (damage == 8) invalid.scale = 0.00001f;
        CHECK(!townmap_storage_valid(&invalid));
    }
}

int main(void)
{
    CHECK(SDL_Init(0));
    tig_memory_init(NULL);
    CHECK(tig_file_repository_add("."));
    tile_bounds();
    script_lists();
    facade_bounds();
    message_merge();
    map_dimensions();
    remove("fixture.bin");
    tig_file_exit();
    tig_memory_exit();
    SDL_Quit();
    printf("Asset storage: %d failures\n", failures);
    return failures != 0;
}
