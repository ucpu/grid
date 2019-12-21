#include <algorithm>
#include <vector>

#include "monsters.h"

#include <cage-core/color.h>

namespace
{
	vec3 playerPosition;

	vec3 aroundPosition(real index, real radius, vec3 center)
	{
		rads angle = index * rads::Full();
		vec3 dir = vec3(cos(angle), 0, sin(angle));
		return center + dir * radius;
	}

	enum class placingPolicyEnum
	{
		Random,
		Around,
		Grouped,
		Line,
	};

	struct spawnDefinitionStruct
	{
		// spawned monsters
		uint32 spawnCountMin, spawnCountMax;
		real distanceMin, distanceMax;
		monsterTypeFlags spawnTypes;
		placingPolicyEnum placingPolicy;

		// priority
		real priorityCurrent; // lowest priority goes first
		real priorityChange;
		real priorityAdditive;
		real priorityMultiplier;

		// statistics
		uint32 iteration;
		uint32 spawned;
		string name;

		spawnDefinitionStruct(const string &name);
		bool operator < (const spawnDefinitionStruct &other) const { return priorityCurrent < other.priorityCurrent; }
		void perform();
		void performSimulation();
		void updatePriority();
		void spawn();
	};

	spawnDefinitionStruct::spawnDefinitionStruct(const string &name) :
		spawnTypes((monsterTypeFlags)0),
		spawnCountMin(1), spawnCountMax(1),
		distanceMin(200), distanceMax(250),
		placingPolicy(placingPolicyEnum::Random),
		priorityCurrent(0), priorityChange(0), priorityAdditive(0), priorityMultiplier(1),
		iteration(0), spawned(0), name(name)
	{}

	uint32 monstersLimit()
	{
		if (game.cinematic)
			return 100;
		return 150 + 20 * game.defeatedBosses;
	}

	std::vector<spawnDefinitionStruct> definitions;

	void spawnDefinitionStruct::perform()
	{
		spawn();
		updatePriority();
		statistics.monstersCurrentSpawningPriority = priorityCurrent;
	}

	void spawnDefinitionStruct::updatePriority()
	{
		iteration++;
		priorityCurrent += max(priorityChange, 1e-5);
		priorityChange += priorityAdditive;
		priorityChange *= priorityMultiplier;
	}

	void spawnDefinitionStruct::performSimulation()
	{
		updatePriority();
		if (any(spawnTypes & monsterTypeFlags::BossEgg))
			game.defeatedBosses++;
	}

	void spawnDefinitionStruct::spawn()
	{
		{ // update player position
			CAGE_COMPONENT_ENGINE(transform, p, game.playerEntity);
			playerPosition = p.position;
		}

		CAGE_ASSERT(spawnCountMin <= spawnCountMax && spawnCountMin > 0, spawnCountMin, spawnCountMax);
		CAGE_ASSERT(distanceMin <= distanceMax && distanceMin > 0, distanceMin, distanceMax);
		std::vector<monsterTypeFlags> allowed;
		allowed.reserve(32);
		{
			uint32 bit = 1;
			for (uint32 i = 0; i < 32; i++)
			{
				if ((spawnTypes & (monsterTypeFlags)bit) == (monsterTypeFlags)bit)
					allowed.push_back((monsterTypeFlags)bit);
				bit <<= 1;
			}
		}
		uint32 alSiz = numeric_cast<uint32>(allowed.size());
		CAGE_ASSERT(alSiz > 0);
		uint32 spawnCount = randomRange(spawnCountMin, spawnCountMax + 1);
		spawned += spawnCount;
		vec3 color = colorHsvToRgb(vec3(randomChance(), sqrt(randomChance()) * 0.5 + 0.5, sqrt(randomChance()) * 0.5 + 0.5));
		switch (placingPolicy)
		{
		case placingPolicyEnum::Random:
		{
			for (uint32 i = 0; i < spawnCount; i++)
				spawnGeneral(allowed[randomRange(0u, alSiz)], aroundPosition(randomChance(), randomRange(distanceMin, distanceMax), playerPosition), color);
		} break;
		case placingPolicyEnum::Around:
		{
			real angularOffset = randomChance();
			real radius = randomRange(distanceMin, distanceMax);
			for (uint32 i = 0; i < spawnCount; i++)
				spawnGeneral(allowed[randomRange(0u, alSiz)], aroundPosition(angularOffset + (randomChance() * 0.3 + i) / (real)spawnCount, radius, playerPosition), color);
		} break;
		case placingPolicyEnum::Grouped:
		{
			real radius = (distanceMax - distanceMin) * 0.5;
			vec3 center = aroundPosition(randomChance(), distanceMin + radius, playerPosition);
			for (uint32 i = 0; i < spawnCount; i++)
				spawnGeneral(allowed[randomRange(0u, alSiz)], aroundPosition((real)i / (real)spawnCount, radius, center), color);
		} break;
		case placingPolicyEnum::Line:
		{
			vec3 origin = aroundPosition(randomChance(), randomRange(distanceMin, distanceMax), playerPosition);
			vec3 span = cross(normalize(origin - playerPosition), vec3(0, 1, 0)) * 10;
			origin -= span * spawnCount * 0.5;
			for (uint32 i = 0; i < spawnCount; i++)
				spawnGeneral(allowed[randomRange(0u, alSiz)], origin + i * span, color);
		} break;
		default: CAGE_THROW_CRITICAL(exception, "invalid placing policy");
		}
	}

