#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "gfc_vector.h"
#include "gfc_shape.h"

#include "entity.h"
#include "item.h"

#define PLAYER_MAX 4
#define PLAYER_START_STOCKS 3	// lives each player starts with unless the match says otherwise

/**
 * @brief spawn a player controlled character
 * @param playerIndex 0 for player 1, 1 for player 2, and so on. Decides which input commands (p1_left, p2_left...) it reads.
 * @param characterFile path to the character json, such as "characters/sword.json"
 * @param position where the character's feet start
 * @return NULL on error, the player entity otherwise
 */
Entity* player_spawn(Uint8 playerIndex, const char *characterFile, GFC_Vector3D position);

/**
 * @brief get a player by index
 * @return NULL if there is no player with that index
 */
Entity* player_get(Uint8 playerIndex);

/**
 * @brief set how many lives a player has left (used at the start of a match)
 */
void player_set_stocks(Entity *player, int stocks);

/**
 * @brief lives left for a player. 0 if eliminated or if there is no such player.
 */
int player_get_stocks(Uint8 playerIndex);

/**
 * @brief damage percent of a player. 0 if there is no such player.
 */
float player_get_percent(Uint8 playerIndex);

/**
 * @brief the character's name for a player, or "" if there is no such player
 */
const char *player_get_name(Uint8 playerIndex);

/**
 * @brief get a player's body shape in the world (shape x = world x, shape y = world z)
 */
GFC_Shape player_get_hurtbox(Entity *player);

/**
 * @brief give an item to a player (applies a boost, heals, or hands over a bomb to throw)
 * @return 1 if the player took it, 0 if it had no use (already carrying a bomb, or nothing to heal)
 */
int player_apply_item(Entity *player, const ItemDef *def);

/**
 * @brief an explosion: hurts and launches every player inside the circle, away from its middle
 * @param center middle of the blast (x and z are used)
 * @param radius how far it reaches
 * @param damage percent added to each player hit
 * @param knockback launch speed at 0 percent
 * @param growth extra launch speed per percent
 * @param angle degrees up from horizontal
 */
void player_blast(GFC_Vector3D center, float radius, float damage, float knockback, float growth, float angle);

/**
 * @brief draw every player's hurtbox (blue) and attack hitbox (yellow/red) as see-through boxes.
 * Only draws while the F1 debug view is on. Call in the 3D part of the frame after level_draw_debug.
 */
void player_draw_hitboxes();

#endif
