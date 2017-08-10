#include "includes.h"
#include "game.h"

namespace grid
{
	void environmentUpdate()
	{
		{ // destroy outdated sound effects
			statistics.soundEffectsCurrent = 0;
			uint64 time = getApplicationTime() - 2000000;
			uint32 count = transformStruct::component->getComponentEntities()->entitiesCount();
			entityClass *const *effects = transformStruct::component->getComponentEntities()->entitiesArray();
			for (uint32 i = 0; i < count; i++)
			{
				entityClass *e = effects[i];
				if (!e->hasComponent(voiceStruct::component))
					continue;
				ENGINE_GET_COMPONENT(voice, s, e);
				if (s.sound == hashString("grid/player/shield.ogg"))
					continue;
				sourceClass *src = assets()->get<assetSchemeIndexSound, sourceClass>(s.sound);
				if (src && s.soundStart + src->getDuration() + 100000 < time)
					e->addGroup(entitiesToDestroy);
				statistics.soundEffectsCurrent++;
			}
			statistics.soundEffectsMax = max(statistics.soundEffectsMax, statistics.soundEffectsCurrent);
		}

		{ // update effects
			uint32 count = effectStruct::component->getComponentEntities()->entitiesCount();
			entityClass *const *effects = effectStruct::component->getComponentEntities()->entitiesArray();
			for (uint32 i = 0; i < count; i++)
			{
				entityClass *e = effects[i];
				GRID_GET_COMPONENT(effect, g, e);
				if (g.ttl-- == 0)
				{
					e->addGroup(entitiesToDestroy);
					continue;
				}
				ENGINE_GET_COMPONENT(transform, t, e);
				t.position += g.speed;
			}
		}

		{ // camera
			ENGINE_GET_COMPONENT(transform, tr, player.primaryCameraEntity);
			tr.position[0] = player.position[0];
			tr.position[2] = player.position[2];
		}

		{ // update skybox
			ENGINE_GET_COMPONENT(transform, t, player.skyboxRenderEntity);
			t.orientation = skyboxOrientation;
			ENGINE_GET_COMPONENT(animatedTexture, a, player.skyboxRenderEntity);
			real playerHit = player.cinematic || statistics.monstersFirstHit == 0 ? 0 : clamp(real(statistics.updateIterationNoPause - statistics.monstersLastHit) / 5, 0, 1) * 1000000;
			a.animationOffset = playerHit;
		}

		if (player.gameOver)
			return;

		{ // secondaryCamera
			static configBool secondaryCamera("grid.secondary-camera.enabled", false);
			if (secondaryCamera)
			{
				ENGINE_GET_COMPONENT(transform, tp, player.playerEntity);
				{
					ENGINE_GET_COMPONENT(transform, ts, player.skyboxSecondaryCameraEntity);
					ts.orientation = tp.orientation;
					ENGINE_GET_COMPONENT(camera, c, player.skyboxSecondaryCameraEntity);
					c.cameraOrder = 3;
					c.renderMask = 2;
					c.near = 0.5;
					c.far = 3;
					c.ambientLight = vec3(1, 1, 1);
					c.perspectiveFov = degs(60);
					c.viewportOrigin = vec2(0.7, 0);
					c.viewportSize = vec2(0.3, 0.3);
				}
				{
					ENGINE_GET_COMPONENT(transform, tc, player.secondaryCameraEntity);
					tc.position = tp.position;
					tc.orientation = tp.orientation;
					ENGINE_GET_COMPONENT(camera, c, player.secondaryCameraEntity);
					c.cameraOrder = 4;
					c.renderMask = 1;
					c.near = 3;
					c.far = 500;
					c.ambientLight = vec3(1, 1, 1) * 0.8;
					c.clear = (cameraClearFlags)0;
					c.perspectiveFov = degs(60);
					c.viewportOrigin = vec2(0.7, 0);
					c.viewportSize = vec2(0.3, 0.3);
				}
			}
			else
			{
				player.skyboxSecondaryCameraEntity->removeComponent(cameraStruct::component);
				player.secondaryCameraEntity->removeComponent(cameraStruct::component);
			}
		}

		if (player.paused)
			return;

		{ // update grid markers
			uint32 count = gridStruct::component->getComponentEntities()->entitiesCount();
			entityClass *const *grids = gridStruct::component->getComponentEntities()->entitiesArray();
			for (uint32 i = 0; i < count; i++)
			{
				entityClass *e = grids[i];
				ENGINE_GET_COMPONENT(transform, t, e);
				ENGINE_GET_COMPONENT(render, r, e);
				GRID_GET_COMPONENT(grid, g, e);
				g.speed *= 0.95;
				g.speed += (g.place - t.position) * 0.005;
				t.position += g.speed;
				r.color = interpolate(r.color, g.originalColor, 0.002);
			}
		}
	}

