#include "game/obj_private.h"

#include <stdio.h>
#include <limits.h>

#include "game/map.h"
#include "game/obj_file.h"
#include "game/object.h"
#include "game/sector.h"

#define WRITE_BUFFER_INITIAL_SIZE 256

/**
 * Initial capacity for the `BitsetDescriptor` array, and the granularity at
 * which it is grown when more bitsets are needed.
 */
#define BITSET_DESCRIPTORS_INITIAL_CAPACITY 4096
#define BITSET_DESCRIPTORS_GROWTH_STEP BITSET_DESCRIPTORS_INITIAL_CAPACITY

/**
 * Initial capacity for the free indices array, and the granularity at
 * which it is grown when more bitsets are freed.
 */
#define BITSET_FREE_LIST_INITIAL_CAPACITY 4096
#define BITSET_FREE_LIST_GROWTH_STEP BITSET_FREE_LIST_INITIAL_CAPACITY

/**
 * Initial capacity for the flat storage array that backs all live bitsets, and
 * the granularity at which it is grown when more space is needed.
 */
#define BITSET_STORAGE_INITIAL_CAPACITY 8192
#define BITSET_STORAGE_GROWTH_STEP BITSET_STORAGE_INITIAL_CAPACITY

/**
 * Every newly allocated (or recycled) bitset starts with this many 32-bit
 * storage items, giving a minimum capacity of `BITSET_MIN_STORAGE_ITEMS` *
 * `BITS_PER_STORAGE_ITEM` bits.
 */
#define BITSET_MIN_STORAGE_ITEMS 2

/**
 * Number of bits that fit in one storage item (int).
 */
#define BITS_PER_STORAGE_ITEM 32

/**
 * Number of entries in the 16-bit population-count lookup table.
 */
#define POPCOUNT_TABLE_SIZE 65536

/**
 * Number of entries in the split-mask table used by `bitset_count_bits`.
 */
#define POPCOUNT_MASKS_COUNT (BITS_PER_STORAGE_ITEM + 1)

typedef struct BitsetDescriptor {
    /* 0000 */ int cnt;
    /* 0004 */ int offset;
} BitsetDescriptor;

typedef struct PopCountMask {
    uint16_t lo_mask;
    uint16_t hi_mask;
} PopCountMask;

static void bitset_grow(int id, int cnt);
static void bitset_shrink(int id, int cnt);
static void bitset_shift_offsets(int start, int end, int delta);
static int bitset_index_of(int bit);
static int bitset_mask_of(int bit);
static void popcount_table_init(void);
static void popcount_masks_init(void);

/**
 * Flag indicating that obj data system has been initialized.
 *
 * 0x6036A8
 */
static bool obj_data_initialized;

/**
 * Lookup table for every 16-bit value.
 *
 * `popcount_table[n]` is the number of set bits in the 16-bit value `n`.
 *
 * Size: `65536`
 *
 * 0x6036FC
 */
static uint8_t* popcount_tbl;

/**
 * Capacity of the `bitset_descriptors` array.
 *
 * 0x603700
 */
static int bitset_descriptors_capacity;

/**
 * Capacity of the `bitset_free_list` array.
 *
 * 0x603704
 */
static int bitset_free_list_capacity;

/**
 * Total number of 32-bit storage items currently in use across all live
 * bitsets.
 *
 * 0x603708
 */
static int bitset_storage_size;

/**
 * List of recycled bitset indices available for reuse.
 *
 * Capacity: `bitset_free_list_capacity`
 * Size: `bitset_free_list_count`
 *
 * 0x60370C
 */
static int* bitset_free_list;

/**
 * Array of `BitsetDescriptor` entries, one per allocated bitset.
 *
 * Capacity: `bitset_descriptors_capacity`
 * Size: `bitset_count`
 *
 * 0x603710
 */
static BitsetDescriptor* bitset_descriptors;

/**
 * Current number of entries on the `bitset_free_list`.
 *
 * 0x603714
 */
static int bitset_free_list_count;

/**
 * Flat array that stores the actual bit data for all live bitsets. Each
 * bitset owns a contiguous slice of this array.
 *
 * Capacity: `bitset_storage_capacity`
 * Size: `bitset_storage_size`
 *
 * 0x603718
 */
static int* bitset_storage;

/**
 * Total allocated capacity of `bitset_storage` in number of ints.
 *
 * 0x60371C
 */
static int bitset_storage_capacity;

/**
 * Split-mask table used to count set bits in the first N bits of a 32-bit value
 * via two 16-bit lookups into `popcount_table`.
 *
 * Size: `33`
 *
 * 0x603720
 */
static PopCountMask* popcount_masks;

/**
 * Total number of bitset ever created.
 *
 * 0x603724
 */
static int bitset_count;

/**
 * Flag indicating whether the bitset pool has been initialized.
 *
 * 0x603728
 */
static bool bitset_pool_initialized;

/**
 * Initializes the obj data system.
 *
 * 0x4E3F80
 */
void obj_data_init(void)
{
    obj_data_initialized = true;
}

/**
 * Shuts down the obj data system.
 *
 * 0x4E3F90
 */
void obj_data_exit(void)
{
    obj_data_initialized = false;
}

/**
 * Resets an obj field to its zero/null state, freeing heap memory where the
 * field type requires it.
 *
 * Operates on the live field pointed to by `descr->ptr`. After this call the
 * field holds its "empty" representation: 0 for scalars, NULL for pointers,
 * and a deallocated `SizeableArray` for array types.
 *
 * 0x4E3FA0
 */
