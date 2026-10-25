#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tig/file.h>
#include <tig/idxtable.h>
#include <tig/memory.h>
#include <tig/str_parse.h>
#ifdef _WIN32
#include <windows.h>
#endif

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

static void table_roundtrip(void)
{
    TigIdxTable original, loaded, copied;
    tig_idxtable_init(&original, sizeof(int));
    tig_idxtable_init(&loaded, sizeof(int));
    for (int key = 0; key < 600; key++) {
        int value = key + 100;
        tig_idxtable_set(&original, key * 63, &value);
    }
    int value = 42;
    tig_idxtable_set(&original, INT_MIN, &value);
    tig_idxtable_set(&original, -7, &value);
    CHECK(tig_idxtable_contains(&original, 7));
    CHECK(tig_idxtable_get(&original, INT_MIN, &value) && value == 42);
    tig_idxtable_copy(&copied, &original);
    CHECK(copied.count == original.count);
    CHECK(tig_idxtable_get(&copied, INT_MIN, &value) && value == 42);
    tig_idxtable_exit(&copied);
    for (int key = 8; key < 600; key++) tig_idxtable_remove(&original, key * 63);
    tig_idxtable_copy(&copied, &original);
    TigFile* stream = tig_file_fopen("table.bin", "wb");
    CHECK(stream != NULL);
    CHECK(tig_idxtable_write(&copied, stream) != 0);
    tig_file_fclose(stream);
    stream = tig_file_fopen("table.bin", "rb");
    CHECK(tig_idxtable_read(&loaded, stream));
    CHECK(loaded.count == original.count);
    for (int key = 0; key < 8; key++) CHECK(tig_idxtable_get(&loaded, key * 63, &value) && value == key + 100);
    CHECK(tig_idxtable_get(&loaded, INT_MIN, &value) && value == 42);
    tig_file_fclose(stream);
    FILE* native = fopen("table.bin", "rb");
    CHECK(native != NULL);
    fseek(native, 0, SEEK_END);
    size_t size = (size_t)ftell(native);
    rewind(native);
    unsigned char* bytes = malloc(size);
    CHECK(bytes != NULL && fread(bytes, 1, size, native) == size);
    fclose(native);
    for (size_t prefix = 0; prefix < size; prefix++) {
        native = fopen("invalid.bin", "wb");
        fwrite(bytes, 1, prefix, native);
        fclose(native);
        stream = tig_file_fopen("invalid.bin", "rb");
        CHECK(!tig_idxtable_read(&loaded, stream));
        CHECK(loaded.count == original.count);
        CHECK(tig_idxtable_get(&loaded, INT_MIN, &value) && value == 42);
        tig_file_fclose(stream);
    }
    const int offsets[] = { 0, 4, 8, 12, 16, (int)size - 4 };
    for (int damage = 0; damage < 6; damage++) {
        unsigned char* altered = malloc(size);
        memcpy(altered, bytes, size);
        int invalid = damage == 3 ? INT_MAX : -2;
        memcpy(altered + offsets[damage], &invalid, sizeof(invalid));
        native = fopen("invalid.bin", "wb");
        fwrite(altered, 1, size, native);
        fclose(native);
        free(altered);
        stream = tig_file_fopen("invalid.bin", "rb");
        CHECK(!tig_idxtable_read(&loaded, stream));
        CHECK(loaded.count == original.count);
        tig_file_fclose(stream);
    }
    free(bytes);
    tig_idxtable_remove(&loaded, INT_MIN);
    CHECK(!tig_idxtable_contains(&loaded, INT_MIN));
    tig_idxtable_exit(&copied);
    tig_idxtable_exit(&loaded);
    tig_idxtable_exit(&original);
    remove("table.bin");
    remove("invalid.bin");
}

