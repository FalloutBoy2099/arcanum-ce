#include <stdio.h>
#include <string.h>
#include "game/obj_pool.h"
#include "game/object.h"
#include <tig/memory.h>

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

void obj_deallocate(int64_t object) { obj_pool_deallocate(object); }
int map_current_map(void) { return 0; }
int obj_field_int32_get(int64_t object, int field) { (void)object; (void)field; return 0; }
void object_list_location(int64_t location, unsigned int flags, ObjectList* list)
{
    (void)location; (void)flags; memset(list, 0, sizeof(*list));
}
void object_list_destroy(ObjectList* list) { (void)list; }
bool objid_compare(ObjectID left, ObjectID right)
{
    if (left.type != right.type) return left.type < right.type;
    return left.d.a < right.d.a;
}

static void handles(void)
{
    int64_t handle, replacement;
    obj_pool_init(13, true);
    unsigned char* value = obj_pool_allocate(&handle);
    CHECK(value != NULL && (uintptr_t)value % 8 == 0);
    memset(value, 123, 13);
    CHECK(obj_handle_is_valid(handle));
    CHECK(obj_pool_lock(handle) == value);
    CHECK(obj_handle_is_valid(OBJ_HANDLE_NULL));
    int64_t invalid[] = { handle | (INT64_C(1) << 61),
        (int64_t)((uint64_t)handle | (UINT64_C(1) << 63)),
        (INT64_C(0x200000) << 29) | (handle & ((INT64_C(1) << 29) - 1)),
        handle ^ 1 };
    for (int i = 0; i < 4; i++) CHECK(!obj_handle_is_valid(invalid[i]));
    obj_pool_deallocate(handle);
    CHECK(!obj_handle_is_valid(handle));
    for (int i = 0; i < 4; i++) CHECK(!obj_handle_request(invalid[i]));
    CHECK(obj_handle_request(handle));
    unsigned char* restored = obj_pool_allocate(&replacement);
    CHECK(restored == value && replacement == handle);
    CHECK(obj_handle_is_valid(replacement));
    obj_pool_deallocate(replacement);
    obj_pool_allocate(&replacement);
    CHECK(replacement != handle && obj_handle_is_valid(replacement));
    CHECK(!obj_handle_is_valid(handle));
    obj_pool_deallocate(replacement);
    obj_pool_exit();
}

static void requested_reuse(void)
{
    int64_t handles[3], restored, ordinary, last;
    obj_pool_init(16, true);
    for (int i = 0; i < 3; i++) obj_pool_allocate(&handles[i]);
    for (int i = 0; i < 3; i++) obj_pool_deallocate(handles[i]);
    CHECK(obj_handle_request(handles[0]));
    void* requested = obj_pool_allocate(&restored);
    void* normal = obj_pool_allocate(&ordinary);
    void* final = obj_pool_allocate(&last);
    CHECK(restored == handles[0]);
    CHECK(requested != normal && requested != final && normal != final);
    CHECK(obj_handle_is_valid(restored));
    CHECK(obj_handle_is_valid(ordinary));
    if (requested != normal && requested != final) obj_pool_deallocate(restored);
    if (normal != final) obj_pool_deallocate(ordinary);
    obj_pool_deallocate(last);
    obj_pool_exit();
}

static void buckets(void)
{
    const int count = 8193;
    int64_t* handles = MALLOC(sizeof(int64_t) * count);
    obj_pool_init(13, true);
    for (int i = 0; i < count; i++) {
        unsigned char* data = obj_pool_allocate(&handles[i]);
        CHECK(data != NULL && (uintptr_t)data % 8 == 0);
        memcpy(data, &i, sizeof(i));
        CHECK(obj_handle_is_valid(handles[i]));
    }
    CHECK(handles[count - 1] > UINT32_MAX);
    for (int i = 0; i < count; i++) {
        int value;
        memcpy(&value, obj_pool_lock(handles[i]), sizeof(value));
        CHECK(value == i);
        obj_pool_deallocate(handles[i]);
        CHECK(!obj_handle_is_valid(handles[i]));
    }
    obj_pool_exit();
    FREE(handles);
}

int main(void)
{
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    handles();
    requested_reuse();
    buckets();
    tig_memory_exit();
    SDL_Quit();
    printf("Object pool: %d failures\n", failures);
    return failures != 0;
}
