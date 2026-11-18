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
    GFC_Box box;
    Uint8 oneWay;

    Uint8 breakable;
    Uint8 triggered;
    Uint8 broken;
    int timer;
    int breakTicks;
    int respawnTicks;

    Uint8 moving;
    GFC_Box startBox;
    GFC_Box prevBox;
    float moveX, moveZ;
    int moveTicks;
    int moveTimer;
    float velX, velZ;
}LevelPlatform;

typedef enum
{
    HAZARD_JUMP_PAD,
    HAZARD_SPIKES,
    HAZARD_GEYSER,
    HAZARD_ICE
}HazardType;

typedef struct
{
    HazardType type;
    GFC_Box box;
    float power;
    float damage;

    int offTicks, warnTicks, onTicks;
    int timer;
}LevelHazard;

typedef struct
{
    Model *sky;
    Model *terrain;
    GFC_Matrix4 terrainMat;
    GFC_Box bounds;
    GFC_Vector3D terrainMin;
    GFC_Vector3D terrainMax;
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

Level *level_get_the();

Level *level_new();

Level *level_load(const char *fileName);

void level_free(Level *level);

void level_draw(Level *level);

void level_draw_debug(Level *level);

void level_update(Level *level);

int level_hazard_state(const LevelHazard *hazard);

int level_platform_carry(Level *level, float x, float y, float feetZ, float *dx, float *dz);

int level_debug_enabled();

void level_draw_box(Level *level, float x, float y, float z, float w, float h, float d, GFC_Color color);

int level_in_bounds(Level *level, float x, float z);

int level_line_test(Level *level, GFC_Edge3D edge, GFC_Vector3D *contact);

int level_ground_test(Level *level, float x, float y, float feetZ, float prevFeetZ, Uint8 dropThrough, float *groundZ);

#endif