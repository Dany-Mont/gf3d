#include "simple_logger.h"
#include "simple_json.h"

#include "character.h"

static void character_set_defaults(CharacterDef *def)
{
    memset(def,0,sizeof(CharacterDef));
    gfc_line_cpy(def->name,"unnamed");
    gfc_line_cpy(def->model,"models/dino/dino.model");
    def->scale = 1.0f;

    def->runSpeed = 0.6f;
    def->groundAccel = 0.12f;
    def->groundFriction = 0.15f;
    def->airAccel = 0.04f;
    def->airFriction = 0.01f;
    def->gravity = 0.06f;
    def->maxFallSpeed = 1.2f;
    def->jumpSpeed = 1.0f;
    def->airJumpSpeed = 0.9f;
    def->maxAirJumps = 1;

    def->weight = 1.0f;
    def->strength = 1.0f;
    def->hurtbox = gfc_shape_rect(-1.5f,0,3,5);

    def->attack.hitbox = gfc_shape_rect(1,1,3,2);
    def->attack.startFrame = 6;
    def->attack.activeFrames = 4;
    def->attack.totalFrames = 24;
    def->attack.damage = 8;
    def->attack.baseKnockback = 0.5f;
    def->attack.knockbackGrowth = 0.01f;
    def->attack.knockbackAngle = 40;
}

int character_load(const char *filename, CharacterDef *def)
{
    SJson *json;
    SJson *data;
    SJson *attack;
    SJson *shape;
    const char *str;
    if ((!filename)||(!def))return 0;
    json = sj_load(filename);
    if (!json)
    {
        slog("failed to load character file %s",filename);
        return 0;
    }
    data = sj_object_get_value(json,"character");
    if (!data)
    {
        slog("character file %s is missing the \"character\" object",filename);
        sj_free(json);
        return 0;
    }
    character_set_defaults(def);

    str = sj_object_get_string(data,"name");
    if (str)gfc_line_cpy(def->name,str);
    str = sj_object_get_string(data,"model");
    if (str)gfc_line_cpy(def->model,str);

    sj_object_get_float(data,"scale",&def->scale);
    if (def->scale <= 0)def->scale = 1;
    sj_object_get_float(data,"runSpeed",&def->runSpeed);
    sj_object_get_float(data,"groundAccel",&def->groundAccel);
    sj_object_get_float(data,"groundFriction",&def->groundFriction);
    sj_object_get_float(data,"airAccel",&def->airAccel);
    sj_object_get_float(data,"airFriction",&def->airFriction);
    sj_object_get_float(data,"gravity",&def->gravity);
    sj_object_get_float(data,"maxFallSpeed",&def->maxFallSpeed);
    sj_object_get_float(data,"jumpSpeed",&def->jumpSpeed);
    sj_object_get_float(data,"airJumpSpeed",&def->airJumpSpeed);
    sj_object_get_int(data,"maxAirJumps",&def->maxAirJumps);

    sj_object_get_float(data,"weight",&def->weight);
    sj_object_get_float(data,"strength",&def->strength);
    if (def->weight <= 0)def->weight = 1;

    shape = sj_object_get_value(data,"hurtbox");
    if (shape)gfc_shape_from_json(shape,&def->hurtbox);

    attack = sj_object_get_value(data,"attack");
    if (attack)
    {
        shape = sj_object_get_value(attack,"hitbox");
        if (shape)gfc_shape_from_json(shape,&def->attack.hitbox);
        sj_object_get_int(attack,"startFrame",&def->attack.startFrame);
        sj_object_get_int(attack,"activeFrames",&def->attack.activeFrames);
        sj_object_get_int(attack,"totalFrames",&def->attack.totalFrames);
        sj_object_get_float(attack,"damage",&def->attack.damage);
        sj_object_get_float(attack,"baseKnockback",&def->attack.baseKnockback);
        sj_object_get_float(attack,"knockbackGrowth",&def->attack.knockbackGrowth);
        sj_object_get_float(attack,"knockbackAngle",&def->attack.knockbackAngle);
    }
    // the attack has to end after the hitbox turns off
    if (def->attack.totalFrames < def->attack.startFrame + def->attack.activeFrames)
    {
        def->attack.totalFrames = def->attack.startFrame + def->attack.activeFrames;
    }

    sj_free(json);
    return 1;
}