#include <string.h>
#include <stdlib.h>

#include "simple_logger.h"
#include "simple_json.h"

#include "gfc_config.h"
#include "gfc_shape.h"

#include "gf2d_sprite.h"
#include "gf2d_mouse.h"

#include "window.h"

typedef struct
{
	char key[48];
	char value[128];
}Setting;

static Window *windows[MAX_WINDOWS];
static int windowCount = 0;
static Window *openWindows[MAX_OPEN];
static int openCount = 0;
static Sprite *white = NULL;
static Setting settings[MAX_SETTINGS];
static int settingCount = 0;
static GameState state = GS_MENU;


void window_draw_box(float x, float y, float w, float h, GFC_Color color)
{
	GFC_Vector2D scale;
	if ((!white)||(w <= 0)||(h <= 0))return;
		scale = gfc_vector2d(w / (white->frameWidth + 1), h / (white->frameHeight + 1));
	gf2d_sprite_draw(white, gfc_vector2d(x, y), &scale, NULL, NULL, NULL, &color, NULL, 0);
}

static void draw_frame(float x, float y, float w, float h)
{
	GFC_Color c = gfc_color(1, 1, 1, 1);
	window_draw_box(x, y, w, 2, c);
	window_draw_box(x, y + h - 2, w, 2, c);
	window_draw_box(x, y, 2, h, c);
	window_draw_box(x + w - 2, y, 2, h, c);
}


const char *window_get_setting(const char *key)
{
	int i;
	for (i = 0; i < settingCount; i++)
	{
		if (strcmp(settings[i].key, key) == 0)return settings[i].value;
	}
	return NULL;
}

void window_set_setting(const char *key, const char *value)
{
	int i;
	for (i = 0; i < settingCount; i++)
	{
		if (strcmp(settings[i].key, key) == 0)
		{
			strncpy(settings[i].value, value, 127);
			return;
		}
	}
	if (settingCount >= MAX_SETTINGS)
	{
		slog("no room for setting %s", key);
		return;
	}
	strncpy(settings[settingCount].key, key, 47);
	strncpy(settings[settingCount].value, value, 127);
	settingCount++;
}


static void load_widgets(Window *win, SJson *list, int isButton)
{
	SJson *item;
	Widget *w;
	const char *str;
	int i, count;
	count = sj_array_get_count(list);
	for (i = 0; (i < count)&&(win->widgetCount < MAX_WIDGETS); i++)
	{
		item = sj_array_get_nth(list, i);
		w = &win->widgets[win->widgetCount++];
		w->isButton = isButton;
		w->style = FT_Normal;
		str = sj_object_get_string(item, "text");
		if (str)strncpy(w->text, str, 127);
			str = sj_object_get_string(item, "text_from");
			if (str)strncpy(w->textFrom, str, 47);
			str = sj_object_get_string(item, "action");
			if (str)strncpy(w->action, str, 47);
			str = sj_object_get_string(item, "param");
			if (str)strncpy(w->param, str, 127);
			str = sj_object_get_string(item, "style");
			if (str)w->style = gf2d_font_type_from_text(str);
			if (w->style == FT_MAX)w->style = FT_Normal;
			sj_object_get_vector2d(item, "position", &w->position);
			sj_object_get_vector2d(item, "size", &w->size);
		}
}

static Window *load_window(const char *filename)
{
	SJson *json, *data;
	Window *win;
	const char *str;
	json = sj_load(filename);
	if (!json)
	{
		slog("could not load window file %s", filename);
		return NULL;
	}
	data = sj_object_get_value(json, "window");
	if (!data)
	{
		slog("window file %s has no window object", filename);
		sj_free(json);
		return NULL;
	}
	win = gfc_allocate_array(sizeof(Window), 1);
	win->state = -1;
	str = sj_object_get_string(data, "name");
	if (str)strncpy(win->name, str, 47);
		sj_object_get_vector2d(data, "position", &win->position);
	sj_object_get_vector2d(data, "size", &win->size);
	str = sj_object_get_string(data, "state");
	if (str)
	{
		if (strcmp(str, "menu") == 0)win->state = GS_MENU;
		else if (strcmp(str, "paused") == 0)win->state = GS_PAUSED;
		else if (strcmp(str, "results") == 0)win->state = GS_RESULTS;
	}
	load_widgets(win, sj_object_get_value(data, "labels"), 0);
	load_widgets(win, sj_object_get_value(data, "buttons"), 1);
	sj_free(json);
	return win;
}

static Window *find_window(const char *name)
{
	int i;
	for (i = 0; i < windowCount; i++)
	{
		if (strcmp(windows[i]->name, name) == 0)return windows[i];
	}
	return NULL;
}

int window_manager_open(const char *name)
{
	Window *win;
	win = find_window(name);
	if (!win)
	{
		slog("no window named %s", name);
		return 0;
	}
	if (openCount >= MAX_OPEN)return 0;
	openWindows[openCount++] = win;
	if (win->state >= 0)state = (GameState)win->state;
	return 1;
}

void window_manager_back()
{
	if (openCount > 0)openCount--;
}

void window_manager_close_all()
{
	openCount = 0;
}

GameState window_manager_get_state()
{
	return state;
}

void window_manager_set_state(GameState newState)
{
	state = newState;
}

