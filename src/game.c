#include <SDL.h>            

#include "simple_json.h"
#include "simple_logger.h"

#include "gfc_input.h"
#include "gfc_config_def.h"
#include "gfc_vector.h"
#include "gfc_matrix.h"
#include "gfc_audio.h"
#include "gfc_string.h"
#include "gfc_actions.h"

#include "gf2d_sprite.h"
#include "gf2d_font.h"
#include "gf2d_actor.h"
#include "gf2d_mouse.h"

#include "gf3d_vgraphics.h"
#include "gf3d_pipeline.h"
#include "gf3d_swapchain.h"
#include "gf3d_mesh.h"

#include "model.h"
#include "entity.h"
#include "level.h"
#include "item.h"
#include "player.h"
#include "window.h"
#include "match.h"

extern int __DEBUG;

static int _done = 0;
static Uint32 frame_delay = 16;
static float fps = 0;

// Gameplay runs at a fixed 60 ticks per second, independent of render speed
#define TICK_MS				(1000.0f / 60.0f)
#define MAX_TICKS_PER_FRAME	5

void parse_arguments(int argc,char *argv[]);
void game_frame_delay();

void exitGame()
{
    _done = 1;
}


int main(int argc,char *argv[])
{
    //local variables
    Uint32 now, last;
    float accumulator = 0;
    int ticks;
    GameState state;
    //initialization    
    parse_arguments(argc,argv);
    init_logger("gf3d.log",0);
    slog("gf3d begin");
    //gfc init
    // the joystick subsystem has to be running BEFORE gfc_input_init opens the pads
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0)slog("joystick init failed: %s",SDL_GetError());
    slog("joysticks found by SDL: %i",SDL_NumJoysticks());
    gfc_input_init("config/input.cfg");
    gfc_config_def_init();
    gfc_config_def_load("data/items.json");
    gfc_action_init(1024);
    //gf3d init
    gf3d_vgraphics_init("config/setup.cfg");
    gf2d_font_init("config/font.cfg");
    gf2d_actor_init(1000);
    entity_init(1000);
    
    //game init
    srand(SDL_GetTicks());
    slog_sync();
    gf2d_mouse_load("actors/mouse.actor");
    window_manager_init("config/windows/windows.json");   // opens the main menu
    last = SDL_GetTicks();
    // main game loop    
    while(!_done)
    {
        now = SDL_GetTicks();
        accumulator += (now - last);
        last = now;

        gfc_input_update();
        gfc_input_controller_slog(0);   // debug: prints which buttons are held on pad 0
        gf2d_mouse_update();
        gf2d_font_update();


        // ---- what the game is doing is decided by the menus (see window.h) ----
        state = window_manager_get_state();
        if (state == GS_START_MATCH)
        {
            match_start();
            // if the stage could not load, match_start already sent us back to the main menu
            if (match_is_active())window_manager_set_state(GS_PLAYING);
        }
        else if ((match_is_active())&&(state == GS_MENU))
        {

            match_end();       // back at a menu: throw the match away
        }
        else if (state == GS_QUIT)
        {
            _done = 1;
        }
        // ---- pause ----
        state = window_manager_get_state();
        if ((state == GS_PLAYING)&&(gfc_input_command_pressed("pause")))
        {
            window_manager_open("pause_menu");
        }
        else if ((state == GS_PAUSED)&&(gfc_input_command_pressed("pause")))
        {
            window_manager_action("resume","");
        }
        // ---- menus ----
        window_manager_update();

        // ---- the match: only runs while playing, so pause and results freeze it ----
        state = window_manager_get_state();
        if (state == GS_PLAYING)
        {
            // thinks (input)
            entity_system_think();

            // updates, on the fixed timestep (may run 0 or more times per frame)
            ticks = 0;
            while ((accumulator >= TICK_MS)&&(ticks < MAX_TICKS_PER_FRAME))
            {
                entity_system_update();
                match_update();    // power-up drops and win check
                accumulator -= TICK_MS;
                ticks++;
                if (window_manager_get_state() != GS_PLAYING)break;    // somebody won
            }
            if (ticks == MAX_TICKS_PER_FRAME) accumulator = 0; // fell too far behind, drop the backlog
        }
        else
        {
            accumulator = 0;   // so the game does not jump ahead after a pause
        }
        // 3D draws
        gf3d_vgraphics_render_start();
        if (match_is_active())
        {
            level_draw(level_get_the());
            level_draw_debug(level_get_the());
            player_draw_hitboxes();
            entity_system_draw();
        }
        // 2D draws
        if (match_is_active())
        {
            match_draw_hud();
            if (window_manager_get_state() == GS_PLAYING)
            {
                gf2d_font_draw_line_tag("ESC to pause",FT_Small,GFC_COLOR_WHITE, gfc_vector2d(10,10));
            }
        }
        window_manager_draw();
        gf2d_mouse_draw();
        gf3d_vgraphics_render_end();
        if (gfc_input_command_down("exit"))_done = 1; // exit condition
        game_frame_delay();
    }    
    vkDeviceWaitIdle(gf3d_vgraphics_get_default_logical_device());    
    //cleanup
    slog("gf3d program end");
    exit(0);
    slog_sync();
    return 0;
}

void parse_arguments(int argc,char *argv[])
{
    int a;

    for (a = 1; a < argc;a++)
    {
        if (strcmp(argv[a],"--debug") == 0)
        {
            __DEBUG = 1;
        }
    }    
}

void game_frame_delay()
{
    Uint32 diff;
    static Uint32 now;
    static Uint32 then;
    then = now;
    slog_sync();// make sure logs get written when we have time to write it
    now = SDL_GetTicks();
    diff = (now - then);
    if (diff < frame_delay)
    {
        SDL_Delay(frame_delay - diff);
    }
    fps = 1000.0/MAX(SDL_GetTicks() - then,0.001);
        //slog("fps: %f",fps);
}
/*eol@eof*/