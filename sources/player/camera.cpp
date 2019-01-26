#include "../game.h"

#include <cage-core/config.h>
#include <cage-core/entities.h>
#include <cage-core/hashString.h>

namespace
{
	quat skyboxOrientation;
	quat skyboxRotation;

	entityClass *skyboxRenderEntity;
	entityClass *primaryCameraEntity;
	entityClass *skyboxPrimaryCameraEntity;
	entityClass *secondaryCameraEntity;
	entityClass *skyboxSecondaryCameraEntity;

	const real ambientSkybox = 1.0;
	const real ambientPlayer = 0.25;

	void engineInit()
	{
		skyboxOrientation = randomDirectionQuat();
		skyboxRotation = interpolate(quat(), randomDirectionQuat(), 5e-5);
	}

	void engineUpdate()
	{
		{ // update skybox
			skyboxOrientation = skyboxRotation * skyboxOrientation;
			ENGINE_GET_COMPONENT(transform, t, skyboxRenderEntity);
			t.orientation = skyboxOrientation;
			ENGINE_GET_COMPONENT(animatedTexture, a, skyboxRenderEntity);
			real playerHit = game.cinematic || statistics.monstersFirstHit == 0 ? 0 : clamp(real(statistics.updateIterationIgnorePause - statistics.monstersLastHit) / 5, 0, 1) * 1000000;
			a.offset = playerHit;
		}

		if (game.gameOver)
			return;

		{ // camera
			ENGINE_GET_COMPONENT(transform, tr, primaryCameraEntity);
			ENGINE_GET_COMPONENT(transform, p, game.playerEntity);
			tr.position[0] = p.position[0];
			tr.position[2] = p.position[2];
		}

		{ // secondaryCamera
			static configBool secondaryCamera("grid.secondary-camera.enabled", false);
			if (secondaryCamera)
			{
				ENGINE_GET_COMPONENT(transform, tp, game.playerEntity);
				{
					ENGINE_GET_COMPONENT(transform, ts, skyboxSecondaryCameraEntity);
					ts.orientation = tp.orientation;
					ENGINE_GET_COMPONENT(camera, c, skyboxSecondaryCameraEntity);
					c.cameraOrder = 3;
					c.renderMask = 2;
					c.near = 0.5;
					c.far = 3;
					c.ambientLight = vec3(1, 1, 1) * ambientSkybox;
					c.camera.perspectiveFov = degs(60);
					c.viewportOrigin = vec2(0.7, 0);
					c.viewportSize = vec2(0.3, 0.3);
				}
				{
					ENGINE_GET_COMPONENT(transform, tc, secondaryCameraEntity);
					tc.position = tp.position;
					tc.orientation = tp.orientation;
					ENGINE_GET_COMPONENT(camera, c, secondaryCameraEntity);
					c.cameraOrder = 4;
					c.renderMask = 1;
					c.near = 3;
					c.far = 500;
					c.ambientLight = vec3(1, 1, 1) * ambientPlayer;
					c.clear = (cameraClearFlags)0;
					c.camera.perspectiveFov = degs(60);
					c.viewportOrigin = vec2(0.7, 0);
					c.viewportSize = vec2(0.3, 0.3);
					c.effects = cameraEffectsFlags::CombinedPass;
				}
			}
			else
			{
				skyboxSecondaryCameraEntity->remove(cameraComponent::component);
				secondaryCameraEntity->remove(cameraComponent::component);
			}
		}
	}

	void gameStart()
	{
		{
			skyboxRenderEntity = entities()->createUnique();
			ENGINE_GET_COMPONENT(render, r, skyboxRenderEntity);
			r.object = hashString("grid/environment/skybox.object");
			r.renderMask = 2;
			ENGINE_GET_COMPONENT(animatedTexture, a, skyboxRenderEntity);
			a.speed = 0;
		}

		{
			skyboxPrimaryCameraEntity = entities()->createUnique();
			ENGINE_GET_COMPONENT(transform, transform, skyboxPrimaryCameraEntity);
			transform.orientation = quat(degs(-90), degs(), degs());
			ENGINE_GET_COMPONENT(camera, c, skyboxPrimaryCameraEntity);
			c.cameraOrder = 1;
			c.renderMask = 2;
			c.near = 0.5;
			c.far = 3;
			c.ambientLight = vec3(1, 1, 1) * ambientSkybox;
		}

		{
			primaryCameraEntity = entities()->createUnique();
			ENGINE_GET_COMPONENT(transform, transform, primaryCameraEntity);
			transform.orientation = quat(degs(-90), degs(), degs());
			transform.position = vec3(0, 140, 0);
			ENGINE_GET_COMPONENT(camera, c, primaryCameraEntity);
			c.cameraOrder = 2;
			c.renderMask = 1;
			c.near = 50;
			c.far = 200;
			c.ambientLight = vec3(1, 1, 1) * ambientPlayer;
			c.clear = (cameraClearFlags)0;
			c.effects = cameraEffectsFlags::CombinedPass;
			ENGINE_GET_COMPONENT(listener, l, primaryCameraEntity);
			static const float halfVolumeDistance = 30;
			l.attenuation[1] = 2.0 / halfVolumeDistance;
			l.attenuation[0] = l.attenuation[1] * transform.position[1] * -1;
		}

		{
			skyboxSecondaryCameraEntity = entities()->createUnique();
			secondaryCameraEntity = entities()->createUnique();
		}
	}

	class callbacksClass
	{
		eventListener<void()> engineInitListener;
		eventListener<void()> engineUpdateListener;
		eventListener<void()> gameStartListener;
	public:
		callbacksClass()
		{
			engineInitListener.attach(controlThread().initialize, 50);
			engineInitListener.bind<&engineInit>();
			engineUpdateListener.attach(controlThread().update, 50);
			engineUpdateListener.bind<&engineUpdate>();
			gameStartListener.attach(gameStartEvent(), 50);
			gameStartListener.bind<&gameStart>();
		}
	} callbacksInstance;
}

entityClass *getPrimaryCameraEntity()
{
	return primaryCameraEntity;
}

