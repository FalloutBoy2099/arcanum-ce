#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "game/obj_private.h"
#include "game/obj_file.h"
#include <tig/memory.h>

static int failures;
static int alignment;
static int enumerated;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

bool objf_read(void* bytes, size_t size, TigFile* stream)
{
    return tig_file_fread(bytes, 1, size, stream) == size;
}

bool objf_write(const void* bytes, size_t size, TigFile* stream)
{
    return tig_file_fwrite(bytes, 1, size, stream) == size;
}

static bool check_alignment(void* entry, int key)
{
    CHECK((uintptr_t)entry % alignment == 0);
    CHECK(key == 0 || key == 31 || key == 32 || key == 65);
    enumerated++;
    return true;
}

static void roundtrip(void)
{
    SizeableArray* array = NULL;
    const uint64_t values[] = { UINT64_C(0x1122334455667788), UINT64_C(0xfedcba9876543210) };
    sa_allocate(&array, sizeof(uint64_t));
    sa_set(&array, 0, &values[0]);
    sa_set(&array, 65, &values[1]);
    const uint32_t expected[] = { 8, 2, 0, 0x55667788, 0x11223344, 0x76543210, 0xfedcba98, 3, 1, 0, 2 };
    unsigned char serialized[sizeof(expected)] = { 0 };
    CHECK(sa_total_byte_size(&array) == sizeof(expected));
    sa_write_mem(&array, (SizeableArray*)serialized);
    CHECK(memcmp(serialized, expected, sizeof(expected)) == 0);
    for (size_t prefix = 0; prefix < sizeof(serialized); prefix++) {
        SizeableArray* partial = NULL;
        ReadBuffer incomplete = { .cursor = serialized, .remaining = prefix };
        CHECK(!sa_read_mem(&partial, &incomplete));
        CHECK(partial == NULL);
    }
    for (int damage = 0; damage < 3; damage++) {
        uint32_t corrupted[sizeof(expected) / sizeof(expected[0])];
        memcpy(corrupted, expected, sizeof(expected));
        if (damage == 0) corrupted[1] = 1;
        else if (damage == 1) corrupted[7] = UINT32_MAX;
        else corrupted[7] = INT_MAX;
        SizeableArray* partial = NULL;
        ReadBuffer invalid = { .cursor = (uint8_t*)corrupted, .remaining = sizeof(corrupted) };
        CHECK(!sa_read_mem(&partial, &invalid));
        CHECK(partial == NULL);
    }
    SizeableArray* loaded = NULL;
    ReadBuffer cursor = { .cursor = serialized, .remaining = sizeof(serialized) };
    CHECK(sa_read_mem(&loaded, &cursor));
    CHECK(cursor.remaining == 0);
    CHECK(sa_count(&loaded) == 2);
    uint64_t value = 0;
    sa_get(&loaded, 65, &value);
    CHECK(value == values[1]);
    alignment = 8;
    enumerated = 0;
    CHECK(sa_enumerate(&loaded, check_alignment));
    CHECK(enumerated == 2);
    sa_deallocate(&loaded);
    TigFile* file = tig_file_fopen("object-storage.bin", "wb");
    CHECK(file != NULL);
    if (file != NULL) { CHECK(sa_write_file(&array, file)); tig_file_fclose(file); }
    FILE* native = fopen("object-storage.bin", "rb");
    CHECK(native != NULL);
    if (native != NULL) {
        memset(serialized, 0, sizeof(serialized));
        CHECK(fread(serialized, 1, sizeof(serialized), native) == sizeof(serialized));
        CHECK(fgetc(native) == EOF);
        CHECK(memcmp(serialized, expected, sizeof(expected)) == 0);
        fclose(native);
    }
    file = tig_file_fopen("object-storage.bin", "rb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(sa_read_file(&loaded, file));
        sa_get(&loaded, 0, &value);
        CHECK(value == values[0]);
        sa_get(&loaded, 65, &value);
        CHECK(value == values[1]);
        tig_file_fclose(file);
    }
    sa_deallocate(&loaded);
    sa_deallocate(&array);
    remove("object-storage.bin");
    remove("object-partial.bin");
}

