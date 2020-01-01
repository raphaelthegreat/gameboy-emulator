#include <video/window.h>
#include <gameboy.h>
#include <thread>
#include <iomanip>
#include <iostream>

void main()
{
	Window window(580, 504, "Gameboy");
	GameBoy gb(&window);
	
	gb.boot("bios.gb");
	auto cart = gb.load_rom("../assets/tetris.gb");

	while (!window.should_close()) {
		window.update();				

		gb.tick();
		window.render(gb.ppu.frame_buffer);
	}
}