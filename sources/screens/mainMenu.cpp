#include "screens.h"

#include <cage-core/config.h>

void reloadLanguage(uint32 index);
extern configUint32 confLanguage;

namespace
{
	eventListener<bool(uint32)> guiEvent;

	bool guiFunction(uint32 en)
	{
		switch (en)
		{
		case 103:
			setScreenGame();
			return true;
		case 104:
			setScreenOptions();
			return true;
		case 105:
			setScreenScores();
			return true;
		case 106:
			setScreenCredits();
			return true;
		case 107:
			engineStop();
			return true;
		}
		if (en >= 200)
		{
			reloadLanguage(confLanguage = en - 200);
			return true;
		}
		return false;
	}
}

void setScreenMainmenu()
{
	{
		guiConfig c;
		c.backButton = false;
		regenerateGui(c);
	}
	entityManagerClass *ents = gui()->entities();
	guiEvent.bind<&guiFunction>();
	guiEvent.attach(gui()->widgetEvent);

	{ // main menu
		{
			entityClass *e = ents->get(12);
			GUI_GET_COMPONENT(scrollbars, sc, e);
			sc.alignment = vec2(0.8, 0.66);
		}

		entityClass *panel = ents->createUnique();
		{
			GUI_GET_COMPONENT(panel, panel2, panel);
			GUI_GET_COMPONENT(parent, parent, panel);
			parent.parent = 12;
			GUI_GET_COMPONENT(layoutLine, layout, panel);
			layout.vertical = true;
		}

		{
			entityClass *butNewGame = ents->create(103);
			GUI_GET_COMPONENT(parent, parent, butNewGame);
			parent.parent = panel->name();
			parent.order = 1;
			GUI_GET_COMPONENT(button, control, butNewGame);
			GUI_GET_COMPONENT(text, txt, butNewGame);
			txt.assetName = hashString("grid/languages/internationalized.textpack");
			txt.textName = hashString("gui/mainmenu/newgame");
		}

		{
			entityClass *butOptions = ents->create(104);
			GUI_GET_COMPONENT(parent, parent, butOptions);
			parent.parent = panel->name();
			parent.order = 2;
			GUI_GET_COMPONENT(button, control, butOptions);
			GUI_GET_COMPONENT(text, txt, butOptions);
			txt.assetName = hashString("grid/languages/internationalized.textpack");
			txt.textName = hashString("gui/mainmenu/options");
		}

		{
			entityClass *butScores = ents->create(105);
			GUI_GET_COMPONENT(parent, parent, butScores);
			parent.parent = panel->name();
			parent.order = 3;
			GUI_GET_COMPONENT(button, control, butScores);
			GUI_GET_COMPONENT(text, txt, butScores);
			txt.assetName = hashString("grid/languages/internationalized.textpack");
			txt.textName = hashString("gui/mainmenu/scores");
		}

		{
			entityClass *butCredits = ents->create(106);
			GUI_GET_COMPONENT(parent, parent, butCredits);
			parent.parent = panel->name();
			parent.order = 4;
			GUI_GET_COMPONENT(button, control, butCredits);
			GUI_GET_COMPONENT(text, txt, butCredits);
			txt.assetName = hashString("grid/languages/internationalized.textpack");
			txt.textName = hashString("gui/mainmenu/credits");
		}

		{
			entityClass *butQuit = ents->create(107);
			GUI_GET_COMPONENT(parent, parent, butQuit);
			parent.parent = panel->name();
			parent.order = 5;
			GUI_GET_COMPONENT(button, control, butQuit);
			GUI_GET_COMPONENT(text, txt, butQuit);
			txt.assetName = hashString("grid/languages/internationalized.textpack");
			txt.textName = hashString("gui/mainmenu/quit");
		}
	}

	{ // languages
		entityClass *column = ents->createUnique();
		{
			GUI_GET_COMPONENT(parent, parent, column);
			parent.parent = 10;
			GUI_GET_COMPONENT(layoutLine, layout, column);
			layout.vertical = true;
		}

		static const uint32 flags[] = {
			hashString("grid/languages/english.png"),
			hashString("grid/languages/czech.png")
		};

		for (uint32 i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
		{
			entityClass *but = ents->create(200 + i);
			GUI_GET_COMPONENT(parent, parent, but);
			parent.parent = column->name();
			parent.order = i;
			GUI_GET_COMPONENT(button, control, but);
			GUI_GET_COMPONENT(image, img, but);
			img.textureName = flags[i];
			GUI_GET_COMPONENT(explicitSize, size, but);
			size.size = vec2(80, 40);
		}
	}
}