static void array_operations(int size)
{
    SizeableArray* array = NULL;
    SizeableArray* copy = NULL;
    unsigned char data[24], actual[24];
    for (int i = 0; i < size; i++) data[i] = (unsigned char)(i + 17);
    sa_allocate(&array, size);
    const int keys[] = { 65, 0, 32, 31 };
    for (int i = 0; i < 4; i++) sa_set(&array, keys[i], data);
    alignment = size % 8 == 0 ? 8 : 4;
    enumerated = 0;
    CHECK(sa_enumerate(&array, check_alignment));
    CHECK(enumerated == 4);
    sa_copy(&copy, &array);
    sa_remove(&array, 32);
    CHECK(!sa_has(&array, 32) && sa_count(&array) == 3);
    CHECK(sa_has(&copy, 32) && sa_count(&copy) == 4);
    for (int i = 0; i < 4; i++) {
        sa_get(&copy, keys[i], actual);
        CHECK(memcmp(data, actual, size) == 0);
    }
    memset(actual, 255, sizeof(actual));
    sa_get(&array, 32, actual);
    for (int i = 0; i < size; i++) CHECK(actual[i] == 0);
    sa_deallocate(&array);
    sa_deallocate(&copy);
}

static void native_pointers(void)
{
    int* allocation = MALLOC(sizeof(int));
    *allocation = 12345;
    intptr_t scalar = 0;
    ObjDataDescriptor descriptor = { .type = OD_TYPE_PTR, .ptr = &scalar };
    descriptor.storage.ptr = (intptr_t)allocation;
    obj_data_store(&descriptor);
    descriptor.storage.ptr = 0;
    obj_data_fetch(&descriptor);
    CHECK(descriptor.storage.ptr == (intptr_t)allocation);
    CHECK(*(int*)descriptor.storage.ptr == 12345);
    obj_data_reset(&descriptor);
    CHECK(scalar == 0 && *allocation == 12345);
    SizeableArray* array = NULL;
    descriptor.type = OD_TYPE_PTR_ARRAY;
    descriptor.ptr = &array;
    descriptor.idx = 65;
    descriptor.storage.ptr = (intptr_t)allocation;
    obj_data_store(&descriptor);
    descriptor.storage.ptr = 0;
    obj_data_fetch(&descriptor);
    CHECK(descriptor.storage.ptr == (intptr_t)allocation);
    CHECK(*(int*)descriptor.storage.ptr == 12345);
    alignment = sizeof(intptr_t) == 8 ? 8 : 4;
    enumerated = 0;
    CHECK(sa_enumerate(&array, check_alignment));
    CHECK(enumerated == 1);
    obj_data_reset(&descriptor);
    CHECK(array == NULL && *allocation == 12345);
    printf("Native pointer roundtrip: %p, %zu bits\n", (void*)allocation, sizeof(intptr_t) * 8);
    FREE(allocation);
}

static void malformed_arrays(void)
{
    const int headers[][3] = { { -1, 1, 0 }, { 8, -1, 0 }, { INT_MAX, INT_MAX, 0 }, { 0, 0, 0 }, { 8, 2, 0 } };
    for (int i = 0; i < 5; i++) {
        FILE* file = fopen("object-storage.bin", "wb");
        CHECK(file != NULL);
        if (file == NULL) continue;
        fwrite(headers[i], sizeof(headers[i]), 1, file);
        fclose(file);
        SizeableArray* array = NULL;
        sa_allocate(&array, sizeof(int));
        int value = 42;
        sa_set(&array, 0, &value);
        SizeableArray* original = array;
        TigFile* stream = tig_file_fopen("object-storage.bin", "rb");
        CHECK(stream != NULL);
        if (stream != NULL) {
            CHECK(!sa_read_file(&array, stream));
            CHECK(array == original);
            sa_get(&array, 0, &value);
            CHECK(value == 42);
            tig_file_fclose(stream);
        }
        sa_deallocate(&array);
    }
    for (int count_index = 0; count_index < 3; count_index++) {
        int counts[] = { -1, INT_MAX, 3 };
        const int header[] = { 8, 0, 0 };
        FILE* file = fopen("object-storage.bin", "wb");
        CHECK(file != NULL);
        if (file == NULL) continue;
        fwrite(header, sizeof(header), 1, file);
        fwrite(&counts[count_index], sizeof(int), 1, file);
        fclose(file);
        SizeableArray* array = NULL;
        TigFile* stream = tig_file_fopen("object-storage.bin", "rb");
        CHECK(stream != NULL);
        if (stream != NULL) {
            CHECK(!sa_read_no_dealloc(&array, stream));
            CHECK(array == NULL);
            tig_file_fclose(stream);
        }
    }
    remove("object-storage.bin");
    remove("object-partial.bin");
}

