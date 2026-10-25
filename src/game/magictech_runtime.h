#ifndef ARCANUM_GAME_MAGICTECH_RUNTIME_H_
#define ARCANUM_GAME_MAGICTECH_RUNTIME_H_

#include "game/magictech.h"

#define MAGICTECH_RUN_CAPACITY 512

bool magictech_run_info_load(MagicTechRunInfo* run_info, TigFile* stream);
bool magictech_runs_load(TigFile* stream, MagicTechRunInfo* destination, int* allocated_count);
bool magictech_map_runs_load(TigFile* stream, MagicTechRunInfo* destination, int* allocated_count, int* loaded_ids, int* loaded_count);
bool magictech_run_object_handles_recover(MagicTechRunInfo* run_info);
bool magictech_run_cost_check_for_mode(int mt_id, bool cheat_mode);
int magictech_fatigue_cost(int64_t obj, int spell, MagicTechAction action, bool cheat_mode);

#endif
