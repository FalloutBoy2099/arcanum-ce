#ifndef ARCANUM_GAME_CONTROLLER_H_
#define ARCANUM_GAME_CONTROLLER_H_

#include "game/settings.h"
#include <tig/controller.h>

void controller_settings_register(Settings* settings);
void controller_config_build(Settings* settings, TigControllerConfig* config);
void controller_init(void);

#endif
