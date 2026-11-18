#include <stdlib.h>
#include <stdio.h>

#include "simple_logger.h"
#include "simple_json.h"

#include "gf2d_font.h"
#include "gf3d_vgraphics.h"

#include "entity.h"
#include "level.h"
#include "item.h"
#include "player.h"
#include "window.h"
#include "match.h"

// ---- HUD layout, in pixels ----
#define HUD_PANEL_W			300
#define HUD_PANEL_H			96
#define HUD_BOTTOM_MARGIN	14

// ---- Match settings ----
#define MATCH_PLAYERS				2		// how many players a match has
#define MATCH_ITEM_DROP_SECONDS		20		// a random power-up drops this often. 0 turns drops off.
#define MATCH_ITEM_DROP_HEIGHT		40		// how high up they appear (they fall onto the nearest platform)
#define MATCH_ITEM_DROP_RANGE		40		// they appear between -range and +range on the x axis

static GFC_Vector3D match_spawns[MATCH_PLAYERS] =
{
	{-10, 0, 10},
	{ 10, 0, 10}
};

static const char *match_item_names[] =
{
	"mushroom",
	"star",
	"speed_boots",
	"heart",
	"bomb"
};

static struct
{
	int active;
	int startStocks;
	int itemTimer;
	int dropSeconds;	// seconds between random power-up drops, 0 = none (a stage can change it)
}match_state = {0};

int match_is_active()
{
	return match_state.active;
}

/**
 * A stage json can have its own power-ups, for testing:
 *   "itemDropSeconds": 5                      how often a random one drops (0 = never)
 *   "items": [ {"name":"star","x":0,"z":4}, ... ]    (y is optional)    placed when the match starts
 */
static void match_load_stage_items(Level *level)
{
	SJson *data, *list, *entry;
	GFC_Vector3D position;
	const char *name;
	int i, count, seconds;

	match_state.dropSeconds = MATCH_ITEM_DROP_SECONDS;
	if ((!level)||(!level->json))return;
	data = sj_object_get_value(level->json, "level");
	if (!data)return;
	if (sj_object_get_int(data, "itemDropSeconds", &seconds))match_state.dropSeconds = seconds;
	list = sj_object_get_value(data, "items");
	count = sj_array_get_count(list);
	for (i = 0; i < count; i++)
	{
		entry = sj_array_get_nth(list, i);
		name = sj_object_get_string(entry, "name");
		position = gfc_vector3d(0, 0, 5);
		sj_object_get_float(entry, "x", &position.x);
		sj_object_get_float(entry, "y", &position.y);
		sj_object_get_float(entry, "z", &position.z);
		if (name)item_spawn(name, position);
	}
}

void match_start()
{
	char key[32];
	const char *setting;
	Entity *player;
	int i, stocks;

	setting = window_get_setting("stage");
	if ((!setting)||(!level_load(setting)))
	{
		slog("could not start the match: failed to load stage %s", setting ? setting : "(none chosen)");
		window_manager_action("goto", "main_menu");
		return;
	}

	stocks = PLAYER_START_STOCKS;
	setting = window_get_setting("stocks");
	if ((setting)&&(atoi(setting) > 0))stocks = atoi(setting);

	for (i = 0; i < MATCH_PLAYERS; i++)
	{
		snprintf(key, sizeof(key), "p%i_character", i + 1);
		setting = window_get_setting(key);
		if (!setting)
		{
			slog("no character chosen for player %i", i + 1);
			continue;
		}
		player = player_spawn(i, setting, match_spawns[i]);
		if (!player)continue;
		player_set_stocks(player, stocks);
		if (!level_in_bounds(level_get_the(), match_spawns[i].x, match_spawns[i].z))
		{
			slog("WARNING: the spawn point of player %i is outside the level bounds, so they will lose lives right away. Fix \"bounds\" in the level json.", i + 1);
		}
	}

	match_state.active = 1;
	match_state.startStocks = stocks;
	match_state.itemTimer = 0;
	match_load_stage_items(level_get_the());
	slog("match started with %i lives each", stocks);
}

void match_end()
{
	entity_system_free_all();
	level_free(level_get_the());
	match_state.active = 0;
	slog("match ended");
}

static void match_finish(int winner)
{
	char text[128];
	if (winner >= 0)
	{
		snprintf(text, sizeof(text), "Player %i wins!  (%s)", winner + 1, player_get_name(winner));
	}
	else
	{
		snprintf(text, sizeof(text), "Draw");
	}
	slog("match over: %s", text);
	window_set_setting("result_text", text);
	window_manager_open("results_menu");
}

void match_update()
{
	int i, total = 0, alive = 0, lastAlive = -1;
	float x;

	if (!match_state.active)return;

	// ---- power-up drops ----
	if (match_state.dropSeconds > 0)
	{
		match_state.itemTimer++;
		if (match_state.itemTimer >= match_state.dropSeconds * ITEM_TICKS_PER_SECOND)
		{
			match_state.itemTimer = 0;
			x = (rand() % (2 * MATCH_ITEM_DROP_RANGE + 1)) - MATCH_ITEM_DROP_RANGE;
			item_spawn(match_item_names[rand() % 5], gfc_vector3d(x, 0, MATCH_ITEM_DROP_HEIGHT));
		}
	}

	// ---- last player standing wins ----
	for (i = 0; i < PLAYER_MAX; i++)
	{
		if (!player_get(i))continue;
		total++;
		if (player_get_stocks(i) > 0)
		{
			alive++;
			lastAlive = i;
		}
	}
	if ((total >= 2)&&(alive <= 1))
	{
		match_finish(lastAlive);
	}
}

/**
 * Just text at the bottom of the screen for each player: name, damage percent and lives left.
 */
void match_draw_hud()
{
	GFC_Vector2D screen;
	GFC_Color color;
	char text[64];
	float x, y, percent, t;
	int i, slot = 0, count = 0;

	if (!match_state.active)return;
	for (i = 0; i < PLAYER_MAX; i++)
	{
		if (player_get(i))count++;
	}
	if (!count)return;
	screen = gf3d_vgraphics_get_resolution();

	for (i = 0; i < PLAYER_MAX; i++)
	{
		if (!player_get(i))continue;
		x = screen.x * (slot + 1) / (count + 1) - HUD_PANEL_W / 2;
		y = screen.y - HUD_PANEL_H - HUD_BOTTOM_MARGIN;
		slot++;

		// who it is
		snprintf(text, sizeof(text), "P%i  %s", i + 1, player_get_name(i));
		gf2d_font_draw_line_tag(text, FT_Small, GFC_COLOR_WHITE, gfc_vector2d(x, y + 14));

		// percent: white at 0, turning red as it climbs
		percent = player_get_percent(i);
		t = percent / 150.0f;
		if (t > 1)t = 1;
		color = gfc_color(1, 1 - t, 1 - t, 1);
		snprintf(text, sizeof(text), "%i%%", (int)percent);
		gf2d_font_draw_line_tag(text, FT_H1, color, gfc_vector2d(x, y + 38));

		// lives left
		snprintf(text, sizeof(text), "Lives: %i", player_get_stocks(i));
		gf2d_font_draw_line_tag(text, FT_Small, GFC_COLOR_WHITE, gfc_vector2d(x + 150, y + 50));
	}
}