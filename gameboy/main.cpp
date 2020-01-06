#include <video/window.h>
#include <gameboy.h>
#include <thread>
#include <iomanip>
#include <iostream>

void main()
{
	GameBoy gb;
	Window window(560, 504, "Gameboy Emulator", &gb);
	
	gb.boot("bios.gb");
	auto cart = gb.load_rom("../assets/mario.gb");
	
	while (!window.should_close()) {
		window.update();

		gb.tick();
		window.render(gb.viewport);
	}
}