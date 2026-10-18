#ifndef ARCANUM_GAME_BROADCAST_H_
#define ARCANUM_GAME_BROADCAST_H_

#include "game/context.h"

typedef enum BroadcastCommandType {
    BROADCAST_CMD_TYPE_NONE,
    BROADCAST_CMD_TYPE_LEAVE,
    BROADCAST_CMD_TYPE_WAIT,
    BROADCAST_CMD_TYPE_FOLLOW,
    BROADCAST_CMD_TYPE_MOVE,
    BROADCAST_CMD_TYPE_STAY_CLOSE,
    BROADCAST_CMD_TYPE_SPREAD_OUT,
    BROADCAST_CMD_TYPE_CURSE,
    BROADCAST_CMD_TYPE_JOIN,
    BROADCAST_CMD_TYPE_DISBAND,
    BROADCAST_CMD_TYPE_ATTACK,
    BROADCAST_CMD_TYPE_WALK_HERE,
    BROADCAST_CMD_TYPE_BACK_OFF,
    BROADCAST_CMD_TYPE_FOLLOW_OTHER,
    BROADCAST_CMD_TYPE_COUNT,
} BroadcastCommandType;

static inline BroadcastCommandType broadcast_follower_shortcut_command(unsigned int index)
{
    static const BroadcastCommandType commands[] = {
        BROADCAST_CMD_TYPE_WALK_HERE, BROADCAST_CMD_TYPE_ATTACK,
        BROADCAST_CMD_TYPE_STAY_CLOSE, BROADCAST_CMD_TYPE_SPREAD_OUT,
        BROADCAST_CMD_TYPE_BACK_OFF, BROADCAST_CMD_TYPE_MOVE,
    };
    return index < SDL_arraysize(commands) ? commands[index] : BROADCAST_CMD_TYPE_NONE;
}

typedef struct Broadcast {
    int64_t loc;
    char str[128];
} Broadcast;

// Serializeable.
static_assert(sizeof(Broadcast) == 0x88, "wrong size");

typedef void (*BroadcastFloatLineFunc)(int64_t npc_obj, int64_t pc_obj, const char* str, int speech_id);

bool broadcast_init(GameInitInfo* init_info);
void broadcast_exit(void);
void broadcast_set_float_line_func(BroadcastFloatLineFunc func);
void broadcast_msg(int64_t obj, Broadcast* bcast);
void broadcast_msg_client(int64_t pc_obj, Broadcast* bcast);
// Portrait-menu orders use typed commands, independent of translated labels.
bool broadcast_order_follower(int64_t pc_obj, int64_t follower_obj,
    BroadcastCommandType command, int64_t loc, int64_t target_obj, const char* label);
void broadcast_follower_shortcut_execute(unsigned int a1, int64_t loc);

#endif /* ARCANUM_GAME_BROADCAST_H_ */
