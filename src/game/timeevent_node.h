#ifndef ARCANUM_GAME_TIMEEVENT_NODE_H_
#define ARCANUM_GAME_TIMEEVENT_NODE_H_

#include "game/object.h"
#include "game/timeevent.h"

// Private scheduler node. Pooling does not alter its layout or serialization.
typedef struct TimeEventNode {
    TimeEvent te;
    Ryan field_30[TIMEEVENT_PARAM_TYPE_COUNT];
    struct TimeEventNode* next;
    int field_D4;
} TimeEventNode;

#endif