	namespace
	{
		struct explosionStruct
		{
			vec3 position;
			vec3 color;
			real size;

			explosionStruct(const vec3 &position, const vec3 &color, real size) :
				position(position), color(color), size(size)
			{
				spatialQuery->sphere(position, size);
				const uint32 *res = spatialQuery->resultArray();
				for (uint32 i = 0, e = spatialQuery->resultCount(); i != e; i++)
					test(res[i]);
			}

			void test(uint32 otherName)
			{
				if (!entities()->hasEntity(otherName))
					return;
				entityClass *e = entities()->getEntity(otherName);
				if (!e->hasComponent(gridStruct::component))
					return;
				ENGINE_GET_COMPONENT(transform, ot, e);
				ENGINE_GET_COMPONENT(render, orc, e);
				GRID_GET_COMPONENT(grid, og, e);
				vec3 toOther = ot.position - position;
				vec3 change = toOther.normalize() * (size / max(2, toOther.squaredLength())) * 2;
				og.speed += change;
				ot.position += change * 2;
				orc.color = interpolate(color, orc.color, toOther.length() / size);
			}
		};
	}

	void environmentExplosion(const vec3 &position, const vec3 &speed, const vec3 &color, real size, real scale)
	{
		statistics.environmentExplosions++;
		explosionStruct explosion(position, color, size);
		uint32 cnt = numeric_cast<uint32>(clamp(size, 2, 20));
		for (uint32 i = 0; i < cnt; i++)
		{
			entityClass *e = entities()->newEntity();
			GRID_GET_COMPONENT(effect, effect, e);
			effect.ttl = numeric_cast<uint32>(random() * 5 + 10);
			effect.speed = randomDirection3();
			if (speed.squaredLength() > 0)
				effect.speed = normalize(effect.speed + speed.normalize() * speed.length().sqrt());
			effect.speed *= random() + 0.5;
			ENGINE_GET_COMPONENT(transform, transform, e);
			transform.scale = scale * (random() * 0.4 + 0.8) * 0.4;
			transform.position = position + vec3(random() * 2 - 1, 0, random() * 2 - 1);
			transform.orientation = randomDirectionQuat();
			ENGINE_GET_COMPONENT(render, render, e);
			render.object = hashString("grid/environment/explosion.object");
			render.color = colorVariation(color);
		}
	}

	void monsterExplosion(entityClass *e)
	{
		ENGINE_GET_COMPONENT(transform, t, e);
		ENGINE_GET_COMPONENT(render, r, e);
		GRID_GET_COMPONENT(monster, m, e);
		environmentExplosion(t.position, m.speed, r.color, min(m.damage, 10) + 2, t.scale);
	}

	void shotExplosion(entityClass *e)
	{
		ENGINE_GET_COMPONENT(transform, t, e);
		ENGINE_GET_COMPONENT(render, r, e);
		GRID_GET_COMPONENT(shot, s, e);
		environmentExplosion(t.position, s.speed, r.color, 5, t.scale * 2);
	}