static void buffer_boundaries(void)
{
    size_t next = 0;
    CHECK(serialization_buffer_capacity(256, 256, 1, &next) && next == 512);
    CHECK(serialization_buffer_capacity(256, 200, 0, &next) && next == 256);
    CHECK(serialization_buffer_capacity(256, 200, INT_MAX - 200, &next) && next == INT_MAX);
    CHECK(!serialization_buffer_capacity(256, 200, (size_t)INT_MAX, &next));
    CHECK(!serialization_buffer_capacity(256, 257, 0, &next));
    CHECK(!serialization_buffer_capacity(SIZE_MAX, 0, 1, &next));
    CHECK(!serialization_buffer_capacity(256, 0, SIZE_MAX, &next));
    WriteBuffer buffer;
    write_buffer_init(&buffer);
    for (int i = 0; i < 20000; i++) {
        unsigned char byte = (unsigned char)i;
        CHECK(write_buffer_append(&byte, 1, &buffer));
    }
    CHECK(buffer.size == 32768 && buffer.size - buffer.remaining == 20000);
    for (int i = 0; i < 20000; i++) CHECK(buffer.base[i] == (unsigned char)i);
    size_t used = buffer.size - buffer.remaining;
    CHECK(!write_buffer_append("x", SIZE_MAX, &buffer));
    CHECK(buffer.failed && buffer.size - buffer.remaining == used);
    CHECK(!write_buffer_append("x", 1, &buffer));
    FREE(buffer.base);
    CHECK(!read_buffer_read(NULL, NULL, 0));
    CHECK(read_buffer_take(NULL, 1) == NULL);
    CHECK(!write_buffer_append(NULL, 0, NULL));
    ReadBuffer empty = { 0 };
    CHECK(read_buffer_read(&empty, NULL, 0));
    unsigned char byte = 42;
    CHECK(!read_buffer_read(&empty, &byte, 1) && byte == 42);
    ReadBuffer null_source = { .remaining = SIZE_MAX };
    CHECK(!read_buffer_read(&null_source, &byte, 1));
    ReadBuffer null_target = { .cursor = &byte, .remaining = 1 };
    CHECK(!read_buffer_read(&null_target, NULL, 1));
}

