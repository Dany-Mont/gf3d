#include <math.h>

#include "simple_logger.h"

#include "gfc_config.h"
#include "gfc_config_def.h"
#include "gfc_shape.h"
#include "simple_json.h"

#include "level.h"
#include "player.h"
#include "item.h"

#define ITEM_GRAVITY	0.04f
#define ITEM_FALL_LIMIT_Z	-60.0f	

typedef struct
{
    ItemDef def;
    Uint8 thrown;      
    Uint8 ownerIndex;   
    int fuse;        
}ItemEntityData;

int item_update(Entity *self);
void item_free(Entity *self);

static void item_def_set_defaults(ItemDef *def)
{
    memset(def,0,sizeof(ItemDef));
    def->scale = gfc_vector3d(1,1,1);
    def->color = GFC_COLOR_WHITE;
    def->pickupRadius = 2;
    def->kind = ITEM_BUFF;
    def->buff.duration = 10;
    def->buff.size = 1;
    def->buff.damageMult = 1;
    def->buff.knockbackMult = 1;
    def->buff.speedMult = 1;
    def->buff.jumpMult = 1;
    def->throwSpeed = 0.7f;
    def->throwAngle = 35;
    def->fuse = 3;
    def->blastRadius = 8;
    def->blastDamage = 15;
    def->blastKnockback = 1.0f;
    def->blastKnockbackGrowth = 0.012f;
    def->blastAngle = 45;
}

int item_def_load(const char *item_name, ItemDef *def)
{
    const char *str;
    SJson *item_def;
    uint8_t flag;
    if ((!item_name)||(!def))
    {
        slog("no item name specified");
        return 0;
    }
    item_def = gfc_config_def_get_by_name("items",item_name);
    if(!item_def)
    {
        slog("failed to load item definition for %s",item_name);
        return 0;
    }
    item_def_set_defaults(def);
    gfc_line_cpy(def->name,item_name);

    str = sj_object_get_string(item_def,"model");
    if(str)
    {
        gfc_line_cpy(def->model,str);
    }
    else
    {
        slog("item definition for %s has no model",item_name);
    }
    sj_object_get_vector3d(item_def,"scale",&def->scale);
    sj_object_get_color_value(item_def,"color",&def->color);
    sj_object_get_float(item_def,"pickupRadius",&def->pickupRadius);

    str = sj_object_get_string(item_def,"kind");
    if (str)
    {
        if (strcmp(str,"buff") == 0)def->kind = ITEM_BUFF;
        else if (strcmp(str,"heal") == 0)def->kind = ITEM_HEAL;
        else if (strcmp(str,"bomb") == 0)def->kind = ITEM_BOMB;
        else slog("item %s has unknown kind %s",item_name,str);
    }

    sj_object_get_float(item_def,"duration",&def->buff.duration);
    sj_object_get_float(item_def,"size",&def->buff.size);
    sj_object_get_float(item_def,"damageMult",&def->buff.damageMult);
    sj_object_get_float(item_def,"knockbackMult",&def->buff.knockbackMult);
    sj_object_get_float(item_def,"speedMult",&def->buff.speedMult);
    sj_object_get_float(item_def,"jumpMult",&def->buff.jumpMult);
    flag = 0;
    sj_object_get_uint8(item_def,"invincible",&flag);
    def->buff.invincible = flag ? 1 : 0;

    sj_object_get_float(item_def,"healPercent",&def->healPercent);

    sj_object_get_float(item_def,"throwSpeed",&def->throwSpeed);
    sj_object_get_float(item_def,"throwAngle",&def->throwAngle);
    sj_object_get_float(item_def,"fuse",&def->fuse);
    sj_object_get_float(item_def,"blastRadius",&def->blastRadius);
    sj_object_get_float(item_def,"blastDamage",&def->blastDamage);
    sj_object_get_float(item_def,"blastKnockback",&def->blastKnockback);
    sj_object_get_float(item_def,"blastKnockbackGrowth",&def->blastKnockbackGrowth);
    sj_object_get_float(item_def,"blastAngle",&def->blastAngle);
    return 1;
}