	void environmentInit()
	{
		{
			entityClass *light = entities()->newEntity(entities()->generateUniqueName());
			ENGINE_GET_COMPONENT(transform, t, light);
			t.orientation = quat(degs(), degs(45), degs());
			ENGINE_GET_COMPONENT(light, l, light);
			l.lightType = lightTypeEnum::Directional;
			l.color = vec3(1, 1, 1) * 2;
		}

		{
			player.skyboxRenderEntity = entities()->newEntity(entities()->generateUniqueName());
			ENGINE_GET_COMPONENT(render, r, player.skyboxRenderEntity);
			r.object = hashString("grid/environment/skybox.object");
			r.renderMask = 2;
			ENGINE_GET_COMPONENT(animatedTexture, a, player.skyboxRenderEntity);
			a.animationSpeed = 0;
		}

		{
			player.skyboxPrimaryCameraEntity = entities()->newEntity(entities()->generateUniqueName());
			ENGINE_GET_COMPONENT(transform, transform, player.skyboxPrimaryCameraEntity);
			transform.orientation = quat(degs(-90), degs(), degs());
			ENGINE_GET_COMPONENT(camera, c, player.skyboxPrimaryCameraEntity);
			c.cameraOrder = 1;
			c.renderMask = 2;
			c.near = 0.5;
			c.far = 3;
			c.ambientLight = vec3(1, 1, 1);
		}

		{
			player.primaryCameraEntity = entities()->newEntity(entities()->generateUniqueName());
			ENGINE_GET_COMPONENT(transform, transform, player.primaryCameraEntity);
			transform.orientation = quat(degs(-90), degs(), degs());
			transform.position = vec3(0, 140, 0);
			ENGINE_GET_COMPONENT(camera, c, player.primaryCameraEntity);
			c.cameraOrder = 2;
			c.renderMask = 1;
			c.near = 50;
			c.far = 200;
			c.ambientLight = vec3(1, 1, 1) * 0.8;
			c.clear = (cameraClearFlags)0;
			ENGINE_GET_COMPONENT(listener, l, player.primaryCameraEntity);
			static const float halfVolumeDistance = 30;
			l.volumeAttenuationByDistance[1] = 2.0 / halfVolumeDistance;
			l.volumeAttenuationByDistance[0] = l.volumeAttenuationByDistance[1] * transform.position[1] * -1;
		}

		{
			player.skyboxSecondaryCameraEntity = entities()->newEntity(entities()->generateUniqueName());
			player.secondaryCameraEntity = entities()->newEntity(entities()->generateUniqueName());
		}

#ifdef CAGE_DEBUG
		return;
#endif

		const real radius = player.mapNoPullRadius * 1.5;
		const real step = 10;
		for (real y = -radius; y < radius + 1e-3; y += step)
		{
			for (real x = -radius; x < radius + 1e-3; x += step)
			{
				const real d = vec3(x, 0, y).length();
				if (d > radius)
					continue;
				entityClass *e = entities()->newEntity(entities()->generateUniqueName());
				GRID_GET_COMPONENT(grid, grid, e);
				grid.place = vec3(x, -2, y) + vec3(random(), random() * 0.1, random()) * 2 - 1;
				grid.speed = randomDirection3();
				grid.speed[1] *= 0.01;
				ENGINE_GET_COMPONENT(transform, transform, e);
				transform.scale = 0.6;
				transform.position = grid.place + randomDirection3() * vec3(10, 0.1, 10);
				ENGINE_GET_COMPONENT(render, render, e);
				render.object = hashString("grid/environment/grid.object");
				real ang = real(aTan2(x, y)) / real::TwoPi + 0.5;
				real dst = d / radius;
				render.color = convertHsvToRgb(vec3(ang, 1, interpolate(real(1), real(0.2), sqr(dst))));
				grid.originalColor = render.color;
			}
		}

		for (rads ang = degs(0); ang < degs(360); ang += degs(1))
		{
			entityClass *e = entities()->newEntity(entities()->generateUniqueName());
			GRID_GET_COMPONENT(grid, grid, e);
			ENGINE_GET_COMPONENT(transform, transform, e);
			transform.position = grid.place = vec3(sin(ang), 0, cos(ang)) * (radius + step * 0.5);
			transform.scale = 0.6;
			ENGINE_GET_COMPONENT(render, render, e);
			render.object = hashString("grid/environment/grid.object");
			grid.originalColor = render.color = vec3(1, 1, 1);
		}

		statistics.environmentGridMarkers = gridStruct::component->getComponentEntities()->entitiesCount();
	}

	const vec3 colorVariation(const vec3 &color)
	{
		vec3 r = color + vec3(random(), random(), random()) * 0.1 - 0.05;
		return min(max(r, vec3(0, 0, 0)), vec3(1, 1, 1));
	}
}