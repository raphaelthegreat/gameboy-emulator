#include <video/window.h>
#include <gameboy.h>
#include <thread>
#include <iomanip>
#include <iostream>

void main()
{
	GameBoy gb;
	Window window(1920, 1080, "Gameboy Emulator", &gb);

	gb.boot("bios.gb");

	while (!window.should_close()) {
		window.update();
		
		gb.tick();		

		gb.dockspace(std::bind(&GameBoy::menu_function, &gb));
		gb.cpu_stats();
		gb.log();
		gb.memory_map(0x0000, 0xFFFF, 5);
		gb.display_viewport();

		window.render();
	}
}