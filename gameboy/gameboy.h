#pragma once
#include <chrono>
#include <vector>
#include <functional>
#include <imgui.h>
#include <imgui_file.h>

#include <cpu/mmu.h>
#include <video/ppu.h>
#include <cartridge/joypad.h>
#include <cartridge/cartridge.h>
#include <logger.h>

template <typename T>
using ref = std::shared_ptr<T>;

#define NO_NEWLINE ImGui::SameLine()
#define NEWLINE ImGui::NewLine()

#define RED ImVec4(255, 0, 0, 255)
#define YELLOW ImVec4(255, 255, 0, 255)
#define WHITE ImVec4(255, 255, 255, 255)
#define BLACK ImVec4(0, 0, 0, 255)

class Window;
class GameBoy {
public:
	GameBoy();

	void boot(const std::string& boot);
	void load_rom(const std::string& file);

	void cpu_stats();
	void memory_map(uint16_t from, uint16_t to, uint8_t step);
	void dockspace(std::function<void()> menu_func);

	void log();
    void menu_function();

	void tick();
	void display_viewport();

public:
	const int cycles_per_frame = 17556;
	const double fps = 59.73;
	const double frame_interval = 1000.0 / fps;

public:
	MMU mmu;

	CPU cpu;
	PPU ppu;
	Joypad joypad;
	
	FileDialog file;
	Logger logger;

	sf::Sprite viewport;
	sf::IntRect view_area;
	uint32_t cycles = 0;
	uint32_t previous = 0;
	
	std::chrono::high_resolution_clock clock;
	bool frame_complete = true;
	bool bool_ptr = true;
	bool rom_loaded = false;
	bool file_dialog_opened = false;
};