static void conversions(void)
{
    const char* inputs[] = { "2147483647", "2147483648", "-2147483648", "-2147483649", "999999999999999999999999999", "-99999999999999999999999999", "42tail", "invalid", "  -7 " };
    const int expected[] = { INT_MAX, INT_MAX, INT_MIN, INT_MIN, INT_MAX, INT_MIN, 42, 0, -7 };
    for (int i = 0; i < (int)SDL_arraysize(inputs); i++) {
        char text[128];
        strcpy(text, inputs[i]);
        char* cursor = text;
        int result = 99;
        tig_str_parse_value(&cursor, &result);
        CHECK(result == expected[i] && cursor == NULL);
    }
    char untouched[] = "abc";
    char* cursor = untouched;
    char destination = 'X';
    tig_str_parse_str_value(&cursor, &destination, 0);
    CHECK(destination == 'X' && cursor == untouched);
    char range[] = "2-999999999999999999999";
    cursor = range;
    int first, last;
    tig_str_parse_range(&cursor, &first, &last);
    CHECK(first == 2 && last == INT_MAX);
}

static void allocations(void)
{
    unsigned char* bytes = tig_memory_calloc(7, 8, __FILE__, __LINE__);
    CHECK(bytes != NULL);
    for (int i = 0; i < 56; i++) CHECK(bytes[i] == 0);
    bytes[0] = 42;
    bytes = tig_memory_realloc(bytes, 8, __FILE__, __LINE__);
    CHECK(bytes[0] == 42);
    TigMemoryStatistics statistics;
    tig_memory_statistics(&statistics);
    CHECK(statistics.live_bytes == 8 && statistics.live_blocks == 1);
    tig_memory_free(bytes, __FILE__, __LINE__);
#ifdef _WIN32
    char path[MAX_PATH];
    CHECK(GetModuleFileNameA(NULL, path, sizeof(path)) != 0);
    for (int mode = 1; mode <= 4; mode++) {
        char command[2 * MAX_PATH];
        SDL_snprintf(command, sizeof(command), "\"%s\" --fatal %d", path, mode);
        STARTUPINFOA startup = { .cb = sizeof(startup) };
        PROCESS_INFORMATION process = { 0 };
        BOOL started = CreateProcessA(path, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process);
        CHECK(started);
        if (started) {
            DWORD status = WaitForSingleObject(process.hProcess, 10000);
            CHECK(status == WAIT_OBJECT_0);
            if (status != WAIT_OBJECT_0) TerminateProcess(process.hProcess, 99);
            DWORD code;
            CHECK(GetExitCodeProcess(process.hProcess, &code) && code == EXIT_FAILURE);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
        }
    }
#endif
}

int main(int argc, char** argv)
{
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    if (argc == 3 && strcmp(argv[1], "--fatal") == 0) {
        int mode = atoi(argv[2]);
        if (mode == 1) tig_memory_calloc(SIZE_MAX / 2 + 1, 2, __FILE__, __LINE__);
        if (mode == 2) tig_memory_alloc(SIZE_MAX, __FILE__, __LINE__);
        if (mode == 3 || mode == 4) {
            unsigned char* bytes = tig_memory_alloc(1, __FILE__, __LINE__);
            if (mode == 4) bytes[1] = 0;
            tig_memory_realloc(bytes, mode == 3 ? SIZE_MAX : 2, __FILE__, __LINE__);
        }
        return 0;
    }
    allocations();
    conversions();
    CHECK(tig_file_repository_add("."));
    char byte = 0;
    CHECK(tig_file_fread(&byte, 1, SIZE_MAX, NULL) == 0);
    CHECK(tig_file_fwrite(&byte, 1, SIZE_MAX, NULL) == 0);
    TigFile* stream = tig_file_fopen("bounds.bin", "wb");
    CHECK(stream != NULL);
    CHECK(tig_file_fwrite(&byte, 2, SIZE_MAX / 2 + 1, stream) == 0);
    CHECK(tig_file_fwrite(NULL, 1, 1, stream) == 0);
    tig_file_fclose(stream);
    stream = tig_file_fopen("bounds.bin", "rb");
    CHECK(tig_file_fread(&byte, 2, SIZE_MAX / 2 + 1, stream) == 0);
    tig_file_fclose(stream);
    remove("bounds.bin");
    table_roundtrip();
    tig_file_exit();
    tig_memory_exit();
    SDL_Quit();
    printf("Numeric storage: %d failures\n", failures);
    return failures != 0;
}
