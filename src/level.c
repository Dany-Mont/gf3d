#include <string.h>
#include <SDL.h>

#include "simple_logger.h"

#include "gfc_primitives.h"

#include "gf3d_obj_load.h"

#include "level.h"


static Level *theLevel = NULL;
Level * level_get_the()
{
    return theLevel;
}

int level_line_test(Level *level, GFC_Edge3D edge, GFC_Vector3D *contact)
{
    Mesh *mesh;
    MeshPrimitive *prim;
    GFC_Triangle3D t;
    int i,c,j;
    int found = 0;
    float dist, bestDist = 0;
    GFC_Vector3D start, diff;
    GFC_Vector3D out = {0};

    if ((!level)||(!level->terrain)||(!level->terrain->mesh))return 0;
    mesh = level->terrain->mesh;
    start = edge.a;
    c = gfc_list_count(mesh->primitives);
    for(i = 0; i < c; i++)
    {
        prim = gfc_list_nth(mesh->primitives,i);
        if ((!prim)||(!prim->objData))
        {
            slog("level_line_test: bad primitive %i of %i for mesh %s",i,c,mesh->filename);
            continue;
        }
        for(j = 0; j < prim->objData->face_count; j++)
        {
            t.a = prim->objData->faceVertices[prim->objData->outFace[j].verts[0]].vertex;
                t.b = prim->objData->faceVertices[prim->objData->outFace[j].verts[1]].vertex;
               t.c = prim->objData->faceVertices[prim->objData->outFace[j].verts[2]].vertex;
            if (gfc_triangle_edge_test(edge,t,&out))
            {
                // keep the hit closest to where the line starts
                gfc_vector3d_sub(diff,out,start);
                dist = gfc_vector3d_magnitude_squared(diff);
                if ((!found)||(dist < bestDist))
                {
                    bestDist = dist;
                    if (contact)*contact = out;
                    found = 1;
                }
            }
        }
    }
    return found;
}


// Logs the lowest and highest corner of the terrain mesh so platform boxes in the level json can be matched to it exactly.
static void level_log_terrain_extents(Level *level)
{
    Mesh *mesh;
    MeshPrimitive *prim;
    GFC_Vector3D v, lo = {0}, hi = {0};
    int i,c,j,k;
    int found = 0;
    if ((!level)||(!level->terrain)||(!level->terrain->mesh))return;
    mesh = level->terrain->mesh;
    c = gfc_list_count(mesh->primitives);
    for(i = 0; i < c; i++)
    {
        prim = gfc_list_nth(mesh->primitives,i);
        if ((!prim)||(!prim->objData))continue;
        for(j = 0; j < prim->objData->face_count; j++)
        {
            for(k = 0; k < 3; k++)
            {
                v = prim->objData->faceVertices[prim->objData->outFace[j].verts[k]].vertex;
                if (!found)
                {
                    lo = v;
                    hi = v;
                    found = 1;
                    continue;
                }
                if (v.x < lo.x)lo.x = v.x;
                if (v.y < lo.y)lo.y = v.y;
                if (v.z < lo.z)lo.z = v.z;
                if (v.x > hi.x)hi.x = v.x;
                if (v.y > hi.y)hi.y = v.y;
                if (v.z > hi.z)hi.z = v.z;
            }
        }
    }
    if (found)
    {
        level->terrainMin = lo;
        level->terrainMax = hi;
        slog("terrain extents: min (%f, %f, %f)  max (%f, %f, %f)  size (%f, %f, %f)",
            lo.x,lo.y,lo.z,hi.x,hi.y,hi.z,hi.x - lo.x,hi.y - lo.y,hi.z - lo.z);
    }
}

int level_in_bounds(Level *level, float x, float z)
{
    if (!level)return 1;
    if ((level->bounds.w <= 0)||(level->bounds.d <= 0))return 1;   // no bounds set in the json
    if ((x < level->bounds.x)||(x > level->bounds.x + level->bounds.w))return 0;
    if ((z < level->bounds.z)||(z > level->bounds.z + level->bounds.d))return 0;
    return 1;
}

#define LEVEL_GROUND_TOLERANCE 0.01f