void obj_data_reset(ObjDataDescriptor* descr)
{
    switch (descr->type) {
    case OD_TYPE_INT32:
        *(int*)descr->ptr = 0;
        break;
    case OD_TYPE_INT64:
    case OD_TYPE_STRING:
    case OD_TYPE_HANDLE:
        // These types store a heap pointer, free it and nullify the pointer.
        if (*(void**)descr->ptr != NULL) {
            FREE(*(void**)descr->ptr);
            *(void**)descr->ptr = NULL;
        }
        break;
    case OD_TYPE_INT32_ARRAY:
    case OD_TYPE_INT64_ARRAY:
    case OD_TYPE_UINT32_ARRAY:
    case OD_TYPE_UINT64_ARRAY:
    case OD_TYPE_SCRIPT_ARRAY:
    case OD_TYPE_QUEST_ARRAY:
    case OD_TYPE_HANDLE_ARRAY:
        // Array types own a `SizeableArray` on the heap, deallocate.
        if (*(SizeableArray**)descr->ptr != NULL) {
            sa_deallocate((SizeableArray**)descr->ptr);
        }
        break;
    case OD_TYPE_PTR:
        // CE: Raw pointer field: zero without freeing (unowned reference).
        *(intptr_t*)descr->ptr = 0;
        break;
    case OD_TYPE_PTR_ARRAY:
        // CE: Raw pointer array: same behaviour as for other arrays.
        if (*(SizeableArray**)descr->ptr != NULL) {
            sa_deallocate((SizeableArray**)descr->ptr);
        }
        break;
    }
}

/**
 * Persists data by copying from transient storage data back to the live obj
 * field.
 *
 * For types that heap-allocate (INT64, STRING, HANDLE, arrays) memory is
 * allocated on demand and freed when the stored value is empty/null.
 *
 * 0x4E4000
 */
