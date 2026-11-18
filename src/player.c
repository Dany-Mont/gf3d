#include <math.h>

#include "simple_logger.h"

#include "gfc_input.h"

#include "gf3d_vgraphics.h"

#include "level.h"
#include "character.h"
#include "player.h"

#define PLAYER_FALL_LIMIT_Z				-200.0f
#define PLAYER_HITSTUN_PER_KNOCKBACK	20.0f
#define PLAYER_CAMERA_SPREAD_ZOOM		0.5f
#define PLAYER_CAMERA_PITCH				0.0f
#define PLAYER_THROW_OFFSET_X			1.5f
#define PLAYER_THROW_OFFSET_Z			2.0f

#define PLAYER_FACING_RIGHT_YAW	(GFC_PI / 2)
#define PLAYER_FACING_LEFT_YAW	(-GFC_PI / 2)

typedef struct
{
	Uint8 playerIndex;
	CharacterDef def;

	float cameraFollowDistance;
	GFC_Vector2D cameraRotation;

	float moveX;
	Uint8 jumpQueued;
	Uint8 attackQueued;
	Uint8 throwQueued;
	Uint8 dropHeld;

	int facing;
	Uint8 airJumpsLeft;

	int stocks;
	Uint8 eliminated;

	int attackFrame;
	Uint8 attackHitMask;
	int hitstun;
	int hazardCooldown;
	float iceGrip;
	float percent;

	ItemBuff buff;
	int buffTicks;
	Uint8 holding;
	ItemDef heldItem;
	float baseDrawOffsetZ;

	GFC_Vector3D spawnPoint;
}PlayerEntityData;

static Entity *players[PLAYER_MAX] = {0};

int player_think(Entity* self);
int player_update(Entity* self);
int player_draw(Entity* self);
void player_free(Entity* self);

static void player_default_buff(ItemBuff *buff)
{
	memset(buff,0,sizeof(ItemBuff));
	buff->size = 1;
	buff->damageMult = 1;
	buff->knockbackMult = 1;
	buff->speedMult = 1;
	buff->jumpMult = 1;
}

static Uint8 player_buff_active(PlayerEntityData *data)
{
	return (data->buffTicks > 0) ? 1 : 0;
}

static float player_size(PlayerEntityData *data)
{
	return data->def.scale * (player_buff_active(data) ? data->buff.size : 1.0f);
}

static Uint8 player_invincible(PlayerEntityData *data)
{
	return (player_buff_active(data) && data->buff.invincible) ? 1 : 0;
}

static void player_clear_buff(Entity *self, PlayerEntityData *data)
{
	data->buffTicks = 0;
	player_default_buff(&data->buff);
	self->scale = gfc_vector3d(data->def.scale,data->def.scale,data->def.scale);
	self->drawOffset.z = data->baseDrawOffsetZ * data->def.scale;
}

Entity* player_spawn(Uint8 playerIndex, const char *characterFile, GFC_Vector3D position)
{
	PlayerEntityData* data;
	Entity* self;
	if (playerIndex >= PLAYER_MAX)
	{
		slog("failed to spawn player: index %i is out of range (max %i)",playerIndex,PLAYER_MAX);
		return NULL;
	}
	if (players[playerIndex])
	{
		slog("failed to spawn player: player %i already exists",playerIndex + 1);
		return NULL;
	}
	data = gfc_allocate_array(sizeof(PlayerEntityData), 1);
	if (!data) return NULL;
	if (!character_load(characterFile, &data->def))
	{
		free(data);
		return NULL;
	}
	self = entity_new();
	if (!self)
	{
		free(data);
		return NULL;
	}
	self->model = model_load(data->def.model);
	entity_set_feet_origin(self);
	self->position = position;
	self->think = player_think;
	self->update = player_update;
	self->draw = player_draw;
	self->free = player_free;

	data->playerIndex = playerIndex;
	data->cameraFollowDistance = 50;
	data->cameraRotation.x = 0;
	data->cameraRotation.y = PLAYER_CAMERA_PITCH;
	data->airJumpsLeft = data->def.maxAirJumps;
	data->facing = 1;
	data->stocks = PLAYER_START_STOCKS;
	data->spawnPoint = position;
	data->baseDrawOffsetZ = self->drawOffset.z;
	self->scale = gfc_vector3d(data->def.scale,data->def.scale,data->def.scale);
	self->drawOffset.z = data->baseDrawOffsetZ * data->def.scale;
	player_default_buff(&data->buff);
	self->rotation.z = PLAYER_FACING_RIGHT_YAW;
	self->data = data;

	players[playerIndex] = self;
	slog("spawned player %i as %s",playerIndex + 1,data->def.name);
	return self;
}