int level_ground_test(Level *level, float x, float y, float feetZ, float prevFeetZ, Uint8 dropThrough, float *groundZ)
{
    LevelPlatform *platform;
    int i;
    int found = 0;
    float top, best = 0;
    if (!level)return 0;
    for(i = 0; i < level->platformCount; i++)
    {
        platform = &level->platforms[i];
        if ((platform->oneWay)&&(dropThrough))continue;
        if ((x < platform->box.x)||(x > platform->box.x + platform->box.w))continue;
        if ((y < platform->box.y)||(y > platform->box.y + platform->box.h))continue;
        top = platform->box.z + platform->box.d;
        if (feetZ > top)continue;                                   // still above it, nothing to land on yet
        if (prevFeetZ < top - LEVEL_GROUND_TOLERANCE)continue;      // was already below the top last tick (came from underneath)
        if ((!found)||(top > best))
        {
            best = top;
            found = 1;
        }
    }
    if ((found)&&(groundZ))*groundZ = best;
    return found;
}


Level * level_load(const char *fileName)
{
    SJson *json;
    SJson *data;
    SJson *bounds;
    SJson *platforms;
    SJson *entry;
    LevelPlatform *platform;
    LevelHazard *hazard;
    int i,count,oneWay;
    const char *str;
    Level *level;
    json = sj_load(fileName);
    if(!json)
    {
        slog("failed to load level file %s",fileName);
        return NULL;
    }
    data = sj_object_get_value(json,"level");
    if(!data)
    {
        slog("level file %s missing level data",fileName);
        sj_free(json);
        return NULL;
    }
    level = level_new();
    if(!level)
    {
        slog("failed to allocate new level");
        sj_free(json);
        return NULL;
    }
    str = sj_object_get_string(data,"sky");
    if(str)
    {
        level->sky = model_load(str);
        if(level->sky)
        {
            gfc_line_cpy(level->skyFile,str);
        }
    }
    str = sj_object_get_string(data,"terrain");
    if(str)
    {
        level->terrain = model_load(str);
        if(level->terrain)
        {
            gfc_line_cpy(level->terrainFile,str);
            level_log_terrain_extents(level);
        }
    }
    bounds = sj_object_get_value(data,"bounds");
    if(bounds)
    {
        level->bounds = gfc_box_from_config(bounds);
    }
    platforms = sj_object_get_value(data,"platforms");
    if(platforms)
    {
        count = sj_array_get_count(platforms);
        for(i = 0; (i < count)&&(level->platformCount < LEVEL_MAX_PLATFORMS); i++)
        {
            entry = sj_array_get_nth(platforms,i);
            if(!entry)continue;
            platform = &level->platforms[level->platformCount];
            platform->box = gfc_box_from_config(entry);
            oneWay = 0;
            sj_object_get_int(entry,"oneway",&oneWay);
            platform->oneWay = oneWay ? 1 : 0;
            slog("level platform %i: x %f y %f z %f  w %f h %f d %f  oneway %i",
                level->platformCount,
                platform->box.x,platform->box.y,platform->box.z,
                platform->box.w,platform->box.h,platform->box.d,
                platform->oneWay);
            level->platformCount++;
        }
    }
    // hazards: "hazards": [ {"type":"jump_pad","m":[x,y,z],"s":[w,h,d],"power":2.0},
    //                       {"type":"spikes","m":[...],"s":[...],"damage":8,"power":0.9} ]
    platforms = sj_object_get_value(data,"hazards");
    if(platforms)
    {
        count = sj_array_get_count(platforms);
        for(i = 0; (i < count)&&(level->hazardCount < LEVEL_MAX_HAZARDS); i++)
        {
            entry = sj_array_get_nth(platforms,i);
            if(!entry)continue;
            str = sj_object_get_string(entry,"type");
            if(!str)continue;
            hazard = &level->hazards[level->hazardCount];
            if (strcmp(str,"jump_pad") == 0)hazard->type = HAZARD_JUMP_PAD;
            else if (strcmp(str,"spikes") == 0)hazard->type = HAZARD_SPIKES;
            else
            {
                slog("unknown hazard type %s",str);
                continue;
            }
            hazard->box = gfc_box_from_config(entry);
            hazard->power = (hazard->type == HAZARD_JUMP_PAD) ? 2.0f : 0.9f;
            hazard->damage = 8;
            sj_object_get_float(entry,"power",&hazard->power);
            sj_object_get_float(entry,"damage",&hazard->damage);
            level->hazardCount++;
        }
    }
    str = sj_object_get_string(data,"music");
    if(str)
    {
        level->bgMusic = Mix_LoadMUS(str);
        if(level->bgMusic)
        {
            gfc_line_cpy(level->musicFile,str);
            Mix_PlayMusic(level->bgMusic,-1);
        }
        else
        {
            slog("failed to load level music %s: %s",str,Mix_GetError());
        }
    }
    level->json = json;

    // only one level is active at a time
    if ((theLevel)&&(theLevel != level))level_free(theLevel);
    theLevel = level;

    return level;
}