void obj_data_store(ObjDataDescriptor* descr)
{
    switch (descr->type) {
    case OD_TYPE_INT32:
        *(int*)descr->ptr = descr->storage.value;
        break;
    case OD_TYPE_INT64:
        if (descr->storage.value64 != 0) {
            // Allocate the backing int64 on demand if not yet present.
            if (*(int64_t**)descr->ptr == NULL) {
                *(int64_t**)descr->ptr = MALLOC(sizeof(int64_t));
            }
            **(int64_t**)descr->ptr = descr->storage.value64;
        } else {
            // Zero value: free the backing storage and nullify the pointer.
            if (*(int64_t**)descr->ptr != NULL) {
                FREE(*(int64_t**)descr->ptr);
                *(int64_t**)descr->ptr = NULL;
            }
        }
        break;
    case OD_TYPE_INT32_ARRAY:
    case OD_TYPE_UINT32_ARRAY:
        if (*(SizeableArray**)descr->ptr == NULL) {
            // Allocate array for storing 32-bit values.
            sa_allocate((SizeableArray**)descr->ptr, sizeof(int));
        }
        sa_set((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        break;
    case OD_TYPE_INT64_ARRAY:
    case OD_TYPE_UINT64_ARRAY:
        if (*(SizeableArray**)descr->ptr == NULL) {
            // Allocate array for storing 64-bit values.
            sa_allocate((SizeableArray**)descr->ptr, sizeof(int64_t));
        }
        sa_set((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        break;
    case OD_TYPE_SCRIPT_ARRAY:
        if (*(SizeableArray**)descr->ptr == NULL) {
            // Allocate array for storing `Script` objects (script number,
            // flags, and counters).
            sa_allocate((SizeableArray**)descr->ptr, sizeof(Script));
        }
        sa_set((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        break;
    case OD_TYPE_QUEST_ARRAY:
        if (*(SizeableArray**)descr->ptr == NULL) {
            // Allocate array for storing `PcQuestState` objects (datetime and
            // state).
            sa_allocate((SizeableArray**)descr->ptr, sizeof(PcQuestState));
        }
        sa_set((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        break;
    case OD_TYPE_STRING:
        // Replace any existing string with a fresh duplicate of the stored one.
        if (*(char**)descr->ptr != NULL) {
            FREE(*(char**)descr->ptr);
        }
        *(char**)descr->ptr = STRDUP(descr->storage.str);
        break;
    case OD_TYPE_HANDLE:
        if (descr->storage.oid.type != OID_TYPE_NULL) {
            // Non-null handle: allocate `ObjectID` on demand and copy into it.
            if (*(ObjectID**)descr->ptr == NULL) {
                *(ObjectID**)descr->ptr = MALLOC(sizeof(ObjectID));
            }
            **(ObjectID**)descr->ptr = descr->storage.oid;
            break;
        } else {
            // Null handle: free any existing `ObjectID` and nullify the
            // pointer.
            if (*(ObjectID**)descr->ptr != NULL) {
                FREE(*(ObjectID**)descr->ptr);
                *(ObjectID**)descr->ptr = NULL;
            }
        }
        break;
    case OD_TYPE_HANDLE_ARRAY:
        if (*(SizeableArray**)descr->ptr == NULL) {
            // Allocate array for storing `ObjectID` objects.
            sa_allocate((SizeableArray**)descr->ptr, sizeof(ObjectID));
        }
        sa_set((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        break;
    case OD_TYPE_PTR:
        *(intptr_t*)descr->ptr = descr->storage.ptr;
        break;
    case OD_TYPE_PTR_ARRAY:
        if (*(SizeableArray**)descr->ptr == NULL) {
            // Allocate array for storing raw pointers (these are useful at
            // runtime in transient properties).
            sa_allocate((SizeableArray**)descr->ptr, sizeof(intptr_t));
        }
        sa_set((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        break;
    }
}

/**
 * Fetches the current value from the live obj field into the transient
 * storage.
 *
 * 0x4E4180
 */
void obj_data_fetch(ObjDataDescriptor* descr)
{
    switch (descr->type) {
    case OD_TYPE_INT32:
        descr->storage.value = *(int*)descr->ptr;
        break;
    case OD_TYPE_INT64:
        if (*(int64_t**)descr->ptr != NULL) {
            descr->storage.value64 = **(int64_t**)descr->ptr;
        } else {
            descr->storage.value64 = 0;
        }
        break;
    case OD_TYPE_INT32_ARRAY:
    case OD_TYPE_UINT32_ARRAY:
        if (*(SizeableArray**)descr->ptr != NULL) {
            sa_get((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        } else {
            descr->storage.value = 0;
        }
        break;
    case OD_TYPE_INT64_ARRAY:
    case OD_TYPE_UINT64_ARRAY:
        if (*(SizeableArray**)descr->ptr != NULL) {
            sa_get((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        } else {
            descr->storage.value64 = 0;
        }
        break;
    case OD_TYPE_SCRIPT_ARRAY:
        if (*(SizeableArray**)descr->ptr != NULL) {
            sa_get((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        } else {
            memset(&(descr->storage.scr), 0, sizeof(descr->storage.scr));
        }
        break;
    case OD_TYPE_QUEST_ARRAY:
        if (*(SizeableArray**)descr->ptr != NULL) {
            sa_get((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        } else {
            memset(&(descr->storage.quest), 0, sizeof(descr->storage.quest));
        }
        break;
    case OD_TYPE_STRING:
        // Duplicate the string so the caller owns an independent copy.
        if (*(char**)descr->ptr != NULL) {
            descr->storage.str = STRDUP(*(char**)descr->ptr);
        } else {
            descr->storage.str = NULL;
        }
        break;
    case OD_TYPE_HANDLE:
        if (*(ObjectID**)descr->ptr != NULL) {
            descr->storage.oid = **(ObjectID**)descr->ptr;
        } else {
            descr->storage.oid.type = OID_TYPE_NULL;
        }
        break;
    case OD_TYPE_HANDLE_ARRAY:
        if (*(SizeableArray**)descr->ptr != NULL) {
            sa_get((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        } else {
            descr->storage.oid.type = OID_TYPE_NULL;
        }
        break;
    case OD_TYPE_PTR:
        descr->storage.ptr = *(intptr_t*)descr->ptr;
        break;
    case OD_TYPE_PTR_ARRAY:
        if (*(SizeableArray**)descr->ptr != NULL) {
            sa_get((SizeableArray**)descr->ptr, descr->idx, &(descr->storage));
        } else {
            descr->storage.ptr = 0;
        }
        break;
    }
}

/**
 * Performs a deep copy of the live obj field into a caller-supplied
 * destination buffer.
 *
 * `value` must point to storage large enough for the field's type.
 * Heap-allocated data is duplicated - the caller is responsible for freeing it.
 *
 * 0x4E4280
 */
void obj_data_copy(ObjDataDescriptor* descr, void* value)
{
    switch (descr->type) {
    case OD_TYPE_INT32:
        *(int*)value = *(int*)descr->ptr;
        break;
    case OD_TYPE_INT64:
        if (*(int64_t**)descr->ptr != NULL) {
            *(int64_t**)value = (int64_t*)MALLOC(sizeof(int64_t));
            **(int64_t**)value = **(int64_t**)descr->ptr;
        } else {
            *(int64_t**)value = NULL;
        }
        break;
    case OD_TYPE_INT32_ARRAY:
    case OD_TYPE_INT64_ARRAY:
    case OD_TYPE_UINT32_ARRAY:
    case OD_TYPE_UINT64_ARRAY:
    case OD_TYPE_SCRIPT_ARRAY:
    case OD_TYPE_QUEST_ARRAY:
    case OD_TYPE_HANDLE_ARRAY:
        if (*(SizeableArray**)descr->ptr != NULL) {
            sa_copy((SizeableArray**)value, (SizeableArray**)descr->ptr);
        } else {
            *(SizeableArray**)value = NULL;
        }
        break;
    case OD_TYPE_STRING:
        if (*(char**)descr->ptr != NULL) {
            *(char**)value = STRDUP(*(char**)descr->ptr);
        } else {
            *(char**)value = NULL;
        }
        break;
    case OD_TYPE_HANDLE:
        if (*(ObjectID**)descr->ptr != NULL) {
            *(ObjectID**)value = (ObjectID*)MALLOC(sizeof(ObjectID));
            **(ObjectID**)value = **(ObjectID**)descr->ptr;
        } else {
            *(ObjectID**)value = NULL;
        }
        break;
    case OD_TYPE_PTR:
    case OD_TYPE_PTR_ARRAY:
        assert(0);
    }
}

/**
 * Reads an obj field from a `TigFile` stream into the live field.
 *
 * Unlike `obj_data_read_file_fast`, this variant handles the case where the
 * field may already hold live heap data: existing allocations are freed before
 * new ones are created.
 *
 * Returns `true` on success, `false` otherwise.
 *
 * 0x4E4360
 */
bool obj_data_read_file_slow(ObjDataDescriptor* descr, TigFile* stream)
{
if (descr == NULL || descr->ptr == NULL || stream == NULL) return false;
    if (descr->type == OD_TYPE_INT32) {
        int value;
        if (!objf_read(&value, sizeof(value), stream)) return false;
        *(intptr_t*)descr->ptr = value;
        return true;
    }
    if (descr->type < OD_TYPE_INT64 || descr->type > OD_TYPE_HANDLE_ARRAY) return false;
    uint8_t presence;
    if (!objf_read(&presence, sizeof(presence), stream) || presence > 1) return false;
    if (!presence) {
        obj_data_reset(descr);
        return true;
    }
    switch (descr->type) {
    case OD_TYPE_INT64:
    case OD_TYPE_HANDLE: {
        union { int64_t integer; ObjectID handle; } value;
        size_t size = descr->type == OD_TYPE_INT64 ? sizeof(value.integer) : sizeof(value.handle);
        if (!objf_read(&value, size, stream)) return false;
        void* loaded = MALLOC(size);
        if (loaded == NULL) return false;
        memcpy(loaded, &value, size);
        obj_data_reset(descr);
        *(void**)descr->ptr = loaded;
        return true;
    }
    case OD_TYPE_STRING: {
        int length;
        if (!objf_read(&length, sizeof(length), stream) || length < 0 || length == INT_MAX
            || !tig_file_count_fits(stream, length + 1, INT_MAX, 1)) return false;
        size_t size = (size_t)length + 1;
        char* loaded = MALLOC(size);
        if (loaded == NULL) return false;
        if (!objf_read(loaded, size, stream) || loaded[length] != '\0') {
            FREE(loaded);
            return false;
        }
        obj_data_reset(descr);
        *(char**)descr->ptr = loaded;
        return true;
    }
    default: {
        int expected_size = sizeof(int);
        if (descr->type == OD_TYPE_INT64_ARRAY || descr->type == OD_TYPE_UINT64_ARRAY) expected_size = sizeof(int64_t);
        else if (descr->type == OD_TYPE_SCRIPT_ARRAY) expected_size = sizeof(Script);
        else if (descr->type == OD_TYPE_QUEST_ARRAY) expected_size = sizeof(PcQuestState);
        else if (descr->type == OD_TYPE_HANDLE_ARRAY) expected_size = sizeof(ObjectID);
        SizeableArray* loaded = NULL;
        if (!sa_read_no_dealloc(&loaded, stream)) return false;
        if (loaded->size != expected_size) {
            sa_deallocate(&loaded);
            return false;
        }
        obj_data_reset(descr);
        *(SizeableArray**)descr->ptr = loaded;
        return true;
    }
    }
}

/**
 * Reads an obj field from a `TigFile` stream into a freshly zeroed field.
 *
 * Returns `true` on success, `false` otherwise.
 *
 * 0x4E44F0
 */
bool obj_data_read_file_fast(ObjDataDescriptor* descr, TigFile* stream)
{
return obj_data_read_file_slow(descr, stream);
}

/**
 * Reads an obj field from a memory buffer.
 *
 * Mirrors `obj_data_read_file_slow`, but reads from a raw byte stream rather
 * than a `TigFile`. The buffer pointer `*data` is advanced by the number of
 * bytes consumed. Existing heap allocations in the field are freed before new
 * ones are created.
 *
 * 0x4E4660
 */
bool obj_data_read_mem(ObjDataDescriptor* descr, ReadBuffer* data)
{
    if (descr == NULL || descr->ptr == NULL || data == NULL) return false;
    uint8_t presence;
    if (descr->type == OD_TYPE_INT32) {
        int value;
        if (!read_buffer_read(data, &value, sizeof(value))) return false;
        *(intptr_t*)descr->ptr = value;
        return true;
    }
    if (descr->type < OD_TYPE_INT64 || descr->type > OD_TYPE_HANDLE_ARRAY) return false;
    if (!read_buffer_read(data, &presence, sizeof(presence))) return false;
    if (!presence) {
        obj_data_reset(descr);
        return true;
    }
    switch (descr->type) {
    case OD_TYPE_INT64: {
        int64_t value;
        if (!read_buffer_read(data, &value, sizeof(value))) return false;
        int64_t* loaded = MALLOC(sizeof(value));
        if (loaded == NULL) return false;
        *loaded = value;
        obj_data_reset(descr);
        *(int64_t**)descr->ptr = loaded;
        return true;
    }
    case OD_TYPE_INT32_ARRAY:
    case OD_TYPE_INT64_ARRAY:
    case OD_TYPE_UINT32_ARRAY:
    case OD_TYPE_UINT64_ARRAY:
    case OD_TYPE_SCRIPT_ARRAY:
    case OD_TYPE_QUEST_ARRAY:
    case OD_TYPE_HANDLE_ARRAY: {
        int expected_size = sizeof(int);
        if (descr->type == OD_TYPE_INT64_ARRAY || descr->type == OD_TYPE_UINT64_ARRAY) expected_size = sizeof(int64_t);
        else if (descr->type == OD_TYPE_SCRIPT_ARRAY) expected_size = sizeof(Script);
        else if (descr->type == OD_TYPE_QUEST_ARRAY) expected_size = sizeof(PcQuestState);
        else if (descr->type == OD_TYPE_HANDLE_ARRAY) expected_size = sizeof(ObjectID);
        SizeableArray* loaded = NULL;
        if (!sa_read_mem(&loaded, data)) return false;
        if (loaded->size != expected_size) {
            sa_deallocate(&loaded);
            return false;
        }
        obj_data_reset(descr);
        *(SizeableArray**)descr->ptr = loaded;
        return true;
    }
    case OD_TYPE_STRING: {
        int length;
        if (!read_buffer_read(data, &length, sizeof(length)) || length < 0 || length == INT_MAX) return false;
        size_t size = (size_t)length + 1;
        const uint8_t* bytes = read_buffer_take(data, size);
        if (data->failed || bytes[size - 1] != 0) return false;
        char* loaded = MALLOC(size);
        if (loaded == NULL) return false;
        memcpy(loaded, bytes, size);
        obj_data_reset(descr);
        *(char**)descr->ptr = loaded;
        return true;
    }
    case OD_TYPE_HANDLE: {
        ObjectID value;
        if (!read_buffer_read(data, &value, sizeof(value))) return false;
        ObjectID* loaded = MALLOC(sizeof(value));
        if (loaded == NULL) return false;
        *loaded = value;
        obj_data_reset(descr);
        *(ObjectID**)descr->ptr = loaded;
        return true;
    }
    default:
        return false;
    }
}

/**
 * Writes an obj field to a `TigFile` stream.
 *
 * Returns `true` on success, `false` otherwise.
 *
 * 0x4E47E0
 */
bool obj_data_write_file(ObjDataDescriptor* descr, TigFile* stream)
{
    uint8_t presence;
    int size;

    switch (descr->type) {
    case OD_TYPE_INT32:
        if (!objf_write((int*)descr->ptr, sizeof(int), stream)) return false;
        return true;
    case OD_TYPE_INT64:
        if (*(int64_t**)descr->ptr != NULL) {
            presence = 1;
            if (!objf_write(&presence, sizeof(presence), stream)) return false;
            if (!objf_write(*(int64_t**)descr->ptr, sizeof(int64_t), stream)) return false;
        } else {
            presence = 0;
            if (!objf_write(&presence, sizeof(presence), stream)) return false;
        }
        return true;
    case OD_TYPE_INT32_ARRAY:
    case OD_TYPE_INT64_ARRAY:
    case OD_TYPE_UINT32_ARRAY:
    case OD_TYPE_UINT64_ARRAY:
    case OD_TYPE_SCRIPT_ARRAY:
    case OD_TYPE_QUEST_ARRAY:
    case OD_TYPE_HANDLE_ARRAY:
        if (*(SizeableArray**)descr->ptr != NULL) {
            presence = 1;
            if (!objf_write(&presence, sizeof(presence), stream)) return false;
            if (!sa_write_file((SizeableArray**)descr->ptr, stream)) return false;
        } else {
            presence = 0;
            if (!objf_write(&presence, sizeof(presence), stream)) return false;
        }
        return true;
    case OD_TYPE_STRING:
        if (*(char**)descr->ptr != NULL) {
            presence = 1;
            if (!objf_write(&presence, sizeof(presence), stream)) return false;
            size_t length = strlen(*(char**)descr->ptr);
            if (length >= INT_MAX) return false;
            size = (int)length;
            if (!objf_write(&size, sizeof(size), stream)) return false;
            if (!objf_write(*(char**)descr->ptr, size + 1, stream)) return false;
        } else {
            presence = 0;
            if (!objf_write(&presence, sizeof(presence), stream)) return false;
        }
        return true;
    case OD_TYPE_HANDLE:
        if (*(ObjectID**)descr->ptr != NULL) {
            presence = 1;
            if (!objf_write(&presence, sizeof(presence), stream)) return false;
            if (!objf_write(*(ObjectID**)descr->ptr, sizeof(ObjectID), stream)) return false;
        } else {
            presence = 0;
            if (!objf_write(&presence, sizeof(presence), stream)) return false;
        }
        return true;
    case OD_TYPE_PTR:
    case OD_TYPE_PTR_ARRAY:
        // These fields should only be stored in transient obj storage and
        // should never be serialized.
        assert(0);
    default:
        return false;
    }
}

// 0x4E4990
void obj_data_write_mem(ObjDataDescriptor* descriptor, WriteBuffer* wb)
{
    uint8_t presence;
    if (descriptor->type == OD_TYPE_INT32) {
        int value = (int)*(intptr_t*)descriptor->ptr;
        write_buffer_append(&value, sizeof(value), wb);
        return;
    }
    if (descriptor->type < OD_TYPE_INT64 || descriptor->type > OD_TYPE_HANDLE_ARRAY) {
        wb->failed = true;
        return;
    }
    presence = *(void**)descriptor->ptr != NULL;
    if (!write_buffer_append(&presence, sizeof(presence), wb) || !presence) return;
    switch (descriptor->type) {
    case OD_TYPE_INT64:
        write_buffer_append(*(int64_t**)descriptor->ptr, sizeof(int64_t), wb);
        return;
    case OD_TYPE_INT32_ARRAY:
    case OD_TYPE_INT64_ARRAY:
    case OD_TYPE_UINT32_ARRAY:
    case OD_TYPE_UINT64_ARRAY:
    case OD_TYPE_SCRIPT_ARRAY:
    case OD_TYPE_QUEST_ARRAY:
    case OD_TYPE_HANDLE_ARRAY: {
        int size = sa_total_byte_size((SizeableArray**)descriptor->ptr);
        if (size < 0) { wb->failed = true; return; }
        if (!write_buffer_reserve(wb, (size_t)size)) return;
        sa_write_mem((SizeableArray**)descriptor->ptr, (SizeableArray*)wb->curr);
        wb->curr += size;
        wb->remaining -= size;
        return;
    }
    case OD_TYPE_STRING: {
        size_t length = strlen(*(char**)descriptor->ptr);
        if (length >= INT_MAX) { wb->failed = true; return; }
        int stored_length = (int)length;
        if (write_buffer_append(&stored_length, sizeof(stored_length), wb))
            write_buffer_append(*(char**)descriptor->ptr, length + 1, wb);
        return;
    }
    case OD_TYPE_HANDLE:
        write_buffer_append(*(ObjectID**)descriptor->ptr, sizeof(ObjectID), wb);
        return;
    default:
        wb->failed = true;
    }
}

/**
 * Erases the element at `descr->idx` from an array-type field.
 *
 * Has no effect for non-array types or when the array pointer is NULL.
 *
 * 0x4E4B70
 */
void obj_data_remove_idx(ObjDataDescriptor* descr)
{
    switch (descr->type) {
    case OD_TYPE_INT32_ARRAY:
    case OD_TYPE_INT64_ARRAY:
    case OD_TYPE_UINT32_ARRAY:
    case OD_TYPE_UINT64_ARRAY:
    case OD_TYPE_SCRIPT_ARRAY:
    case OD_TYPE_QUEST_ARRAY:
    case OD_TYPE_HANDLE_ARRAY:
    case OD_TYPE_PTR_ARRAY:
        if (*(SizeableArray**)descr->ptr != NULL) {
            sa_remove((SizeableArray**)descr->ptr, descr->idx);
        }
        break;
    }
}

/**
 * Returns the number of elements in an array-type field.
 *
 * Returns `0` for non-array types or when the array pointer is NULL.
 *
 * 0x4E4BA0
 */
int obj_data_count_idx(ObjDataDescriptor* descr)
{
    switch (descr->type) {
    case OD_TYPE_INT32_ARRAY:
    case OD_TYPE_INT64_ARRAY:
    case OD_TYPE_UINT32_ARRAY:
    case OD_TYPE_UINT64_ARRAY:
    case OD_TYPE_SCRIPT_ARRAY:
    case OD_TYPE_QUEST_ARRAY:
    case OD_TYPE_HANDLE_ARRAY:
    case OD_TYPE_PTR_ARRAY:
        if (*(SizeableArray**)descr->ptr != NULL) {
            return sa_count((SizeableArray**)descr->ptr);
        }
        break;
    }

    return 0;
}

/**
 * Initializes a `WriteBuffer`.
 *
 * The caller is responsible to free `base` when done.
 *
 * 0x4E4BD0
 */
void write_buffer_init(WriteBuffer* wb)
{
    wb->base = MALLOC(WRITE_BUFFER_INITIAL_SIZE);
    wb->curr = wb->base;
    wb->size = WRITE_BUFFER_INITIAL_SIZE;
    wb->remaining = wb->size;
    wb->failed = wb->base == NULL;
}

bool write_buffer_reserve(WriteBuffer* wb, size_t size)
{
    if (wb == NULL || wb->failed) return false;
    size_t next;
    size_t used = wb->size - wb->remaining;
    if (!serialization_buffer_capacity(wb->size, used, size, &next)) {
        wb->failed = true;
        return false;
    }
    if (next > wb->size) {
        uint8_t* resized = REALLOC(wb->base, next);
        if (resized == NULL) {
            wb->failed = true;
            return false;
        }
        wb->base = resized;
        wb->curr = wb->base + used;
        wb->size = next;
        wb->remaining = next - used;
    }
    return true;
}

bool write_buffer_append(const void* data, size_t size, WriteBuffer* wb)
{
    if (wb == NULL) return false;
    if ((size != 0 && data == NULL) || !write_buffer_reserve(wb, size)) {
        wb->failed = true;
        return false;
    }
    if (size != 0) memcpy(wb->curr, data, size);
    wb->curr += size;
    wb->remaining -= size;
    return true;
}

/**
 * Initializes the bitset pool and allocates all backing arrays.
 *
 * 0x4E59B0
 */
void bitset_pool_init(void)
{
    bitset_count = 0;
    bitset_descriptors_capacity = BITSET_DESCRIPTORS_INITIAL_CAPACITY;

    bitset_free_list_count = 0;
    bitset_free_list_capacity = BITSET_FREE_LIST_INITIAL_CAPACITY;

    bitset_storage_size = 0;
    bitset_storage_capacity = BITSET_STORAGE_INITIAL_CAPACITY;

    bitset_descriptors = (BitsetDescriptor*)MALLOC(sizeof(*bitset_descriptors) * bitset_descriptors_capacity);
    bitset_free_list = (int*)MALLOC(sizeof(*bitset_free_list) * bitset_free_list_capacity);
    bitset_storage = (int*)MALLOC(sizeof(*bitset_storage) * bitset_storage_capacity);

    popcount_tbl = (uint8_t*)MALLOC(POPCOUNT_TABLE_SIZE);
    popcount_masks = (PopCountMask*)MALLOC(sizeof(*popcount_masks) * POPCOUNT_MASKS_COUNT);
    popcount_table_init();
    popcount_masks_init();

    bitset_pool_initialized = true;
}

/**
 * Shuts down the bitset pool and frees all backing storage.
 *
 * 0x4E5A50
 */
void bitset_pool_exit(void)
{
    FREE(popcount_masks);
    FREE(popcount_tbl);
    FREE(bitset_storage);
    FREE(bitset_free_list);
    FREE(bitset_descriptors);

    bitset_pool_initialized = false;
}

/**
 * Allocates a new bitset from the pool and returns its ID.
 *
 * 0x4E5AA0
 */
int bitset_alloc(void)
{
    int index;

    // Reuse a recycled slot from the free list if one is available.
    if (bitset_free_list_count != 0) {
        return bitset_free_list[--bitset_free_list_count];
    }

    // No recycled slots, grow the descriptor array on demand.
    if (bitset_count == bitset_descriptors_capacity) {
        bitset_descriptors_capacity += BITSET_DESCRIPTORS_GROWTH_STEP;
        bitset_descriptors = (BitsetDescriptor*)REALLOC(bitset_descriptors, sizeof(BitsetDescriptor) * bitset_descriptors_capacity);
    }

    // Assign the storage item offset for this new entry. The first entry starts
    // at 0, every subsequent entry is packed directly after the previous one.
    index = bitset_count++;
    if (index == 0) {
        bitset_descriptors[0].offset = index;
    } else {
        bitset_descriptors[index].offset = bitset_descriptors[index - 1].cnt + bitset_descriptors[index - 1].offset;
    }

    // Grow newly allocated bitset to the minimum size.
    bitset_descriptors[index].cnt = 0;
    bitset_grow(index, BITSET_MIN_STORAGE_ITEMS);

    return index;
}

/**
 * Returns a bitset to the pool for future reuse.
 *
 * 0x4E5B40
 */
void bitset_free(int id)
{
    BitsetDescriptor* desc;
    int index;

    desc = &(bitset_descriptors[id]);

    // Shrink to the minimum size to reclaim excess storage items in
    // `bitset_storage`.
    if (desc->cnt != BITSET_MIN_STORAGE_ITEMS) {
        bitset_shrink(id, desc->cnt - BITSET_MIN_STORAGE_ITEMS);
    }

    // Clear all remaining bits.
    for (index = desc->offset; index < desc->cnt + desc->offset; index++) {
        bitset_storage[index] = 0;
    }

    // Push onto the free list, growing the list on demand.
    if (bitset_free_list_count == bitset_free_list_capacity) {
        bitset_free_list_capacity += BITSET_FREE_LIST_GROWTH_STEP;
        bitset_free_list = (int*)REALLOC(bitset_free_list, sizeof(int) * bitset_free_list_capacity);
    }

    bitset_free_list[bitset_free_list_count++] = id;
}

/**
 * Creates a copy the specified bitset.
 *
 * 0x4E5BF0
 */
int bitset_copy(int src)
{
    int dst;
    int delta;
    int index;

    dst = bitset_alloc();

    // Resize the new bitset to match the source's storage item count.
    delta = bitset_descriptors[src].cnt - bitset_descriptors[dst].cnt;
    if (delta != 0) {
        bitset_grow(dst, delta);
    }

    // Bulk-copy all storage items from src into dst.
    for (index = 0; index < bitset_descriptors[src].cnt; index++) {
        bitset_storage[bitset_descriptors[dst].offset + index] = bitset_storage[bitset_descriptors[src].offset + index];
    }

    return dst;
}

/**
 * Sets or clears a single bit in a bitset.
 *
 * 0x4E5C60
 */
void bitset_set(int id, int pos, bool val)
{
    int idx;
    int mask;

    idx = bitset_index_of(pos);
    if (idx >= bitset_descriptors[id].cnt) {
        if (!val) {
            // Clearing an out-of-range bit is a no-op, those bits are always 0.
            return;
        }

        // Grow the bitset to include the storage item that contains `pos`.
        bitset_grow(id, idx - bitset_descriptors[id].cnt + 1);
    }

    mask = bitset_mask_of(pos);
    if (val) {
        bitset_storage[idx + bitset_descriptors[id].offset] |= mask;
    } else {
        bitset_storage[idx + bitset_descriptors[id].offset] &= ~mask;
    }
}

/**
 * Tests whether the bit at a specified position in a bitset is set to 1.
 *
 * 0x4E5CE0
 */
bool bitset_test(int id, int pos)
{
    int idx;
    int mask;

    idx = bitset_index_of(pos);
    if (idx > bitset_descriptors[id].cnt - 1) {
        return 0;
    }

    mask = bitset_mask_of(pos);
    return (mask & bitset_storage[idx + bitset_descriptors[id].offset]) != 0;
}

/**
 * Returns the number of set bits strictly before position `pos` in the
 * bitset with ID `id`.
 *
 * 0x4E5D30
 */
int bitset_rank(int id, int pos)
{
    int cnt = 0;
    int base_item_offset;
    int idx;
    int stop_item_offset;

    base_item_offset = bitset_descriptors[id].offset;
    idx = bitset_index_of(pos);
    stop_item_offset = base_item_offset + idx;
    if (idx < bitset_descriptors[id].cnt) {
        // Count only the bits in the target storage item that precede `pos`.
        cnt += bitset_count_bits(bitset_storage[stop_item_offset], pos % BITS_PER_STORAGE_ITEM);
    } else {
        // `pos` is past the end, count up to the last allocated storage item
        // instead.
        stop_item_offset = base_item_offset + bitset_descriptors[id].cnt;
    }

    // Count all set bits in every complete storage item that comes before the
    // target.
    while (base_item_offset < stop_item_offset) {
        cnt += bitset_count_bits(bitset_storage[base_item_offset++], BITS_PER_STORAGE_ITEM);
    }

    return cnt;
}

/**
 * Iterates over all set bits in a bitset, invoking `callback` for each.
 *
 * 0x4E5DB0
 */
bool bitset_enumerate(int id, bool (*callback)(int))
{
    int start;
    int end;
    int pos;
    int idx;
    int bit;
    uint32_t flags;

    start = bitset_descriptors[id].offset;
    end = bitset_descriptors[id].cnt + start;

    pos = 0;
    for (idx = start; idx < end; idx++) {
        flags = 1;
        for (bit = 0; bit < 32; bit++) {
            if ((flags & bitset_storage[idx]) != 0) {
                if (!callback(pos)) {
                    return false;
                }
            }
            flags <<= 1;
            pos++;
        }
    }

    return true;
}

/**
 * Serializes the bitset with ID `id` to a `TigFile` stream.
 *
 * Returns `true` on success, `false` on write error.
 *
 * 0x4E5E20
 */
bool bitset_write_file(int id, TigFile* stream)
{
    if (tig_file_fwrite(&(bitset_descriptors[id].cnt), sizeof(int), 1, stream) != 1) {
        return false;
    }

    if (tig_file_fwrite(&(bitset_storage[bitset_descriptors[id].offset]), sizeof(int) * bitset_descriptors[id].cnt, 1, stream) != 1) {
        return false;
    }

    return true;
}

/**
 * Deserializes a bitset from a `TigFile` stream.
 *
 * Returns `true` on success, `false` on read error.
 *
 * 0x4E5E80
 */
bool bitset_read_file(int* out_id, TigFile* stream)
{
    int id;
    int count;
    int position;
    int file_size;
    if (tig_file_fread(&count, sizeof(count), 1, stream) != 1 || count < 0) return false;
    position = tig_file_ftell(stream);
    file_size = tig_file_filelength(stream);
    if (position < 0 || file_size < position || count > (file_size - position) / (int)sizeof(int)
        || count > INT_MAX / (int)sizeof(int) - bitset_storage_size - BITSET_STORAGE_GROWTH_STEP) return false;
    id = bitset_alloc();
    if (count > bitset_descriptors[id].cnt) bitset_grow(id, count - bitset_descriptors[id].cnt);
    if (count != 0 && tig_file_fread(&bitset_storage[bitset_descriptors[id].offset], sizeof(int), count, stream) != (size_t)count) {
        bitset_free(id);
        return false;
    }
    *out_id = id;
    return true;
}

/**
 * Returns the number of bytes required to serialize the specified bitset.
 *
 * 0x4E5F10
 */
int bitset_serialized_size(int id)
{
    return sizeof(int) + sizeof(int) * (bitset_descriptors[id].cnt);
}

/**
 * Serializes a bitset to a memory buffer.
 *
 * 0x4E5F30
 */
void bitset_write_mem(int id, uint8_t* dst)
{
    int offset;
    int cnt;

    offset = bitset_descriptors[id].offset;
    cnt = bitset_descriptors[id].cnt;

    memcpy(dst, &cnt, sizeof(cnt));
    memcpy(dst + sizeof(cnt), &(bitset_storage[offset]), sizeof(*bitset_storage) * cnt);
}

/**
 * Deserializes a bitset from a memory buffer.
 *
 * 0x4E5F70
 */
bool bitset_read_mem(int* id_ptr, ReadBuffer* data)
{
    int count;
    if (!read_buffer_read(data, &count, sizeof(count)) || count < 0
        || (size_t)count > data->remaining / sizeof(int)
        || count > INT_MAX / (int)sizeof(int) - bitset_storage_size - BITSET_STORAGE_GROWTH_STEP) return false;
    const uint8_t* bytes = read_buffer_take(data, (size_t)count * sizeof(int));
    if (data->failed) return false;
    int id = bitset_alloc();
    if (count > bitset_descriptors[id].cnt) bitset_grow(id, count - bitset_descriptors[id].cnt);
    if (count != 0) memcpy(&bitset_storage[bitset_descriptors[id].offset], bytes, (size_t)count * sizeof(int));
    *id_ptr = id;
    return true;
}

/**
 * Counts the number of set bits in the first `n` bits of a 32-bit value.
 *
 * Uses the precomputed `popcount_table` (one byte per 16-bit value) together
 * with `popcount_masks[n]` to isolate exactly the `n` lowest bits, split across
 * both 16-bit halves.
 *
 * 0x4E5FE0
 */
int bitset_count_bits(int value, int n)
{
    return popcount_tbl[popcount_masks[n].lo_mask & (value & 0xFFFF)]
        + popcount_tbl[popcount_masks[n].hi_mask & ((value >> 16) & 0xFFFF)];
}

/**
 * Grows bitset `id` by `cnt` additional 32-bit storage items.
 *
 * All added storage items are initialized to 0. Because every bitset occupies a
 * contiguous slice of the shared `bitset_storage` array, inserting new items
 * into one bitset requires shifting the data of all higher-indexed bitsets to
 * the right and updating their `offset` fields accordingly.
 *
 * 0x4E6040
 */
void bitset_grow(int id, int cnt)
{
    int* base;
    int idx;

    if (bitset_storage_size + cnt > bitset_storage_capacity) {
        bitset_storage_capacity += ((bitset_storage_size + cnt - bitset_storage_capacity - 1) / BITSET_STORAGE_GROWTH_STEP + 1) * BITSET_STORAGE_GROWTH_STEP;
        bitset_storage = REALLOC(bitset_storage, sizeof(*bitset_storage) * bitset_storage_capacity);
    }

    // Shift the data of all bitsets that come after `id` to make room.
    if (id != bitset_count - 1) {
        base = &(bitset_storage[bitset_descriptors[id + 1].offset]);
        memmove(&(base[cnt]),
            base,
            sizeof(*base) * (bitset_storage_size - bitset_descriptors[id + 1].offset));

        // Adjust the stored offset of every subsequent bitset.
        bitset_shift_offsets(id + 1, bitset_count - 1, cnt);
    }

    // Update the descriptor and usage counter.
    bitset_descriptors[id].cnt += cnt;
    bitset_storage_size += cnt;

    // Zero-initialize the newly appended items.
    for (idx = bitset_descriptors[id].cnt + bitset_descriptors[id].offset - cnt; idx < bitset_descriptors[id].cnt + bitset_descriptors[id].offset; idx++) {
        bitset_storage[idx] = 0;
    }
}

/**
 * Shrinks bitset `id` by removing `cnt` 32-bit storage items from its tail.
 *
 * Shifts the data of all higher-indexed bitsets to the left and updates their
 * `offset` fields accordingly.
 *
 * 0x4E6130
 */
void bitset_shrink(int id, int cnt)
{
    int* base;

    // Close the gap by shifting higher bitset data to the left.
    if (id != bitset_count - 1) {
        base = &(bitset_storage[bitset_descriptors[id + 1].offset]);
        memmove(&(base[-cnt]),
            base,
            sizeof(*base) * (bitset_storage_size - bitset_descriptors[id + 1].offset));

        // Adjust the stored offset of every subsequent bitset.
        bitset_shift_offsets(id + 1, bitset_count - 1, -cnt);
    }

    bitset_descriptors[id].cnt -= cnt;
    bitset_storage_size -= cnt;
}

/**
 * Adds `delta` to the `offset` of every bitset within the specified range.
 *
 * Called by `bitset_grow` and `bitset_shrink` after inserting or removing
 * indices so that all descriptor offsets stay consistent with the flat array.
 *
 * 0x4E61B0
 */
void bitset_shift_offsets(int start, int end, int delta)
{
    int index;

    for (index = start; index <= end; index++) {
        bitset_descriptors[index].offset += delta;
    }
}

/**
 * Returns the index of a given bit position in contiguous data array.
 *
 * 0x4E61E0
 */
int bitset_index_of(int pos)
{
    return pos / 32;
}

/**
 * Returns the single-bit mask for a given bit position.
 *
 * 0x4E61F0
 */
int bitset_mask_of(int pos)
{
    return 1u << (pos % 32);
}

/**
 * Fills `popcount_tbl` so that `popcount_tbl[n]` gives the number of set
 * bits in the 16-bit value `n`.
 *
 * 0x4E6210
 */
void popcount_table_init(void)
{
    int idx;
    uint8_t cnt;
    uint16_t mask;
    int bit;

    for (idx = 0; idx < POPCOUNT_TABLE_SIZE; idx++) {
        cnt = 0;
        mask = 1;
        for (bit = 16; bit != 0; bit--) {
            if ((mask & idx) != 0) {
                cnt++;
            }
            mask <<= 1;
        }
        popcount_tbl[idx] = cnt;
    }
}

/**
 * Fills `popcount_masks` so that `popcount_masks[n]` isolates exactly the
 * first `n `bits of a 32-bit value, split across the two 16-bit halves.
 *
 * 0x4E6240
 */
void popcount_masks_init(void)
{
    int idx;
    uint16_t step = 1;
    uint16_t accum = 0;

    // Entry 0: no bits selected in either half.
    popcount_masks[0].lo_mask = 0;
    popcount_masks[0].hi_mask = 0;

    for (idx = 1; idx <= 16; idx++) {
        accum += step;
        step <<= 1;

        // Entries 1–16: all idx target bits are within the low 16-bit half.
        popcount_masks[idx].lo_mask = accum;
        popcount_masks[idx].hi_mask = 0;

        // Entries 17–32: the low half is fully selected (0xFFFF). The high half
        // selects the remaining idx bits (mirroring the low-half pattern).
        popcount_masks[idx + 16].lo_mask = 0xFFFF;
        popcount_masks[idx + 16].hi_mask = accum;
    }
}

bool bitset_count_matches(int id, int expected)
{
    if (expected < 0) return false;
    for (int index = 0; index < bitset_descriptors[id].cnt; index++) {
        int count = bitset_count_bits(bitset_storage[bitset_descriptors[id].offset + index], 32);
        if (count > expected) return false;
        expected -= count;
    }
    return expected == 0;
}
