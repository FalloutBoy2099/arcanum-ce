#ifndef ARCANUM_GAME_RACE_IDENTITY_H_
#define ARCANUM_GAME_RACE_IDENTITY_H_

#include <stdbool.h>

const char* race_identity_marker(int race);
bool race_identity_portrait_matches(int race, int gender, bool npc, const char* name);
bool race_identity_background_matches(int race, int gender, bool npc, const char* conditions);

#endif
