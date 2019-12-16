#include <cstdio>
#include <thread>
#include "allegro5/allegro.h"
#include "Game.h"
#include "Graphics.h"

namespace
{
	struct GameState
	{
		bool isRunning;
		Game game;
	};

	bool InitAllegro()
	{
		if (al_init())
		{
			if (al_install_mouse())
			{
				if (al_install_keyboard())
				{
					return true;
				}

				al_uninstall_mouse();
			}

			al_uninstall_system();
		}

		return false;
	}

	static void ProcessEvents(ALLEGRO_EVENT_QUEUE* queue, GameState* state)
	{
		while (!al_is_event_queue_empty(queue))
		{
			ALLEGRO_EVENT evt;

			al_get_next_event(queue, &evt);
			switch (evt.type)
			{
				case ALLEGRO_EVENT_DISPLAY_CLOSE:
					state->isRunning = false;
				break;
				case ALLEGRO_EVENT_DISPLAY_RESIZE:
					gfx::Resize(state->game.gfx);
				break;
				case ALLEGRO_EVENT_KEY_UP:
					switch (evt.keyboard.keycode)
					{
						case ALLEGRO_KEY_ESCAPE:
							state->isRunning = false;
						break;
						case ALLEGRO_KEY_F3:
							printf("Reloading shaders\n");
							gfx::ReloadShaders(state->game.gfx);
						break;
					}
				break;
			}
		}
	}
}

int main(int argc, char* argv[])
{
	if (!InitAllegro())
	{
		printf("Failed to initialize allegro.\n");
	}
	else
	{
		GameState state{ true };

		if (!game::Init(&state.game))
		{
			printf("Failed to initialize game.\n");
		}
		else
		{
			// Create event queue
			ALLEGRO_EVENT_QUEUE* const eventQueue = al_create_event_queue();
			al_register_event_source(eventQueue, al_get_mouse_event_source());
			al_register_event_source(eventQueue, al_get_keyboard_event_source());
			al_register_event_source(eventQueue, al_get_display_event_source(gfx::GetDisplay(state.game.gfx)));

			while (state.isRunning)
			{
				ProcessEvents(eventQueue, &state);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			game::Shutdown(&state.game);

			al_destroy_event_queue(eventQueue);
		}

		al_uninstall_keyboard();
		al_uninstall_mouse();
		al_uninstall_system();
	}

	return 0;
}
