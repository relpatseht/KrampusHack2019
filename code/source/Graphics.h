#pragma once

struct Graphics;
struct ALLEGRO_DISPLAY;

namespace gfx
{
	Graphics* Init();

	void ReloadShaders(Graphics* g);
	void Resize(Graphics* g);

	ALLEGRO_DISPLAY* GetDisplay(Graphics* g);

	void Shutdown(Graphics* g);
}