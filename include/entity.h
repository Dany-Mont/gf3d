#ifndef __ENTITY_H__
#define __ENTITY_H__

#include "gfc_primitives.h"

#include "model.h"

typedef struct Entity_S
{
	Uint8 _inuse;
	Model* model;
	GFC_Vector3D position;
	GFC_Vector3D rotation;
	GFC_Vector3D scale;
	GFC_Matrix4 mat;
	GFC_Vector3D velocity;
	GFC_Vector3D drawOffset; // shifts where the model is drawn without moving the entity (used to put a model's feet at its position)
	Uint8 grounded; // 1 if standing on solid ground this tick, 0 if in the air
	GFC_Color colorMod;
	GFC_Box bounds; // collision box, relative to the entity's position (x,y,z = min corner, w,h,d = size along x,y,z)

	int (*think)(struct Entity_S *self);
	int (*update)(struct Entity_S* self);
	int (*draw)(struct Entity_S* self);

	void (*free)(struct Entity_S* self); // cleans up custom data
	void* data; // used for custom entity data. If set, you must specify a FREE function.
}Entity;

/**
* @brief initialize the entity manager and pre-allocate a bunch
* @param maxEnts how many entities can exist at any single moment
*/
void entity_init(Uint32 maxEnts);

/**
* @brief get a blank entity from the system
* @return NULL on error or out of memory, a valid pointer otherwise
*/
Entity* entity_new();

/**
* @brief call to clean up an entity after it is no longer needed
* @param ent the entity to clean up
*/
void entity_free(Entity* ent);

/**
* @brief run think functions on all active entities
*/
void entity_system_think();

/**
* @brief run updates on all active entities
*/
void entity_system_update();

/**
* @brief shift how an entity's model is drawn so the lowest point of the model sits at the entity's position.
* Call after the model (and scale) are set. Logs the model's extents so the numbers can be checked.
* @param ent the entity to adjust
*/
void entity_set_feet_origin(Entity *ent);

/**
* @brief check if the collision boxes of two entities overlap
* @param self one entity
* @param other the other entity
* @return 1 if their bounds overlap, 0 if not (or if either is invalid, or they are the same entity)
*/
int entity_collision_other(Entity *self, Entity *other);

/**
* @brief free every active entity (used when a match ends)
*/
void entity_system_free_all();

/**
* @brief queue renders on all active entities
*/
void entity_system_draw();

#endif