void window_manager_action(const char *action, const char *param)
{
	char key[48];
	const char *eq;
	if (strcmp(action, "open") == 0)
	{
		window_manager_open(param);
	}
	else if (strcmp(action, "goto") == 0)
	{
		window_manager_close_all();
		window_manager_open(param);
	}
	else if (strcmp(action, "back") == 0)
	{
		window_manager_back();
	}
	else if (strcmp(action, "set") == 0)
	{
		eq = strchr(param, '=');
		if ((!eq)||((eq - param) > 47))return;
			memset(key, 0, sizeof(key));
			memcpy(key, param, eq - param);
		window_set_setting(key, eq + 1);
	}
	else if (strcmp(action, "start_match") == 0)
	{
		window_manager_close_all();
		state = GS_START_MATCH;
	}
	else if (strcmp(action, "resume") == 0)
	{
		window_manager_back();
		state = GS_PLAYING;
	}
	else if (strcmp(action, "quit") == 0)
	{
		state = GS_QUIT;
	}
	else
	{
		slog("unknown action %s", action);
	}
}


void window_manager_init(const char *indexFile)
{
	SJson *json, *list, *item;
	Window *win;
	const char *str, *key, *value;
		int i, count;
	white = gf2d_sprite_load_image("images/ui/white.png");
	json = sj_load(indexFile);
	if (!json)
	{
		slog("could not load window index %s", indexFile);
		return;
	}
	list = sj_object_get_value(json, "windows");
	count = sj_array_get_count(list);
	for (i = 0; (i < count)&&(windowCount < MAX_WINDOWS); i++)
	{
		str = sj_get_string_value(sj_array_get_nth(list, i));
		win = load_window(str);
		if (win)windows[windowCount++] = win;
	}
	list = sj_object_get_value(json, "settings");
	count = sj_array_get_count(list);
	for (i = 0; i < count; i++)
	{
		item = sj_array_get_nth(list, i);
		key = sj_object_get_string(item, "key");
		value = sj_object_get_string(item, "value");
		if ((key)&&(value))window_set_setting(key, value);
	}
	str = sj_object_get_string(json, "start");
	if (str)window_manager_open(str);
	sj_free(json);
	atexit(window_manager_close);
}

void window_manager_close()
{
	int i;
	for (i = 0; i < windowCount; i++)free(windows[i]);
	windowCount = 0;
	openCount = 0;
	if (white)gf2d_sprite_free(white);
	white = NULL;
}


void window_manager_update()
{
	Window *win;
	Widget *w;
	GFC_Rect rect;
	int i;
	if (openCount <= 0)return;
	win = openWindows[openCount - 1];
	for (i = 0; i < win->widgetCount; i++)
	{
		w = &win->widgets[i];
		if (!w->isButton)continue;
			rect = gfc_rect(win->position.x + w->position.x, win->position.y + w->position.y, w->size.x, w->size.y);
			w->hovered = gf2d_mouse_in_rect(rect);
		if ((w->hovered)&&(gf2d_mouse_button_pressed(0)))
		{
			window_manager_action(w->action, w->param);
			return;
		}
	}
}

static int is_selected(Widget *w)
{
	char key[48];
	const char *eq, *current;
	if (strcmp(w->action, "set") != 0)return 0;
	eq = strchr(w->param, '=');
	if ((!eq)||((eq - w->param) > 47))return 0;
	memset(key, 0, sizeof(key));
	memcpy(key, w->param, eq - w->param);
	current = window_get_setting(key);
	return ((current)&&(strcmp(current, eq + 1) == 0));
}

static void draw_widget(Widget *w, GFC_Vector2D origin)
{
	GFC_Vector2D pos, bounds;
	GFC_Color color;
	const char *text;
	pos = gfc_vector2d(origin.x + w->position.x, origin.y + w->position.y);
	text = w->text;
	if (w->textFrom[0])
	{
		text = window_get_setting(w->textFrom);
		if (!text)text = "";
	}
	if (!w->isButton)
	{
		gf2d_font_draw_line_tag(text, w->style, GFC_COLOR_WHITE, pos);
		return;
	}
	if (is_selected(w))color = gfc_color(0.8f, 0.7f, 0.1f, 1);
		else if (w->hovered)color = gfc_color(0.4f, 0.4f, 0.4f, 1);
		else color = gfc_color(0.2f, 0.2f, 0.2f, 1);
	window_draw_box(pos.x, pos.y, w->size.x, w->size.y, color);
	bounds = gf2d_font_get_bounds_tag(text, w->style);
	gf2d_font_draw_line_tag(text, w->style, GFC_COLOR_WHITE,
		gfc_vector2d(pos.x + (w->size.x - bounds.x) / 2, pos.y + (w->size.y - bounds.y) / 2));
}

void window_manager_draw()
{
	Window *win;
	int i, j;
	for (i = 0; i < openCount; i++)
	{
		win = openWindows[i];
		window_draw_box(win->position.x, win->position.y, win->size.x, win->size.y, gfc_color(0.05f, 0.05f, 0.1f, 0.9f));
		draw_frame(win->position.x, win->position.y, win->size.x, win->size.y);
		for (j = 0; j < win->widgetCount; j++)draw_widget(&win->widgets[j], win->position);
	}
}