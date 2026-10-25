#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "game/settings.h"

typedef enum SaveFailure {
    SAVE_FAILURE_NONE,
    SAVE_FAILURE_OPEN,
    SAVE_FAILURE_WRITE,
    SAVE_FAILURE_FLUSH,
    SAVE_FAILURE_SYNC,
    SAVE_FAILURE_CLOSE,
    SAVE_FAILURE_REPLACE,
} SaveFailure;

static SaveFailure failure;
static int failures;
static int temporary_opens;
static bool collide_once;
static bool expect_original = true;
static char temporary_path[1024];
static char collision_path[1024];
static const char original[] = "original configuration\n";
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition); failures++; } } while (0)

static bool file_equals(const char* path, const char* expected)
{
    FILE* stream = fopen(path, "rb");
    char data[4096];
    if (stream == NULL) return false;
    size_t size = fread(data, 1, sizeof(data), stream);
    bool ok = !ferror(stream) && size == strlen(expected) && memcmp(data, expected, size) == 0;
    fclose(stream);
    return ok;
}

static void write_original(void)
{
    FILE* stream = fopen("arcanum.ini", "wb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK(fputs(original, stream) >= 0);
        CHECK(fclose(stream) == 0);
    }
}

static FILE* save_test_open(const char* path, const char* mode)
{
    if (strcmp(mode, "wx") != 0) return fopen(path, mode);
    temporary_opens++;
    SDL_strlcpy(temporary_path, path, sizeof(temporary_path));
    if (failure == SAVE_FAILURE_OPEN) { errno = EACCES; return NULL; }
    if (collide_once) {
        collide_once = false;
        SDL_strlcpy(collision_path, path, sizeof(collision_path));
        FILE* occupied = fopen(path, "wx");
        CHECK(occupied != NULL);
        if (occupied != NULL) { fputs("collision", occupied); fclose(occupied); }
        errno = EEXIST;
        return NULL;
    }
    FILE* stream = fopen(path, mode);
    if (stream != NULL && failure == SAVE_FAILURE_WRITE) {
        fclose(stream);
        stream = fopen(path, "rb");
    }
    return stream;
}

static int save_test_flush(FILE* stream)
{
    if (failure == SAVE_FAILURE_FLUSH) { errno = ENOSPC; return EOF; }
    return fflush(stream);
}

static int save_test_sync(int descriptor)
{
    if (failure == SAVE_FAILURE_SYNC) { errno = EIO; return -1; }
#ifdef _WIN32
    return _commit(descriptor);
#else
    return fsync(descriptor);
#endif
}

static int save_test_close(FILE* stream)
{
    int result = fclose(stream);
    if (failure == SAVE_FAILURE_CLOSE) { errno = EIO; return EOF; }
    return result;
}

static bool save_test_replace(const char* from, const char* to)
{
    if (expect_original) CHECK(file_equals(to, original));
    else {
        FILE* stream = fopen(to, "rb");
        CHECK(stream == NULL);
        if (stream != NULL) fclose(stream);
    }
    if (failure == SAVE_FAILURE_REPLACE) return SDL_SetError("Simulated replacement failure");
    return SDL_RenamePath(from, to);
}

#define fopen save_test_open
#define fflush save_test_flush
#define fclose save_test_close
#define SDL_RenamePath save_test_replace
#ifdef _WIN32
#define _commit save_test_sync
#else
#define fsync save_test_sync
#endif
#include "../src/game/settings.c"
#undef fopen
#undef fflush
#undef fclose
#undef SDL_RenamePath
#ifdef _WIN32
#undef _commit
#else
#undef fsync
#endif

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

int main(void)
{
    for (failure = SAVE_FAILURE_NONE; failure <= SAVE_FAILURE_REPLACE; failure++) {
        Settings settings;
        write_original();
        temporary_path[0] = '\0';
        settings_init(&settings, "arcanum.ini");
        settings_register(&settings, "language", "english", NULL);
        settings_set_str_value(&settings, "language", "french");
        settings_save(&settings);
        if (failure == SAVE_FAILURE_NONE) {
            CHECK((settings.flags & SETTINGS_CHANGED) == 0);
            CHECK(!file_equals("arcanum.ini", original));
            FILE* stream = fopen("arcanum.ini", "rt");
            char data[4096] = { 0 };
            CHECK(stream != NULL);
            if (stream != NULL) { fread(data, 1, sizeof(data) - 1, stream); fclose(stream); }
            CHECK(strstr(data, "[global]\nlanguage=french\n") != NULL);
            int before = temporary_opens;
            settings_save(&settings);
            CHECK(temporary_opens == before);
        } else {
            CHECK((settings.flags & SETTINGS_CHANGED) != 0);
            CHECK(file_equals("arcanum.ini", original));
            CHECK(strcmp(settings_get_str_value(&settings, "language"), "french") == 0);
        }
        FILE* leftover = fopen(temporary_path, "rb");
        CHECK(leftover == NULL);
        if (leftover != NULL) fclose(leftover);
        if (failure != SAVE_FAILURE_NONE) {
            SaveFailure previous_failure = failure;
            failure = SAVE_FAILURE_NONE;
            settings_save(&settings);
            CHECK((settings.flags & SETTINGS_CHANGED) == 0);
            CHECK(!file_equals("arcanum.ini", original));
            failure = previous_failure;
        }
        settings_exit(&settings);
    }
    failure = SAVE_FAILURE_NONE;
    collide_once = true;
    write_original();
    Settings settings;
    settings_init(&settings, "arcanum.ini");
    settings_register(&settings, "language", "english", NULL);
    settings_set_str_value(&settings, "language", "french");
    settings_save(&settings);
    CHECK((settings.flags & SETTINGS_CHANGED) == 0);
    CHECK(file_equals(collision_path, "collision"));
    SDL_RemovePath(collision_path);
    settings_exit(&settings);
    SDL_RemovePath("arcanum.ini");
    expect_original = false;
    SDL_RemovePath("fresh-settings.ini");
    settings_init(&settings, "fresh-settings.ini");
    settings_register(&settings, "language", "english", NULL);
    settings_set_str_value(&settings, "language", "french");
    settings_save(&settings);
    CHECK((settings.flags & SETTINGS_CHANGED) == 0);
    FILE* fresh = fopen("fresh-settings.ini", "rt");
    char content[128] = { 0 };
    CHECK(fresh != NULL);
    if (fresh != NULL) { fread(content, 1, sizeof(content) - 1, fresh); fclose(fresh); }
    CHECK(strcmp(content, "language=french\n") == 0);
    settings_exit(&settings);
    SDL_RemovePath("fresh-settings.ini");
    tig_memory_exit();
    return failures != 0;
}