void player_free(Entity* self)
{
	PlayerEntityData* data;
	if (!self || !self->data) return;
	data = (PlayerEntityData*)self->data;

	if (players[data->playerIndex] == self)players[data->playerIndex] = NULL;
	free(data);
	self->data = NULL;
}

Entity* player_get(Uint8 playerIndex)
{
	if (playerIndex >= PLAYER_MAX)return NULL;
	return players[playerIndex];
}

void player_set_stocks(Entity *player, int stocks)
{
	PlayerEntityData *data;
	if ((!player)||(!player->data))return;
	data = (PlayerEntityData*)player->data;
	data->stocks = stocks;
	data->eliminated = (stocks <= 0) ? 1 : 0;
}

int player_get_stocks(Uint8 playerIndex)
{
	PlayerEntityData *data;
	if ((playerIndex >= PLAYER_MAX)||(!players[playerIndex])||(!players[playerIndex]->data))return 0;
	data = (PlayerEntityData*)players[playerIndex]->data;
	return data->stocks;
}

float player_get_percent(Uint8 playerIndex)
{
	PlayerEntityData *data;
	if ((playerIndex >= PLAYER_MAX)||(!players[playerIndex])||(!players[playerIndex]->data))return 0;
	data = (PlayerEntityData*)players[playerIndex]->data;
	return data->percent;
}

const char *player_get_name(Uint8 playerIndex)
{
	PlayerEntityData *data;
	if ((playerIndex >= PLAYER_MAX)||(!players[playerIndex])||(!players[playerIndex]->data))return "";
	data = (PlayerEntityData*)players[playerIndex]->data;
	return data->def.name;
}

static Uint8 player_command_down(PlayerEntityData *data, const char *action)
{
	char name[32];
	snprintf(name, sizeof(name), "p%i_%s", data->playerIndex + 1, action);
	return gfc_input_command_down(name);
}

static Uint8 player_command_pressed(PlayerEntityData *data, const char *action)
{
	char name[32];
	snprintf(name, sizeof(name), "p%i_%s", data->playerIndex + 1, action);
	return gfc_input_command_pressed(name);
}

int player_think(Entity* self)
{
	PlayerEntityData* data;
	if (!self || !self->data) return 0;
	data = (PlayerEntityData*)self->data;

	data->moveX = 0;
	if (player_command_down(data,"left"))
	{
		data->moveX -= 1;
	}
	if (player_command_down(data,"right"))
	{
		data->moveX += 1;
	}
	data->dropHeld = player_command_down(data,"down") ? 1 : 0;
	if (player_command_pressed(data,"jump"))
	{
		data->jumpQueued = 1;
	}
	if (player_command_pressed(data,"attack"))
	{
		data->attackQueued = 1;
	}
	if (player_command_pressed(data,"item"))
	{
		data->throwQueued = 1;
	}

	return 1;
}

static GFC_Shape player_world_shape(GFC_Shape shape, int facing, GFC_Vector3D position, float size)
{
	if (size != 1)
	{
		if (shape.type == ST_RECT)
		{
			shape.s.r.x *= size;
			shape.s.r.y *= size;
			shape.s.r.w *= size;
			shape.s.r.h *= size;
		}
		else if (shape.type == ST_CIRCLE)
		{
			shape.s.c.x *= size;
			shape.s.c.y *= size;
			shape.s.c.r *= size;
		}
		else if (shape.type == ST_EDGE)
		{
			shape.s.e.x1 *= size;
			shape.s.e.y1 *= size;
			shape.s.e.x2 *= size;
			shape.s.e.y2 *= size;
		}
	}
	if (facing < 0)
	{

		if (shape.type == ST_RECT)
		{
			shape.s.r.x = -(shape.s.r.x + shape.s.r.w);
		}
		else if (shape.type == ST_CIRCLE)
		{
			shape.s.c.x = -shape.s.c.x;
		}
		else if (shape.type == ST_EDGE)
		{
			shape.s.e.x1 = -shape.s.e.x1;
			shape.s.e.x2 = -shape.s.e.x2;
		}
	}
	gfc_shape_move(&shape, gfc_vector2d(position.x, position.z));
	return shape;
}

