#include "simple_logger.h"

#include "gf3d_obj_load.h"

#include "entity.h"

typedef struct
{
	Entity* entityList;
	Uint32 maxEnts;
	// OTHER STUFF HERE
}EntityManager;

static EntityManager entity_manager = {0};

void entity_close();

void entity_init(Uint32 maxEnts)
{
	if (!maxEnts)
	{
		slog("failed to initialize entity system: maxEnts is zero");
		return;
	}
	entity_manager.entityList = gfc_allocate_array(sizeof(Entity), maxEnts);
	if (!entity_manager.entityList)
	{
		slog("failed to allocate entity list for %i entities", maxEnts);
		return;
	}
	entity_manager.maxEnts = maxEnts;
	slog("Entity system initialized");
	atexit(entity_close);
}

Entity* entity_new()
{
	int i;
	if (!entity_manager.entityList) return NULL;
	for (i = 0; i < entity_manager.maxEnts; i++)
	{
		if (entity_manager.entityList[i]._inuse) continue;
		entity_manager.entityList[i]._inuse = 1;

		// ANY BOILERPLATE CODE GOES HERE
		entity_manager.entityList[i].scale = gfc_vector3d(1, 1, 1);
		entity_manager.entityList[i].colorMod = GFC_COLOR_WHITE;

		return &entity_manager.entityList[i];
	}
	return NULL;
}

void entity_free(Entity* ent)
{
	if (!ent || !ent->_inuse) return;
	if (ent->free) ent->free(ent);
	model_free(ent->model);
	memset(ent, 0, sizeof(Entity));
}

void entity_think(Entity* self)
{
	if (!self || !self->_inuse) return;
	if (self->think)
	{
		if (self->think(self) == 1) return;
	}
}

void entity_system_think()
{
	int i;
	if (!entity_manager.entityList) return;
	for (i = 0; i < entity_manager.maxEnts; i++)
	{
		if (!entity_manager.entityList[i]._inuse) continue;
		entity_think(&entity_manager.entityList[i]);
	}
}

void entity_update(Entity* self)
{
	if (!self || !self->_inuse) return;
	gfc_vector3d_add(self->position, self->position, self->velocity);

	// NOTE: the old global dampening was removed. Friction, gravity, and
	// speed limits are now handled by each entity's own update function
	// (see player_update), so they can differ per entity and per state.

	if (self->update)
	{
		if (self->update(self) == 1) return;
	}
}

void entity_system_update()
{
	int i;
	if (!entity_manager.entityList) return;
	for (i = 0; i < entity_manager.maxEnts; i++)
	{
		if (!entity_manager.entityList[i]._inuse) continue;
		entity_update(&entity_manager.entityList[i]);
	}
}

void entity_set_feet_origin(Entity *ent)
{
	Mesh *mesh;
	MeshPrimitive *prim;
	GFC_Vector3D v, lo = {0}, hi = {0};
	int i,c,j,k;
	int found = 0;
	if ((!ent)||(!ent->model)||(!ent->model->mesh))return;
	mesh = ent->model->mesh;
	c = gfc_list_count(mesh->primitives);
	for (i = 0; i < c; i++)
	{
		prim = gfc_list_nth(mesh->primitives,i);
		if ((!prim)||(!prim->objData))continue;
		for (j = 0; j < prim->objData->face_count; j++)
		{
			for (k = 0; k < 3; k++)
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
	if (!found)
	{
		slog("entity_set_feet_origin: could not read the mesh of the model");
		return;
	}
	slog("entity model extents: min (%f, %f, %f)  max (%f, %f, %f)",lo.x,lo.y,lo.z,hi.x,hi.y,hi.z);
	ent->drawOffset.z = -lo.z * ent->scale.z;
}

int entity_collision_other(Entity *self, Entity *other)

{
	GFC_Box a,b;
	if ((!self)||(!other))return 0;
	if (self == other)return 0;
	if ((!self->_inuse)||(!other->_inuse))return 0;
	a = self->bounds;
	b = other->bounds;
	// bounds are relative to the entity, so move them into world space
	gfc_vector3d_add(a,a,self->position);
	gfc_vector3d_add(b,b,other->position);

	// boxes overlap only if they overlap on every axis
	if ((a.x + a.w <= b.x)||(b.x + b.w <= a.x))return 0;
	if ((a.y + a.h <= b.y)||(b.y + b.h <= a.y))return 0;
	if ((a.z + a.d <= b.z)||(b.z + b.d <= a.z))return 0;
	return 1;
}



void entity_draw(Entity* ent)
{
	GFC_Vector3D drawPosition;
	if (!ent || !ent->_inuse) return;
	if (ent->draw)
	{
		if (ent->draw(ent) == 1) return;
	}
	gfc_vector3d_add(drawPosition, ent->position, ent->drawOffset);
	gfc_matrix4_from_vectors(ent->mat, drawPosition, ent->rotation, ent->scale);
	model_queue_render(ent->model, ent->mat, ent->colorMod);
}

void entity_system_draw()
{
	int i;
	if (!entity_manager.entityList) return;
	for (i = 0; i < entity_manager.maxEnts; i++)
	{
		if (!entity_manager.entityList[i]._inuse) continue;
		entity_draw(&entity_manager.entityList[i]);
	}
}

void entity_system_free_all()
{
	int i;
	if (!entity_manager.entityList) return;
	for (i = 0; i < entity_manager.maxEnts; i++)
	{
		entity_free(&entity_manager.entityList[i]);
	}
}

void entity_close()
{
	int i;
	if (!entity_manager.entityList) return;
	for (i = 0; i < entity_manager.maxEnts; i++)
	{
		entity_free(&entity_manager.entityList[i]);
	}
	free(entity_manager.entityList);
	memset(&entity_manager, 0, sizeof(EntityManager));
	slog("Entity system closed");
}