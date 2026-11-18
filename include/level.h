#ifndef __LEVEL_H__
#define __LEVEL_H__

#include <SDL_mixer.h>

#include "simple_json.h"

#include "gfc_text.h"
#include "gfc_matrix.h"
#include "gfc_primitives.h"

#include "model.h"

#define LEVEL_MAX_PLATFORMS 32
#define LEVEL_MAX_HAZARDS 32

typedef struct
{
    GFC_Box box;        // world space box. x,y,z = min corner, w,h,d = size along x,y,z
    Uint8 oneWay;       // 1 = can be jumped up through and dropped through, 0 = solid
}LevelPlatform;

typedef enum
{
    HAZARD_JUMP_PAD,    // launches whoever touches it upward
    HAZARD_SPIKES       // hurts and launches whoever touches it
}HazardType;

typedef struct
{
    HazardType type;
    GFC_Box box;        // same format as a platform: x,y,z = min corner, w,h,d = size
    float power;        // jump pad: upward speed. spikes: launch strength
    float damage;       // spikes only: percent added to the player
}LevelHazard;

typedef struct
{
    Model *sky;
    Model *terrain;
    GFC_Matrix4 terrainMat;
    GFC_Box bounds;
    GFC_Vector3D terrainMin;    // lowest corner of the terrain mesh, filled in when the terrain loads
    GFC_Vector3D terrainMax;    // highest corner of the terrain mesh
    LevelPlatform platforms[LEVEL_MAX_PLATFORMS];
    int platformCount;
    LevelHazard hazards[LEVEL_MAX_HAZARDS];
    int hazardCount;
    Mix_Music *bgMusic;
    GFC_TextLine skyFile;
    GFC_TextLine terrainFile;
    GFC_TextLine musicFile;
    SJson *json;
}Level;

/**
 * @brief get the level that is currently loaded
 * @return NULL if no level is loaded, the level otherwise
 */
Level *level_get_the();

/**
 * @brief allocate a blank level
 * @return NULL on error, a new level otherwise
 */
Level *level_new();

/**
 * @brief load a level from a json config file. Becomes "the level" on success.
 * @param fileName path to the level config
 * @return NULL on error, the loaded level otherwise
 */
Level *level_load(const char *fileName);

/**
 * @brief free a level and everything it owns
 */
void level_free(Level *level);

/**
 * @brief queue the sky and terrain for rendering
 */
void level_draw(Level *level);

/**
 * @brief debug view: draws every platform box as a tinted copy of the terrain model.
 * Solid platforms are red, one-way platforms are green. Press F1 to turn it on and off (starts on).
 * Call once per frame after level_draw.
 */
void level_draw_debug(Level *level);

/**
 * @brief 1 if the F1 debug boxes are on (platforms and player hitboxes)
 */
int level_debug_enabled();

/**
 * @brief draw a see-through colored box in the world (debug drawing). Call after level_draw.
 * x,y,z is the lowest corner and w,h,d is the size.
 */
void level_draw_box(Level *level, float x, float y, float z, float w, float h, float d, GFC_Color color);

/**
 * @brief check if a point is inside the level's bounds (the blast zone). Only x and z are checked.
 * @return 1 if inside, or if the level has no bounds set. 0 if outside.
 */
int level_in_bounds(Level *level, float x, float z);

/**
 * @brief test a line segment against the terrain mesh
 * @param level the level to test against
 * @param edge the line segment to test
 * @param contact (optional) if given, set to the hit point closest to the start of the edge
 * @return 1 if the edge hits the terrain, 0 if not (or on error)
 */
int level_line_test(Level *level, GFC_Edge3D edge, GFC_Vector3D *contact);

/**
 * @brief find platform ground under a standing point (the feet of a character)
 * Only lands on a platform if the feet are at or below its top now, and were at or above it on the last tick.
 * @param level the level to test against
 * @param x horizontal position of the feet
 * @param y depth position of the feet
 * @param feetZ height of the feet now
 * @param prevFeetZ height of the feet on the previous tick
 * @param dropThrough if 1, one-way platforms are ignored (character is dropping through them)
 * @param groundZ (optional) set to the height of the top of the platform landed on
 * @return 1 if there is ground to land on or stand on, 0 if not
 */
int level_ground_test(Level *level, float x, float y, float feetZ, float prevFeetZ, Uint8 dropThrough, float *groundZ);

#endif
