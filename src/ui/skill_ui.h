#ifndef ARCANUM_UI_SKILL_UI_H_
#define ARCANUM_UI_SKILL_UI_H_

#include "game/context.h"
#include "game/target.h"

bool skill_ui_init(GameInitInfo* init_info);
void skill_ui_reset(void);
void skill_ui_exit(void);
int skill_ui_action_button_art_num_get(int index);
int skill_ui_action_icon_art_num_get(int index);
int skill_ui_selected_action_get(void);
void skill_ui_action_activate(int64_t obj, int type);
void skill_ui_preprocess(int64_t obj, int type);
void skill_ui_cancel(void);
void skill_ui_apply(TargetDescriptor* td);
void skill_ui_activate(TargetDescriptor* td, int64_t obj, int a3);
int skill_ui_action_skill_get(int index);
int skill_ui_target_skill_resolve(int skill, TargetDescriptor* td);
char* skill_ui_action_name_get(int index);
void skill_ui_adjust_skill(int64_t obj, int skill, int action);
void skill_ui_set_training(int64_t obj, int skill, int training);
void skill_ui_inc_skill(int64_t obj, int skill);
void skill_ui_dec_skill(int64_t obj, int skill);

#endif /* ARCANUM_UI_SKILL_UI_H_ */
