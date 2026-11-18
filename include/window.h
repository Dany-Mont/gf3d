#ifndef __WINDOW_H__
#define __WINDOW_H__

#include "gfc_vector.h"
#include "gfc_color.h"

#include "gf2d_font.h"

#define MAX_WINDOWS		16		// how many windows can be loaded
#define MAX_OPEN		8		// how many can be open on top of each other
#define MAX_WIDGETS		32		// labels and buttons in one window
#define MAX_SETTINGS	16		// values the menus remember (chosen character, stage...)

/**
 * What the game is doing right now. The menus change it and game.c reacts to it.
 */
typedef enum
{
	GS_MENU,		// a menu is up, no match running
	GS_START_MATCH,	// Start Match was clicked, game.c sets up the match
	GS_PLAYING,		// the match is running
	GS_PAUSED,		// pause menu is up
	GS_RESULTS,		// results menu is up
	GS_QUIT			// quit the game
}GameState;

typedef struct
{
	char text[128];
	char textFrom[48];		// labels only: show this setting instead of text
	char action[48];		// buttons only: "open", "goto", "back", "set", "start_match", "resume", "quit"
	char param[128];
	int isButton;
	GFC_Vector2D position;	// relative to the window
	GFC_Vector2D size;
	FontTypes style;
	Uint8 hovered;
}Widget;

typedef struct
{
	char name[48];
	GFC_Vector2D position;
	GFC_Vector2D size;
	int state;				// GameState to switch to when this window opens, -1 = leave alone
	Widget widgets[MAX_WIDGETS];
	int widgetCount;
}Window;

/**
 * @brief load every window listed in the index json, set the starting settings, open the start window
 */
void window_manager_init(const char *indexFile);

/**
 * @brief free every window
 */
void window_manager_close();

/**
 * @brief check the mouse against the buttons of the top window. Call once per frame.
 */
void window_manager_update();

/**
 * @brief draw the open windows. Call in the 2D part of the frame, before the mouse.
 */
void window_manager_draw();

/**
 * @brief open a window on top of the others by name
 * @return 1 if opened, 0 if there is no window with that name
 */
int window_manager_open(const char *name);

/**
 * @brief close the top window
 */
void window_manager_back();

/**
 * @brief close every window
 */
void window_manager_close_all();

/**
 * @brief do what a button click does.
 * Actions: "open", "goto", "back", "set" (param is "key=value"), "start_match", "resume", "quit"
 */
void window_manager_action(const char *action, const char *param);

/**
 * @brief draw a filled box in screen pixels (the HUD uses this too)
 */
void window_draw_box(float x, float y, float w, float h, GFC_Color color);

GameState window_manager_get_state();
void window_manager_set_state(GameState state);

/**
 * @brief get a value the menus remember, like "p1_character". NULL if not set.
 */
const char *window_get_setting(const char *key);

/**
 * @brief remember a value
 */
void window_set_setting(const char *key, const char *value);

#endif
