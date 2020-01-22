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
	gb.load_rom("../roms/mario2.gb");

	while (!window.should_close()) {
		window.update();
		
		gb.tick();		
		window.render();
	}
}