Level * level_new()
{
    Level *level;
    level = gfc_allocate_array(sizeof(Level),1);
    if (!level)
    {
        slog("failed to allocate new level");
        return NULL;
    }

    gfc_matrix4_identity(level->terrainMat);

    return level;
}

void level_free(Level *level)
{
    if (!level)return;
    if (level == theLevel)theLevel = NULL;
    if (level->terrain)model_free(level->terrain);
    if (level->sky)model_free(level->sky);
    if (level->bgMusic)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(level->bgMusic);
    }
    if (level->json)sj_free(level->json);
    free(level);
}


static void level_draw_hazards(Level *level)
{
    LevelHazard *hazard;
    GFC_Color color;
    int i;
    for (i = 0; i < level->hazardCount; i++)
    {
        hazard = &level->hazards[i];
        color = (hazard->type == HAZARD_JUMP_PAD) ? gfc_color(0.1f,0.9f,0.3f,1) : gfc_color(0.85f,0.1f,0.1f,1);
        level_draw_box(level,hazard->box.x,hazard->box.y,hazard->box.z,hazard->box.w,hazard->box.h,hazard->box.d,color);
    }
}

void level_draw(Level *level)
{
    GFC_Matrix4 skyMat;
    if (!level)return;
    if (level->sky)
    {
        gfc_matrix4_identity(skyMat);
        model_queue_render_sky(level->sky,skyMat,GFC_COLOR_WHITE);
    }
    if (level->terrain)
    {
        model_queue_render(level->terrain,level->terrainMat,GFC_COLOR_WHITE);
    }
    level_draw_hazards(level);
}

static Uint8 levelDebugOn = 1;

int level_debug_enabled()
{
    return levelDebugOn;
}

void level_draw_box(Level *level, float x, float y, float z, float w, float h, float d, GFC_Color color)
{
    GFC_Matrix4 mat;
    GFC_Vector3D modelSize, modelCenter, scale, position;
    if ((!level)||(!level->terrain))return;
    modelSize = gfc_vector3d(level->terrainMax.x - level->terrainMin.x,
                             level->terrainMax.y - level->terrainMin.y,
                             level->terrainMax.z - level->terrainMin.z);
    if ((modelSize.x <= 0)||(modelSize.y <= 0)||(modelSize.z <= 0))return;//terrain is not a box we can scale
    modelCenter = gfc_vector3d((level->terrainMin.x + level->terrainMax.x) / 2,
                               (level->terrainMin.y + level->terrainMax.y) / 2,
                               (level->terrainMin.z + level->terrainMax.z) / 2);
    // the terrain model is a box, so stretch a copy of it to the size we want
    scale = gfc_vector3d(w / modelSize.x, h / modelSize.y, d / modelSize.z);
    // the model is scaled around its own origin, so shift it so its center lands on the center of the box
    position = gfc_vector3d(x + w / 2 - modelCenter.x * scale.x,
                            y + h / 2 - modelCenter.y * scale.y,
                            z + d / 2 - modelCenter.z * scale.z);
    gfc_matrix4_from_vectors(mat,position,gfc_vector3d(0,0,0),scale);
    model_queue_render(level->terrain,mat,color);
}

// F1 turns the debug boxes on and off (platforms here, hitboxes in player.c)
void level_draw_debug(Level *level)
{
    static Uint8 f1Last = 0;
    const Uint8 *keys;
    Uint8 f1Now;
    LevelPlatform *platform;
    GFC_Color color;
    int i;

    keys = SDL_GetKeyboardState(NULL);
    f1Now = keys[SDL_SCANCODE_F1];
    if ((f1Now)&&(!f1Last))levelDebugOn = !levelDebugOn;
    f1Last = f1Now;

    if ((!levelDebugOn)||(!level))return;
    for(i = 0; i < level->platformCount; i++)
    {
        platform = &level->platforms[i];
        color = platform->oneWay ? gfc_color(0,1,0,0.4) : gfc_color(1,0,0,0.4);
        level_draw_box(level,
            platform->box.x - platform->box.w * 0.01, platform->box.y - platform->box.h * 0.01, platform->box.z - platform->box.d * 0.01,
            platform->box.w * 1.02, platform->box.h * 1.02, platform->box.d * 1.02, color);
    }
}