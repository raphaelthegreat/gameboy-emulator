#pragma once
#include <chrono>
#include <vector>

#include <cpu/mmu.h>
#include <video/ppu.h>
#include <cartridge/cartridge.h>

template <typename T>
using ref = std::shared_ptr<T>;

class Window;
class GameBoy {
public:
	GameBoy(Window* window);

	void boot(const std::string& boot);
	ref<Cartridge> load_rom(const std::string& file);

	/* Debug functions */
	void cpu_stats();
	void memory_map(uint16_t from, uint16_t to, uint8_t step);

	void tick();

public:
	const int cycles_per_frame = 17556;
	const double fps = 59.73;
	const double frame_time = 1000.0 / fps;

public:
	Window* window;
	MMU mmu;

	CPU cpu;
	PPU ppu;

	sf::Sprite viewport;
	uint32_t cycles = 0;
	uint32_t previous = 0;
	bool frame_complete = true;
};
