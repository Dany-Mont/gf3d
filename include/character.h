#ifndef __CHARACTER_H__
#define __CHARACTER_H__

#include "gfc_text.h"
#include "gfc_shape.h"

/**
 * Shapes in this file are 2D (gfc_shape). They live on the fight plane, so
 * shape x = world x (left/right) and shape y = world z (up/down).
 * Everything is relative to the character's feet and written as if the
 * character is facing right. The player code mirrors it when facing left.
 */

typedef struct
{
    GFC_Shape hitbox;       // area that hurts, relative to the feet, facing right
    int startFrame;         // ticks after the attack begins before the hitbox turns on
    int activeFrames;       // ticks the hitbox stays on
    int totalFrames;        // ticks until the attack is over and another can start
    float damage;           // percent added to the target (before the attacker's strength)
    float baseKnockback;    // launch speed at 0 percent
    float knockbackGrowth;  // extra launch speed for each percent the target has
    float knockbackAngle;   // degrees up from horizontal, pushing away from the attacker
}AttackDef;

typedef struct
{
    GFC_TextLine name;
    GFC_TextLine model;
    float scale;            // how big the model is (1 = normal). The hurtbox and hitbox grow with it.

    // movement, in units per tick (60 ticks per second)
    float runSpeed;
    float groundAccel;
    float groundFriction;
    float airAccel;
    float airFriction;
    float gravity;
    float maxFallSpeed;
    float jumpSpeed;
    float airJumpSpeed;
    int maxAirJumps;

    // fighting
    float weight;           // heavier = launched less
    float strength;         // stronger = hits harder
    GFC_Shape hurtbox;      // the body, relative to the feet
    AttackDef attack;
}CharacterDef;

/**
 * @brief load a character definition from a json file. Anything missing from the file gets a default value.
 * @param filename path to the character json
 * @param def where to store the result
 * @return 1 on success, 0 if the file could not be read
 */
int character_load(const char *filename, CharacterDef *def);

#endif
