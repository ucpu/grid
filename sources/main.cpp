#include <cage-core/config.h>
#include <cage-core/assetManager.h>
#include <cage-engine/engineProfiling.h>
#include <cage-engine/fullscreenSwitcher.h>
#include <cage-engine/highPerformanceGpuHint.h>

#include "game.h"
#include "screens/screens.h"

ConfigUint32 confLanguage("degrid/language/language", 0);

void reloadLanguage(uint32 index);

namespace
{
	uint32 loadedLanguageHash;
	uint32 currentLanguageHash;

	bool windowClose()
	{
		engineStop();
		return true;
	}

	bool keyRelease(uint32 key, uint32, ModifiersFlags modifiers)
	{
		if (modifiers != ModifiersFlags::None)
			return false;

		static ConfigBool secondaryCamera("degrid/secondaryCamera/enabled", false);

		CAGE_LOG_DEBUG(SeverityEnum::Info, "keyboard", stringizer() + "key: " + key);

		switch (key)
		{
		case 298: // F9
			secondaryCamera = !(bool)secondaryCamera;
			return true;
		}

		return false;
	}

	void assetsUpdate()
	{
		if (currentLanguageHash != loadedLanguageHash)
		{
			if (loadedLanguageHash)
				engineAssets()->remove(loadedLanguageHash);
			loadedLanguageHash = currentLanguageHash;
			if (loadedLanguageHash)
				engineAssets()->add(loadedLanguageHash);
		}
	}

	void frameCounter()
	{
		statistics.frameIteration++;
	}

	WindowEventListeners listeners;
}

void reloadLanguage(uint32 index)
{
	constexpr const uint32 Languages[] = {
		HashString("degrid/languages/english.textpack"),
		HashString("degrid/languages/czech.textpack")
	};
	if (index < sizeof(Languages) / sizeof(Languages[0]))
		currentLanguageHash = Languages[index];
	else
		currentLanguageHash = 0;
}

int main(int argc, const char *args[])
{
	try
	{
		configSetBool("cage/config/autoSave", true);
		engineInitialize(EngineCreateConfig());
		controlThread().updatePeriod(1000000 / 30);

		listeners.attachAll(engineWindow(), 1000);
		listeners.windowClose.bind<&windowClose>();
		listeners.keyRelease.bind<&keyRelease>();
		EventListener<void()> assetsUpdateListener;
		assetsUpdateListener.bind<&assetsUpdate>();
		assetsUpdateListener.attach(controlThread().update);
		EventListener<void()> frameCounterListener;
		frameCounterListener.bind<&frameCounter>();
		frameCounterListener.attach(graphicsPrepareThread().prepare);

		engineWindow()->title("Degrid");
		reloadLanguage(confLanguage);
		engineAssets()->add(HashString("degrid/degrid.pack"));

		{
			Holder<FullscreenSwitcher> fullscreen = newFullscreenSwitcher({});
			Holder<EngineProfiling> EngineProfiling = newEngineProfiling();
			EngineProfiling->profilingScope = EngineProfilingScopeEnum::None;
			EngineProfiling->screenPosition = vec2(0.5);

			engineStart();
		}

		engineAssets()->remove(HashString("degrid/degrid.pack"));
		if (loadedLanguageHash)
			engineAssets()->remove(loadedLanguageHash);

		engineFinalize();
		return 0;
	}
	catch (...)
	{
		detail::logCurrentCaughtException();
	}
	return 1;
}