	void engineUpdate()
	{
		OPTICK_EVENT("spawning");

		if (game.paused)
			return;

		if (bossComponent::component->group()->count() > 0)
			return;

		uint32 limit = monstersLimit();
		real probability = 1.f - (real)statistics.monstersCurrent / (real)limit;
		if (probability <= 0)
			return;

		probability = pow(100, probability - 1);
		if (randomChance() > probability)
			return;

		definitions[0].perform();
		std::nth_element(definitions.begin(), definitions.begin(), definitions.end());
	}

	void announceJokeMap()
	{
		game.jokeMap = true;
		makeAnnouncement(hashString("announcement/joke-map"), hashString("announcement-desc/joke-map"), 120 * 30);
	}

	void gameStart()
	{
		definitions.clear();
		definitions.reserve(30);

		if (game.cinematic)
		{ // cinematic spawns
			spawnDefinitionStruct d("cinematic");
			d.spawnTypes = (monsterTypeFlags::Circle | monsterTypeFlags::SmallTriangle | monsterTypeFlags::SmallCube | monsterTypeFlags::LargeTriangle | monsterTypeFlags::LargeCube);
			definitions.push_back(d);
			return;
		}

		///////////////////////////////////////////////////////////////////////////
		// jokes
		///////////////////////////////////////////////////////////////////////////

		if (randomRange(0u, 1000u) == 42)
		{
			CAGE_LOG(severityEnum::Info, "joke", "JOKE: snakes only map");
			spawnDefinitionStruct d("snakes only");
			d.spawnTypes = (monsterTypeFlags::Snake);
			definitions.push_back(d);
			announceJokeMap();
			return;
		}

		// todo monkeys joke

		///////////////////////////////////////////////////////////////////////////
		// individually spawned monsters
		///////////////////////////////////////////////////////////////////////////

		monstersSpawnInitial();

		{ // small monsters individually
			spawnDefinitionStruct d("individual small monsters");
			d.spawnTypes = (monsterTypeFlags::Circle | monsterTypeFlags::SmallTriangle | monsterTypeFlags::SmallCube);
			d.priorityCurrent = 0;
			d.priorityChange = 1;
			d.priorityAdditive = 0.001;
			d.priorityMultiplier = 1.02;
			definitions.push_back(d);
		}

		{ // large monsters individually
			spawnDefinitionStruct d("individual large monsters");
			d.spawnTypes = (monsterTypeFlags::LargeTriangle | monsterTypeFlags::LargeCube);
			d.priorityCurrent = 50;
			d.priorityChange = 1;
			d.priorityAdditive = 0.001;
			d.priorityMultiplier = 1.02;
			definitions.push_back(d);
		}

		{ // pin wheels individually
			spawnDefinitionStruct d("individual pinwheels");
			d.spawnTypes = (monsterTypeFlags::PinWheel);
			d.priorityCurrent = randomRange(200, 400);
			d.priorityChange = 27;
			d.priorityAdditive = 0.01;
			d.priorityMultiplier = 1.02;
			definitions.push_back(d);
		}

		{ // diamonds individually
			spawnDefinitionStruct d("individual diamonds");
			d.spawnTypes = (monsterTypeFlags::Diamond);
			d.priorityCurrent = randomRange(300, 500);
			d.priorityChange = 42;
			d.priorityAdditive = 0.1;
			d.priorityMultiplier = 1.02;
			definitions.push_back(d);
		}

		{ // snakes individually
			spawnDefinitionStruct d("individual snakes");
			d.spawnTypes = (monsterTypeFlags::Snake);
			d.priorityCurrent = randomRange(1000, 2500);
			d.priorityChange = 133;
			d.priorityAdditive = 1;
			d.priorityMultiplier = 1.02;
			definitions.push_back(d);
		}

		{ // shielders individually
			spawnDefinitionStruct d("individual shielders");
			d.spawnTypes = (monsterTypeFlags::Shielder);
			d.priorityCurrent = randomRange(1000, 2500);
			d.priorityChange = 127;
			d.priorityAdditive = 1;
			d.priorityMultiplier = 1.02;
			definitions.push_back(d);
		}

		{ // shockers individually
			spawnDefinitionStruct d("individual shockers");
			d.spawnTypes = (monsterTypeFlags::Shocker);
			d.priorityCurrent = randomRange(2500, 3500);
			d.priorityChange = 143;
			d.priorityAdditive = 1;
			d.priorityMultiplier = 1.02;
			definitions.push_back(d);
		}

		{ // rockets individually
			spawnDefinitionStruct d("individual rockets");
			d.spawnTypes = (monsterTypeFlags::Rocket);
			d.priorityCurrent = randomRange(2500, 3500);
			d.priorityChange = 111;
			d.priorityAdditive = 1;
			d.priorityMultiplier = 1.02;
			definitions.push_back(d);
		}

		{ // spawners individually
			spawnDefinitionStruct d("individual spawners");
			d.spawnTypes = (monsterTypeFlags::Spawner);
			d.priorityCurrent = randomRange(2500, 3500);
			d.priorityChange = 122;
			d.priorityAdditive = 1;
			d.priorityMultiplier = 1.02;
			definitions.push_back(d);
		}

		///////////////////////////////////////////////////////////////////////////
		// groups
		///////////////////////////////////////////////////////////////////////////

		{ // circle groups
			spawnDefinitionStruct d("circle groups");
			d.spawnCountMin = 10;
			d.spawnCountMax = 20;
			d.spawnTypes = (monsterTypeFlags::Circle);
			d.placingPolicy = placingPolicyEnum::Grouped;
			d.priorityCurrent = randomRange(400, 600);
			d.priorityChange = randomRange(200, 300);
			d.priorityAdditive = randomRange(1, 10);
			definitions.push_back(d);
		}

		{ // large triangle groups
			spawnDefinitionStruct d("large triangle groups");
			d.spawnCountMin = 5;
			d.spawnCountMax = 15;
			d.spawnTypes = (monsterTypeFlags::LargeTriangle);
			d.placingPolicy = placingPolicyEnum::Grouped;
			d.priorityCurrent = randomRange(1000, 1500);
			d.priorityChange = randomRange(200, 300);
			d.priorityAdditive = randomRange(1, 10);
			definitions.push_back(d);
		}

		{ // large cube groups
			spawnDefinitionStruct d("large cube groups");
			d.spawnCountMin = 5;
			d.spawnCountMax = 15;
			d.spawnTypes = (monsterTypeFlags::LargeCube);
			d.placingPolicy = placingPolicyEnum::Grouped;
			d.priorityCurrent = randomRange(1000, 1500);
			d.priorityChange = randomRange(200, 300);
			d.priorityAdditive = randomRange(1, 10);
			definitions.push_back(d);
		}

		{ // pin wheel groups
			spawnDefinitionStruct d("pinwheel groups");
			d.spawnCountMin = 2;
			d.spawnCountMax = 4;
			d.spawnTypes = (monsterTypeFlags::PinWheel);
			d.placingPolicy = placingPolicyEnum::Grouped;
			d.priorityCurrent = randomRange(2000, 3000);
			d.priorityChange = randomRange(200, 300);
			d.priorityAdditive = randomRange(5, 15);
			definitions.push_back(d);
		}

		{ // diamond groups
			spawnDefinitionStruct d("diamond groups");
			d.spawnCountMin = 3;
			d.spawnCountMax = 7;
			d.spawnTypes = (monsterTypeFlags::Diamond);
			d.placingPolicy = placingPolicyEnum::Grouped;
			d.priorityCurrent = randomRange(2000, 3000);
			d.priorityChange = randomRange(200, 300);
			d.priorityAdditive = randomRange(5, 15);
			definitions.push_back(d);
		}

		///////////////////////////////////////////////////////////////////////////
		// formations (after first boss)
		///////////////////////////////////////////////////////////////////////////

		{ // mixed group
			spawnDefinitionStruct d("mixed group");
			d.spawnCountMin = 20;
			d.spawnCountMax = 25;
			d.spawnTypes = (monsterTypeFlags::LargeTriangle | monsterTypeFlags::LargeCube | monsterTypeFlags::Diamond);
			d.placingPolicy = placingPolicyEnum::Grouped;
			d.priorityCurrent = randomRange(4000, 6000);
			d.priorityChange = randomRange(200, 300);
			d.priorityAdditive = randomRange(5, 15);
			definitions.push_back(d);
		}

		{ // mixed around
			spawnDefinitionStruct d("mixed around");
			d.spawnCountMin = 5;
			d.spawnCountMax = 10;
			d.spawnTypes = (monsterTypeFlags::PinWheel | monsterTypeFlags::Snake | monsterTypeFlags::Shielder | monsterTypeFlags::Shocker | monsterTypeFlags::Rocket);
			d.placingPolicy = placingPolicyEnum::Around;
			d.distanceMin = 160;
			d.distanceMax = 190;
			d.priorityCurrent = randomRange(5000, 7000);
			d.priorityChange = randomRange(200, 300);
			d.priorityAdditive = randomRange(5, 15);
			definitions.push_back(d);
		}

		{ // rockets wall
			spawnDefinitionStruct d("rockets wall");
			d.spawnCountMin = 10;
			d.spawnCountMax = 15;
			d.spawnTypes = (monsterTypeFlags::Rocket);
			d.placingPolicy = placingPolicyEnum::Line;
			d.priorityCurrent = randomRange(5000, 7000);
			d.priorityChange = randomRange(800, 1200);
			d.priorityAdditive = randomRange(20, 60);
			definitions.push_back(d);
		}

		{ // shielders wall
			spawnDefinitionStruct d("shielders wall");
			d.spawnCountMin = 10;
			d.spawnCountMax = 15;
			d.spawnTypes = (monsterTypeFlags::Shielder);
			d.placingPolicy = placingPolicyEnum::Line;
			d.priorityCurrent = randomRange(6000, 8000);
			d.priorityChange = randomRange(800, 1200);
			d.priorityAdditive = randomRange(20, 60);
			definitions.push_back(d);
		}

		{ // rockets circle
			spawnDefinitionStruct d("rockets circle");
			d.spawnCountMin = 30;
			d.spawnCountMax = 50;
			d.spawnTypes = (monsterTypeFlags::Rocket);
			d.placingPolicy = placingPolicyEnum::Around;
			d.priorityCurrent = randomRange(7000, 9000);
			d.priorityChange = randomRange(800, 1200);
			d.priorityAdditive = randomRange(20, 60);
			definitions.push_back(d);
		}

		{ // saturated simple
			spawnDefinitionStruct d("saturated circles");
			d.spawnCountMin = 100;
			d.spawnCountMax = 200;
			d.spawnTypes = (monsterTypeFlags::Circle | monsterTypeFlags::SmallCube | monsterTypeFlags::SmallTriangle);
			d.placingPolicy = placingPolicyEnum::Random;
			d.distanceMin = 120;
			d.distanceMax = 180;
			d.priorityCurrent = randomRange(7000, 9000);
			d.priorityChange = randomRange(1000, 1500);
			d.priorityAdditive = randomRange(25, 75);
			definitions.push_back(d);
		}

		{ // wormholes
			spawnDefinitionStruct d("wormholes");
			d.spawnCountMin = 1;
			d.spawnCountMax = 3;
			d.spawnTypes = (monsterTypeFlags::Wormhole);
			d.placingPolicy = placingPolicyEnum::Around;
			d.distanceMin = 160;
			d.distanceMax = 190;
			d.priorityCurrent = randomRange(5000, 7000);
			d.priorityChange = randomRange(4000, 6000);
			d.priorityAdditive = randomRange(100, 300);
			definitions.push_back(d);
		}

		{ // spawners
			spawnDefinitionStruct d("spawners");
			d.spawnCountMin = 2;
			d.spawnCountMax = 4;
			d.spawnTypes = (monsterTypeFlags::Spawner);
			d.placingPolicy = placingPolicyEnum::Around;
			d.distanceMin = 160;
			d.distanceMax = 190;
			d.priorityCurrent = randomRange(6000, 8000);
			d.priorityChange = randomRange(1000, 1500);
			d.priorityAdditive = randomRange(25, 75);
			definitions.push_back(d);
		}

		///////////////////////////////////////////////////////////////////////////
		// bosses
		///////////////////////////////////////////////////////////////////////////

		{ // bosses
			spawnDefinitionStruct d("bosses");
			d.spawnCountMin = 1;
			d.spawnCountMax = 1;
			d.spawnTypes = (monsterTypeFlags::BossEgg);
			d.placingPolicy = placingPolicyEnum::Around;
			d.distanceMin = 300;
			d.distanceMax = 350;
#if 0
			d.priorityChange = 1;
#else
			d.priorityCurrent = 4000;
			d.priorityChange = 10000;
			d.priorityMultiplier = 1.5;
#endif
			definitions.push_back(d);
		}

		///////////////////////////////////////////////////////////////////////////
		// testing
		///////////////////////////////////////////////////////////////////////////

#if 0
		std::nth_element(definitions.begin(), definitions.begin(), definitions.end());
		while (definitions[0].priorityCurrent < 50000)
		{
			definitions[0].performSimulation();
			std::nth_element(definitions.begin(), definitions.begin(), definitions.end());
		}
#endif
	}

