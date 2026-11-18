#ifndef __MATCH_H__
#define __MATCH_H__

/**
 * A match is one fight: the level, the players, their lives, power-up drops, and the HUD text.
 * The menus decide what goes into it (window settings), game.c starts and ends it.
 */

/**
 * @brief load the chosen stage and spawn the chosen characters, each with the starting number of lives.
 * Reads the window settings stage, p1_character, p2_character and stocks.
 * If the stage cannot be loaded, goes back to the main menu.
 */
void match_start();

/**
 * @brief throw away everything the match created (entities and level)
 */
void match_end();

/**
 * @brief run once per game tick while the match is playing. Drops power-ups and checks for a winner.
 * When someone wins, the results menu opens.
 */
void match_update();

/**
 * @return 1 between match_start and match_end
 */
int match_is_active();

/**
 * @brief draw each player's name, damage percent and lives as plain text. Call in the 2D part of the frame.
 */
void match_draw_hud();

#endif
