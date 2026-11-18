#ifndef __ITEM_H__
#define __ITEM_H__

#include "gfc_text.h"
#include "gfc_vector.h"
#include "gfc_color.h"

#include "entity.h"

#define ITEM_TICKS_PER_SECOND 60    // gameplay runs at 60 ticks per second

typedef enum
{
    ITEM_BUFF,      // temporary boost when picked up (mushroom, star, speed boots)
    ITEM_HEAL,      // instantly lowers the player's percent
    ITEM_BOMB       // picked up and carried, thrown with the item button, explodes
}ItemKind;

typedef struct
{
    float duration;         // seconds the boost lasts
    float size;             // multiplies the character's size (and its hurtbox and hitbox)
    float damageMult;       // multiplies the damage this player deals
    float knockbackMult;    // multiplies the knockback this player deals
    float speedMult;        // multiplies run speed and acceleration
    float jumpMult;         // multiplies how HIGH the player jumps (jump speed is scaled by its square root)
    Uint8 invincible;       // 1 = takes no damage or knockback
}ItemBuff;

typedef struct
{
    GFC_TextLine name;
    GFC_TextLine model;
    GFC_Vector3D scale;     // how big the item is drawn
    GFC_Color color;        // tint, so one model can be used for every item
    float pickupRadius;     // circle around the item that a player has to touch to pick it up
    ItemKind kind;

    ItemBuff buff;          // ITEM_BUFF
    float healPercent;      // ITEM_HEAL: how much percent it removes

    // ITEM_BOMB
    float throwSpeed;
    float throwAngle;       // degrees up from horizontal
    float fuse;             // seconds until it explodes on its own
    float blastRadius;
    float blastDamage;
    float blastKnockback;
    float blastKnockbackGrowth;
    float blastAngle;       // degrees up from horizontal, pushing away from the middle of the blast
}ItemDef;

/**
 * @brief load an item definition by name from the "items" list (config/items.json)
 * @param itemName the "name" of the item in the json
 * @param def where to store it
 * @return 1 on success, 0 if the item was not found
 */
int item_def_load(const char *itemName, ItemDef *def);

/**
 * @brief spawn an item lying in the level for players to pick up
 * @param item_name the "name" of the item in config/items.json
 * @param position where it appears. It falls onto the nearest platform below.
 * @return NULL on error, the item entity otherwise
 */
Entity *item_spawn(const char *item_name, GFC_Vector3D position);

/**
 * @brief spawn a thrown bomb. Used by the player when it throws what it is carrying.
 * @param def the item that is thrown
 * @param position where it starts
 * @param facing 1 = thrown to the right, -1 = thrown to the left
 * @param ownerIndex player index of the thrower. The bomb will not explode on contact with its own thrower.
 * @return NULL on error, the bomb entity otherwise
 */
Entity *item_spawn_thrown(const ItemDef *def, GFC_Vector3D position, int facing, Uint8 ownerIndex);

#endif