GFC_Shape player_get_hurtbox(Entity *player)
{
	PlayerEntityData *data;
	if ((!player)||(!player->data))return gfc_shape_rect(0,0,0,0);
	data = (PlayerEntityData*)player->data;
	if (data->eliminated)return gfc_shape_rect(-99999,-99999,0,0);
	return player_world_shape(data->def.hurtbox, data->facing, player->position, player_size(data));
}

static void player_launch(Entity *target, PlayerEntityData *targetData, float knockback, float dirX, float angleDegrees)
{
	float angle;
	angle = angleDegrees * GFC_PI / 180.0f;
	target->velocity.x = dirX * cos(angle) * knockback;
	target->velocity.z = sin(angle) * knockback;
	target->grounded = 0;
	targetData->hitstun = (int)(knockback * PLAYER_HITSTUN_PER_KNOCKBACK);
	targetData->attackFrame = 0;
}

static void player_apply_hit(Entity *self, PlayerEntityData *data, Entity *other, PlayerEntityData *otherData)
{
	AttackDef *attack;
	float damage, knockback;
	attack = &data->def.attack;

	damage = attack->damage * data->def.strength;
	if (player_buff_active(data))damage *= data->buff.damageMult;
	otherData->percent += damage;

	knockback = (attack->baseKnockback + otherData->percent * attack->knockbackGrowth)
		* (data->def.strength / otherData->def.weight);
	if (player_buff_active(data))knockback *= data->buff.knockbackMult;

	player_launch(other, otherData, knockback, data->facing, attack->knockbackAngle);

	slog("player %i hit player %i: %f percent, knockback %f",
		data->playerIndex + 1, otherData->playerIndex + 1, otherData->percent, knockback);
}

static void player_attack_tick(Entity *self, PlayerEntityData *data)
{
	AttackDef *attack;
	GFC_Shape hitbox, hurtbox;
	Entity *other;
	PlayerEntityData *otherData;
	int i;
	attack = &data->def.attack;

	if ((data->attackFrame > attack->startFrame)&&(data->attackFrame <= attack->startFrame + attack->activeFrames))
	{
		hitbox = player_world_shape(attack->hitbox, data->facing, self->position, player_size(data));
		for (i = 0; i < PLAYER_MAX; i++)
		{
			other = players[i];
			if ((!other)||(other == self))continue;
			if (data->attackHitMask & (1 << i))continue;
			otherData = (PlayerEntityData*)other->data;
			if ((!otherData)||(otherData->eliminated))continue;
			hurtbox = player_get_hurtbox(other);
			if (gfc_shape_overlap(hitbox, hurtbox))
			{
				data->attackHitMask |= (1 << i);
				if (player_invincible(otherData))continue;
				player_apply_hit(self, data, other, otherData);
			}
		}
	}
	data->attackFrame++;
	if (data->attackFrame > attack->totalFrames)data->attackFrame = 0;
}

#define PLAYER_HAZARD_COOLDOWN	40
#define PLAYER_SPIKE_ANGLE		75