static void memory_fields(void)
{
    const int types[] = { OD_TYPE_INT32, OD_TYPE_INT64, OD_TYPE_STRING, OD_TYPE_HANDLE,
        OD_TYPE_INT32_ARRAY, OD_TYPE_UINT32_ARRAY, OD_TYPE_INT64_ARRAY, OD_TYPE_UINT64_ARRAY,
        OD_TYPE_SCRIPT_ARRAY, OD_TYPE_QUEST_ARRAY, OD_TYPE_HANDLE_ARRAY };
    for (int index = 0; index < sizeof(types) / sizeof(types[0]); index++) {
        intptr_t original = 0;
        ObjDataDescriptor field = { .type = types[index], .ptr = &original, .idx = 65 };
        if (field.type == OD_TYPE_INT32) field.storage.value = -12345;
        else if (field.type == OD_TYPE_INT64) field.storage.value64 = INT64_C(0x1122334455667788);
        else if (field.type == OD_TYPE_STRING) field.storage.str = "serialized name";
        else {
            memset(&field.storage, 0, sizeof(field.storage));
            field.storage.oid.type = OID_TYPE_GUID;
            for (int n = 0; n < sizeof(field.storage.oid.d.g.data); n++) field.storage.oid.d.g.data[n] = (unsigned char)(n + 1);
        }
        obj_data_store(&field);
        WriteBuffer bytes;
        write_buffer_init(&bytes);
        obj_data_write_mem(&field, &bytes);
        size_t length = bytes.size - bytes.remaining;
        CHECK(!bytes.failed && length != 0);
        TigFile* file = tig_file_fopen("object-storage.bin", "wb");
        CHECK(file != NULL);
        if (file != NULL) { CHECK(obj_data_write_file(&field, file)); tig_file_fclose(file); }
        FILE* native = fopen("object-storage.bin", "rb");
        CHECK(native != NULL);
        if (native != NULL) {
            unsigned char* expected = MALLOC(length);
            CHECK(fread(expected, 1, length, native) == length && fgetc(native) == EOF);
            CHECK(memcmp(expected, bytes.base, length) == 0);
            FREE(expected);
            fclose(native);
        }
        for (size_t prefix = 0; prefix < length; prefix++) {
            intptr_t output = 0;
            ObjDataDescriptor decoded = { .type = field.type, .ptr = &output, .idx = 65 };
            ReadBuffer reader = { .cursor = bytes.base, .remaining = prefix };
            CHECK(!obj_data_read_mem(&decoded, &reader));
            CHECK(output == 0);
        }
        for (int variant = 0; variant < 2; variant++) {
            for (size_t prefix = 0; prefix <= length; prefix++) {
                FILE* partial = fopen("object-partial.bin", "wb");
                CHECK(partial != NULL);
                fwrite(bytes.base, 1, prefix, partial);
                fclose(partial);
                TigFile* input = tig_file_fopen("object-partial.bin", "rb");
                intptr_t result = 0;
                ObjDataDescriptor decoded_file = { .type = field.type, .ptr = &result };
                bool loaded = variant == 0 ? obj_data_read_file_fast(&decoded_file, input) : obj_data_read_file_slow(&decoded_file, input);
                CHECK(loaded == (prefix == length));
                if (!loaded) CHECK(result == 0);
                else {
                    if (field.type == OD_TYPE_INT32) CHECK((int)result == -12345);
                    if (field.type == OD_TYPE_STRING) CHECK(strcmp((char*)result, "serialized name") == 0);
                    obj_data_reset(&decoded_file);
                }
                tig_file_fclose(input);
            }
        }
        intptr_t output = 0;
        ObjDataDescriptor decoded = { .type = field.type, .ptr = &output, .idx = 65 };
        ReadBuffer reader = { .cursor = bytes.base, .remaining = length };
        CHECK(obj_data_read_mem(&decoded, &reader) && reader.remaining == 0);
        obj_data_fetch(&field);
        obj_data_fetch(&decoded);
        if (field.type == OD_TYPE_STRING) {
            CHECK(strcmp(field.storage.str, decoded.storage.str) == 0);
            FREE(field.storage.str); FREE(decoded.storage.str);
        } else if (field.type == OD_TYPE_INT32 || field.type == OD_TYPE_INT32_ARRAY || field.type == OD_TYPE_UINT32_ARRAY) CHECK(field.storage.value == decoded.storage.value);
        else if (field.type == OD_TYPE_INT64 || field.type == OD_TYPE_INT64_ARRAY || field.type == OD_TYPE_UINT64_ARRAY) CHECK(field.storage.value64 == decoded.storage.value64);
        else if (field.type == OD_TYPE_SCRIPT_ARRAY) CHECK(memcmp(&field.storage.scr, &decoded.storage.scr, sizeof(Script)) == 0);
        else if (field.type == OD_TYPE_QUEST_ARRAY) CHECK(memcmp(&field.storage.quest, &decoded.storage.quest, sizeof(PcQuestState)) == 0);
        else CHECK(memcmp(&field.storage.oid, &decoded.storage.oid, sizeof(ObjectID)) == 0);
        obj_data_reset(&decoded);
        obj_data_reset(&field);
        FREE(bytes.base);
        write_buffer_init(&bytes);
        obj_data_write_mem(&field, &bytes);
        reader = (ReadBuffer){ .cursor = bytes.base, .remaining = bytes.size - bytes.remaining };
        CHECK(obj_data_read_mem(&decoded, &reader) && reader.remaining == 0);
        obj_data_reset(&decoded);
        FREE(bytes.base);
    }
    intptr_t pointer = 0;
    ObjDataDescriptor transient = { .type = OD_TYPE_PTR, .ptr = &pointer };
    WriteBuffer rejected;
    write_buffer_init(&rejected);
    obj_data_write_mem(&transient, &rejected);
    CHECK(rejected.failed && rejected.size == rejected.remaining);
    FREE(rejected.base);
    unsigned char malformed[] = { 1, 255, 255, 255, 127 };
    ReadBuffer reader = { .cursor = malformed, .remaining = sizeof(malformed) };
    char* previous = STRDUP("previous");
    transient.type = OD_TYPE_STRING;
    transient.ptr = &previous;
    CHECK(!obj_data_read_mem(&transient, &reader));
    CHECK(strcmp(previous, "previous") == 0);
    FREE(previous);
    remove("object-storage.bin");
    remove("object-partial.bin");
}

int main(int argc, char** argv)
{
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    CHECK(tig_file_repository_add("."));
    obj_data_init();
    bitset_pool_init();
    if (argc > 1 && strcmp(argv[1], "-alignment-only") == 0) {
        array_operations(8);
        native_pointers();
        bitset_pool_exit();
        obj_data_exit();
        tig_memory_exit();
        SDL_Quit();
        return failures != 0;
    }
    roundtrip();
    const int sizes[] = { 4, 8, 12, 16, 24 };
    for (int i = 0; i < 5; i++) array_operations(sizes[i]);
    native_pointers();
    malformed_arrays();
    buffer_boundaries();
    memory_fields();
    bitset_pool_exit();
    obj_data_exit();
    tig_file_exit();
    tig_memory_exit();
    SDL_Quit();
    printf("Object storage: %d failures\n", failures);
    return failures != 0;
}
