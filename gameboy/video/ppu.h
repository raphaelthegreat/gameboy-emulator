#pragma once
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

#define S(x) std::to_string(x)

using std::vector;

enum Object {
	Background,
	Sprite
};

enum LCDMode {
	HBlank = 0,
	VBlank = 1,
	OAMSearch = 2,
	DataTrans = 3
};

#define BG_PALETTE_DATA 0xFF47
#define SPRITE_PALETTE0 0xFF48
#define SPRITE_PALETTE1 0xFF49

#define SCROLL_X 0xFF43
#define SCROLL_Y 0xFF42

#define WINDOW_X 0xFF4B
#define WINDOW_Y 0xFF4A

#define SPRITE_ATTR 0xFE00

class MMU;
class PPU {
public:
	PPU() = default;
	~PPU() = default;

	void init(MMU* mmu);

	void tick(uint32_t cycles);
	bool lcd_enabled();

	void draw_tiles();
	void draw_sprites();

	void draw_line();
	void blit_pixels();

	sf::Color get_color(uint8_t value, uint8_t palette, Object obj);

public:
	MMU* mmu;

public:
	int scanline_counter = 0;
	LCDMode mode;

	sf::Texture frame_buffer;
	
	sf::Image pixels;
};