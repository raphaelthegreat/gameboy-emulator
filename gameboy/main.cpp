#include <video/window.h>
#include <gameboy.h>
#include <thread>
#include <iomanip>
#include <iostream>

void main()
{
	GameBoy gb;
	Window window(800, 600, "Gameboy Emulator", &gb);

	gb.boot("bios.gb");

	while (!window.should_close()) {
		window.update();
		
		if (gb.rom_loaded) {
			gb.tick();
		}

		gb.dockspace(std::bind(&GameBoy::menu_function, &gb));
		gb.cpu_stats();
		gb.memory_map(0x0000, 0xFFFF, 21);
		gb.display_viewport();

		window.render();
	}
}