static void player_hazard_tick(Entity *self, PlayerEntityData *data)
{
	Level *level;
	LevelHazard *hazard;
	GFC_Shape hurtbox, area;
	float knockback, dirX;
	int i;
	data->iceGrip = 1;
	level = level_get_the();
	if (!level)return;
	hurtbox = player_get_hurtbox(self);

	for (i = 0; i < level->hazardCount; i++)
	{
		hazard = &level->hazards[i];
		if (hazard->type != HAZARD_ICE)continue;
		area = gfc_shape_rect(hazard->box.x, hazard->box.z, hazard->box.w, hazard->box.d);
		if (gfc_shape_overlap(hurtbox, area))data->iceGrip = hazard->power;
	}

	if (data->hazardCooldown > 0)
	{
		data->hazardCooldown--;
		return;
	}
	for (i = 0; i < level->hazardCount; i++)
	{
		hazard = &level->hazards[i];
		if (hazard->type == HAZARD_ICE)continue;
		area = gfc_shape_rect(hazard->box.x, hazard->box.z, hazard->box.w, hazard->box.d);
		if (!gfc_shape_overlap(hurtbox, area))continue;
		if (hazard->type == HAZARD_JUMP_PAD)
		{
			self->velocity.z = hazard->power;
			self->grounded = 0;
			data->airJumpsLeft = data->def.maxAirJumps;
			data->hazardCooldown = 10;
			return;
		}

		if ((hazard->type == HAZARD_GEYSER)&&(level_hazard_state(hazard) != 2))continue;
		if (player_invincible(data))continue;
		data->percent += hazard->damage;
		knockback = (hazard->power + data->percent * 0.01f) / data->def.weight;
		dirX = (self->position.x < hazard->box.x + hazard->box.w / 2) ? -1 : 1;
		player_launch(self, data, knockback, dirX, PLAYER_SPIKE_ANGLE);
		data->hazardCooldown = PLAYER_HAZARD_COOLDOWN;
		return;
	}
}

int player_apply_item(Entity *player, const ItemDef *def)
{
	PlayerEntityData *data;
	if ((!player)||(!player->data)||(!def))return 0;
	data = (PlayerEntityData*)player->data;
	switch (def->kind)
	{
		case ITEM_BUFF:

			data->buff = def->buff;
			data->buffTicks = (int)(def->buff.duration * ITEM_TICKS_PER_SECOND);
			if (data->buffTicks < 1)data->buffTicks = 1;
			player->scale = gfc_vector3d(data->def.scale * def->buff.size,data->def.scale * def->buff.size,data->def.scale * def->buff.size);
			player->drawOffset.z = data->baseDrawOffsetZ * data->def.scale * def->buff.size;
			return 1;
		case ITEM_HEAL:
			if (data->percent <= 0)return 0;
			data->percent -= def->healPercent;
			if (data->percent < 0)data->percent = 0;
			slog("player %i healed to %f percent",data->playerIndex + 1,data->percent);
			return 1;
		case ITEM_BOMB:
			if (data->holding)return 0;
			data->heldItem = *def;
			data->holding = 1;
			return 1;
	}
	return 0;
}

void player_blast(GFC_Vector3D center, float radius, float damage, float knockback, float growth, float angle)
{
	GFC_Shape blast, hurtbox;
	Entity *player;
	PlayerEntityData *data;
	float dirX, launch;
	int i;
	blast = gfc_shape_circle(center.x, center.z, radius);
	for (i = 0; i < PLAYER_MAX; i++)
	{
		player = players[i];
		if ((!player)||(!player->data))continue;
		data = (PlayerEntityData*)player->data;
		if ((data->eliminated)||(player_invincible(data)))continue;
		hurtbox = player_get_hurtbox(player);
		if (!gfc_shape_overlap(blast, hurtbox))continue;

		data->percent += damage;
		launch = (knockback + data->percent * growth) / data->def.weight;
		dirX = (player->position.x >= center.x) ? 1 : -1;
		player_launch(player, data, launch, dirX, angle);
		slog("explosion hit player %i: %f percent, knockback %f",i + 1,data->percent,launch);
	}
}

static void player_update_camera(PlayerEntityData *data)
{
	GFC_Matrix4* view;
	GFC_Vector3D offset = {0, -1, 0};
	GFC_Vector3D total = {0};
	GFC_Vector3D target;
	float minX = 0, maxX = 0, distance;
	int i, count = 0;

	for (i = 0; i < PLAYER_MAX; i++)
	{
		if ((!players[i])||(!players[i]->data))continue;
		if (((PlayerEntityData*)players[i]->data)->eliminated)continue;
		if (!count)
		{
			minX = players[i]->position.x;
			maxX = players[i]->position.x;
		}
		if (players[i]->position.x < minX)minX = players[i]->position.x;
		if (players[i]->position.x > maxX)maxX = players[i]->position.x;
		total.x += players[i]->position.x;
		total.y += players[i]->position.y;
		total.z += players[i]->position.z;
		count++;
	}
	if (!count)return;
	target = gfc_vector3d(total.x / count, total.y / count, (total.z + 20.0) / count);
	distance = data->cameraFollowDistance + (maxX - minX) * PLAYER_CAMERA_SPREAD_ZOOM;

	gfc_vector3d_scale(offset, offset, distance);
	view = gf3d_vgraphics_get_view_matrix();
	gfc_vector3d_rotate_about_z(&offset, data->cameraRotation.x);
	gfc_vector3d_rotate_about_x(&offset, data->cameraRotation.y);
	gfc_vector3d_add(offset, offset, target);
	gfc_matrix4_view(
		*view,
		offset,
		target,
		gfc_vector3d(0, 0, 1)
	);
}

