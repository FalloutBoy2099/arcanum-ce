#ifndef ARCANUM_GAME_OBJECT_QUERY_H_
#define ARCANUM_GAME_OBJECT_QUERY_H_

#include "game/context.h"

typedef bool (*ObjectQueryPredicate)(int64_t obj, void* context);

// Internal query shared by callers that only need the first result. Predicates
// must not move/destroy objects or mutate the sector list while it is traversed.
int64_t object_query_first(int64_t loc, unsigned int types, unsigned int excluded_flags,
    ObjectQueryPredicate predicate, void* context);

// Load/pin then release a sector without copying its tile's objects. Used when
// restoring persistent handles: the caller still performs the original lookup.
bool object_query_load_sector(int64_t loc);

#endif