	void gameStop()
	{
#ifdef DEGRID_TESTING
		for (auto &d : definitions)
		{
			if (d.iteration > 0)
				CAGE_LOG(severityEnum::Info, "statistics", stringizer() + "spawn '" + d.name + "', iteration: " + d.iteration + ", spawned: " + d.spawned + ", priority: " + d.priorityCurrent + ", change: " + d.priorityChange);
		}
#endif // DEGRID_TESTING

		definitions.clear();
	}

	class callbacksClass
	{
		eventListener<void()> engineUpdateListener;
		eventListener<void()> gameStartListener;
		eventListener<void()> gameStopListener;
	public:
		callbacksClass() : engineUpdateListener("spawning"), gameStartListener("spawning"), gameStopListener("spawning")
		{
			engineUpdateListener.attach(controlThread().update);
			engineUpdateListener.bind<&engineUpdate>();
			gameStartListener.attach(gameStartEvent());
			gameStartListener.bind<&gameStart>();
			gameStopListener.attach(gameStopEvent());
			gameStopListener.bind<&gameStop>();
		}
	} callbacksInstance;
}

void spawnGeneral(monsterTypeFlags type, const vec3 &spawnPosition, const vec3 &color)
{
	switch (type)
	{
	case monsterTypeFlags::Snake: return spawnSnake(spawnPosition, color);
	case monsterTypeFlags::Shielder: return spawnShielder(spawnPosition, color);
	case monsterTypeFlags::Shocker: return spawnShocker(spawnPosition, color);
	case monsterTypeFlags::Wormhole: return spawnWormhole(spawnPosition, color);
	case monsterTypeFlags::Rocket: return spawnRocket(spawnPosition, color);
	case monsterTypeFlags::Spawner: return spawnSpawner(spawnPosition, color);
	case monsterTypeFlags::BossEgg: return spawnBossEgg(spawnPosition, color);
	default: return spawnSimple(type, spawnPosition, color);
	}
}

void monstersSpawnInitial()
{
	{
		spawnDefinitionStruct d("initial 1");
		d.spawnTypes = monsterTypeFlags::Circle;
		d.spawnCountMin = monstersLimit();
		d.spawnCountMax = d.spawnCountMin + 10;
		d.spawn();
	}
	{
		spawnDefinitionStruct d("initial 2");
		d.spawnTypes = monsterTypeFlags::Circle;
		d.placingPolicy = placingPolicyEnum::Grouped;
		d.distanceMin = 80;
		d.distanceMax = 100;
		d.spawnCountMin = 1;
		d.spawnCountMax = 3;
		d.spawn();
	}
}