static void player_lose_stock(Entity *self, PlayerEntityData *data)
{
	data->stocks--;
	slog("player %i lost a life, %i left",data->playerIndex + 1,data->stocks);
	data->percent = 0;
	data->hitstun = 0;
	data->attackFrame = 0;
	data->holding = 0;
	data->jumpQueued = 0;
	data->attackQueued = 0;
	data->throwQueued = 0;
	data->airJumpsLeft = data->def.maxAirJumps;
	player_clear_buff(self, data);
	self->velocity = gfc_vector3d(0, 0, 0);
	if (data->stocks <= 0)
	{
		data->stocks = 0;
		data->eliminated = 1;
		return;
	}
	self->position = data->spawnPoint;
}

int player_draw(Entity* self)
{
	PlayerEntityData* data;
	if (!self || !self->data) return 0;
	data = (PlayerEntityData*)self->data;
	return data->eliminated ? 1 : 0;
}

int player_update(Entity* self)
{
	PlayerEntityData* data;
	CharacterDef *def;
	float accel, friction, runSpeed, jumpSpeed, airJumpSpeed;
	float speedMult = 1, jumpMult = 1, size;
	float carryX, carryZ;
	float groundZ = 0;
	float moveX;
	Uint8 jump, attack, throwItem;
	if (!self || !self->data) return 0;
	data = (PlayerEntityData*)self->data;
	def = &data->def;

	if (data->eliminated)
	{
		self->velocity = gfc_vector3d(0, 0, 0);
		if (data->playerIndex == 0)
		{
			player_update_camera(data);
		}
		return 1;
	}

	if (self->grounded)
	{
		if (level_platform_carry(level_get_the(), self->position.x, self->position.y, self->position.z, &carryX, &carryZ))
		{
			self->position.x += carryX;
			self->position.z += carryZ;
		}
	}

	if ((self->velocity.z <= 0) &&
		level_ground_test(level_get_the(), self->position.x, self->position.y,
			self->position.z, self->position.z - self->velocity.z, data->dropHeld, &groundZ))
	{
		self->position.z = groundZ;
		self->velocity.z = 0;
		self->grounded = 1;
		data->airJumpsLeft = def->maxAirJumps;
	}
	else
	{
		self->grounded = 0;
	}

	if ((self->position.z < PLAYER_FALL_LIMIT_Z)||
		(!level_in_bounds(level_get_the(), self->position.x, self->position.z)))
	{
		player_lose_stock(self, data);
		if (data->eliminated)
		{
			return 1;
		}
		groundZ = 0;
	}

	player_hazard_tick(self, data);

	if (data->buffTicks > 0)
	{
		data->buffTicks--;
		if (data->buffTicks == 0)
		{
			player_clear_buff(self, data);
		}
	}
	if (player_buff_active(data))
	{
		speedMult = data->buff.speedMult;

		jumpMult = sqrt(data->buff.jumpMult);
	}
	size = player_size(data);

	moveX = data->moveX;
	jump = data->jumpQueued;
	attack = data->attackQueued;
	throwItem = data->throwQueued;
	data->jumpQueued = 0;
	data->attackQueued = 0;
	data->throwQueued = 0;
	if (data->hitstun > 0)
	{

		data->hitstun--;
		moveX = 0;
		jump = 0;
		attack = 0;
		throwItem = 0;
	}

	runSpeed = def->runSpeed * speedMult;
	accel = (self->grounded ? def->groundAccel : def->airAccel) * speedMult;
	friction = (self->grounded ? def->groundFriction : def->airFriction) * speedMult;
	if (self->grounded)
	{

		friction *= data->iceGrip;
		accel *= (data->iceGrip * 3 > 1) ? 1 : data->iceGrip * 3;
	}
	if (moveX)
	{
		self->velocity.x += moveX * accel;
		if (self->velocity.x > runSpeed) self->velocity.x = runSpeed;
		if (self->velocity.x < -runSpeed) self->velocity.x = -runSpeed;
		data->facing = (moveX > 0) ? 1 : -1;
		self->rotation.z = (data->facing > 0) ? PLAYER_FACING_RIGHT_YAW : PLAYER_FACING_LEFT_YAW;
	}
	else
	{
		if (self->velocity.x > friction) self->velocity.x -= friction;
		else if (self->velocity.x < -friction) self->velocity.x += friction;
		else self->velocity.x = 0;
	}

	if ((attack)&&(data->attackFrame == 0))
	{
		data->attackFrame = 1;
		data->attackHitMask = 0;
	}
	if (data->attackFrame > 0)
	{
		player_attack_tick(self, data);
	}

	if ((throwItem)&&(data->holding))
	{
		item_spawn_thrown(&data->heldItem,
			gfc_vector3d(self->position.x + data->facing * PLAYER_THROW_OFFSET_X * size,
						 self->position.y,
						 self->position.z + PLAYER_THROW_OFFSET_Z * size),
			data->facing, data->playerIndex);
		data->holding = 0;
	}

	jumpSpeed = def->jumpSpeed * jumpMult;
	airJumpSpeed = def->airJumpSpeed * jumpMult;
	if (jump)
	{
		if (self->grounded)
		{
			self->velocity.z = jumpSpeed;
			self->grounded = 0;
		}
		else if (data->airJumpsLeft > 0)
		{
			self->velocity.z = airJumpSpeed;
			data->airJumpsLeft--;
		}
	}

	self->velocity.z -= def->gravity;
	if (self->velocity.z < -def->maxFallSpeed) self->velocity.z = -def->maxFallSpeed;

	self->velocity.y = 0;

	if (data->playerIndex == 0)
	{
		player_update_camera(data);
	}

	return 1;
}

