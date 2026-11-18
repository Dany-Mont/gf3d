#ifndef __STAGE_H__
#define __STAGE_H__

#include "entity.h"

/**
* @brief spawn the main floor of the stage as a static entity.
* The top surface of the floor sits at z = 0 (matches PLAYER_FLOOR_Z in player.c).
* @return NULL on error, the floor entity otherwise
*/
Entity* stage_spawn_floor();

#endif
