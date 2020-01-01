#pragma once
#include <SDL.h>
#include <SDL_opengl.h>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

#define S(x) std::to_string(x)

using std::vector;

enum LCDMode {
	HBlank = 0,
	VBlank = 1,
	OAMSearch = 2,
	DataTrans = 3
};

enum Color {
	White = 00,
	LGray = 01,
	DGray = 10,
	Black = 11
};

#define BG_PALETTE_DATA 0xFF47
#define SPRITE_PALETTE0 0xFF48
#define SPRITE_PALETTE1 0xFF49

#define SCROLL_X 0xFF42
#define SCROLL_Y 0xFF43

#define WINDOW_X 0xFF4A
#define WINDOW_Y 0xFF4B

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

	SDL_Color get_color(uint8_t value);

	void set_pixel(int x, int y, SDL_Color c);
	SDL_Color get_pixel(int x, int y);

public:
	MMU* mmu;

public:
	int scanline_counter = 0;
	LCDMode mode;

	SDL_Texture* frame_buffer;
	SDL_Surface* surface;
};