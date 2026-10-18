#ifndef ARCANUM_GAME_PROTO_H_
#define ARCANUM_GAME_PROTO_H_

#include "game/context.h"
#include "game/obj.h"

bool proto_init(GameInitInfo* init_info);
void proto_exit(void);
int64_t proto_default_for_type_get(ObjectType object_type);
int64_t proto_handle_get(int number);
bool proto_is_valid(int description);
int proto_number_from_instance(int64_t obj);

#endif /* ARCANUM_GAME_PROTO_H_ */