#define DEBUG_BOX_DEPTH 2.0f

static void player_draw_shape(GFC_Shape shape, float y, GFC_Color color)
{
	Level *level;
	level = level_get_the();
	if (shape.type == ST_RECT)
	{
		level_draw_box(level, shape.s.r.x, y - DEBUG_BOX_DEPTH / 2, shape.s.r.y, shape.s.r.w, DEBUG_BOX_DEPTH, shape.s.r.h, color);
	}
	else if (shape.type == ST_CIRCLE)
	{

		level_draw_box(level, shape.s.c.x - shape.s.c.r, y - DEBUG_BOX_DEPTH / 2, shape.s.c.y - shape.s.c.r,
			shape.s.c.r * 2, DEBUG_BOX_DEPTH, shape.s.c.r * 2, color);
	}
}

void player_draw_hitboxes()
{
	PlayerEntityData *data;
	AttackDef *attack;
	Entity *player;
	GFC_Shape shape;
	int i;
	if (!level_debug_enabled())return;
	for (i = 0; i < PLAYER_MAX; i++)
	{
		player = players[i];
		if ((!player)||(!player->data))continue;
		data = (PlayerEntityData*)player->data;
		if (data->eliminated)continue;

		player_draw_shape(player_get_hurtbox(player), player->position.y, gfc_color(0.2f, 0.4f, 1.0f, 0.4f));

		if (data->attackFrame > 0)
		{
			attack = &data->def.attack;
			shape = player_world_shape(attack->hitbox, data->facing, player->position, player_size(data));
			if ((data->attackFrame > attack->startFrame)&&(data->attackFrame <= attack->startFrame + attack->activeFrames))
			{
				player_draw_shape(shape, player->position.y, gfc_color(1.0f, 0.1f, 0.1f, 0.5f));
			}
			else
			{
				player_draw_shape(shape, player->position.y, gfc_color(1.0f, 0.9f, 0.1f, 0.25f));
			}
		}
	}
}