static Entity *item_create(const ItemDef *def, GFC_Vector3D position)
{
    ItemEntityData *data;
    Entity *self;
    data = gfc_allocate_array(sizeof(ItemEntityData),1);
    if(!data)return NULL;
    self = entity_new();
    if(!self)
    {
        slog("failed to allocate entity for item %s",def->name);
        free(data);
        return NULL;
    }
    data->def = *def;
    if(def->model[0])
    {
        self->model = model_load(def->model);
        if(!self->model)
        {
            slog("could not load model %s", def->model);
        }
    }
    self->scale = def->scale;
    self->colorMod = def->color;
    entity_set_feet_origin(self);
    self->position = position;
    self->update = item_update;
    self->free = item_free;
    self->data = data;
    return self;
}

Entity *item_spawn(const char *item_name, GFC_Vector3D position)
{
    ItemDef def;
    if(!item_def_load(item_name,&def))return NULL;
    return item_create(&def,position);
}

Entity *item_spawn_thrown(const ItemDef *def, GFC_Vector3D position, int facing, Uint8 ownerIndex)
{
    ItemEntityData *data;
    Entity *self;
    float angle;
    if(!def)return NULL;
    self = item_create(def,position);
    if(!self)return NULL;
    data = (ItemEntityData*)self->data;
    data->thrown = 1;
    data->ownerIndex = ownerIndex;
    data->fuse = (int)(def->fuse * ITEM_TICKS_PER_SECOND);
    if (data->fuse < 1)data->fuse = 1;
    angle = def->throwAngle * GFC_PI / 180.0f;
    self->velocity.x = facing * cos(angle) * def->throwSpeed;
    self->velocity.z = sin(angle) * def->throwSpeed;
    return self;
}

void item_free(Entity *self)
{
    if (!self || !self->data)return;
    free(self->data);
    self->data = NULL;
}

int item_update(Entity *self)
{
    ItemEntityData *data;
    GFC_Shape area, hurtbox;
    Entity *player;
    float groundZ = 0;
    int i, explode;
    if (!self || !self->data)return 0;
    data = (ItemEntityData*)self->data;

    if ((self->velocity.z <= 0) &&
        level_ground_test(level_get_the(),self->position.x,self->position.y,
            self->position.z,self->position.z - self->velocity.z,0,&groundZ))
    {
        self->position.z = groundZ;
        self->velocity.z = 0;
        self->velocity.x = 0;
    }
    self->velocity.z -= ITEM_GRAVITY;
    self->velocity.y = 0;

    if (self->position.z < ITEM_FALL_LIMIT_Z)
    {
        entity_free(self);
        return 1;
    }

    area = gfc_shape_circle(self->position.x,
                            self->position.z + data->def.pickupRadius,
                            data->def.pickupRadius);

    if (!data->thrown)
    {
        for (i = 0; i < PLAYER_MAX; i++)
        {
            player = player_get(i);
            if (!player)continue;
            hurtbox = player_get_hurtbox(player);
            if (!gfc_shape_overlap(area,hurtbox))continue;
            if (player_apply_item(player,&data->def))
            {
                slog("player %i picked up %s",i + 1,data->def.name);
                entity_free(self);
                return 1;
            }
        }
    }
    else
    {
        data->fuse--;
        explode = (data->fuse <= 0);
        for (i = 0; (i < PLAYER_MAX)&&(!explode); i++)
        {
            if (i == data->ownerIndex)continue;
            player = player_get(i);
            if (!player)continue;
            hurtbox = player_get_hurtbox(player);
            if (gfc_shape_overlap(area,hurtbox))explode = 1;
        }
        if (explode)
        {
            player_blast(
                gfc_vector3d(self->position.x,self->position.y,self->position.z + data->def.pickupRadius),
                    data->def.blastRadius,
                    data->def.blastDamage,
                    data->def.blastKnockback,
                    data->def.blastKnockbackGrowth,
                    data->def.blastAngle);
            entity_free(self);
            return 1;
        }
    }
    return 1;
}