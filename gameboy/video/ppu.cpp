#include "ppu.h"
#include <gameboy.h>
#include <video/window.h>
#include <cpu/mmu.h>
#include <fstream>

void PPU::init(MMU* _mmu)
{
	mmu = _mmu;

	surface = SDL_CreateRGBSurface(0, 160, 144, 32, 0, 0, 0, 0);
	SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 255, 255, 255));

	frame_buffer = SDL_CreateTextureFromSurface(mmu->gb->window->renderer, surface);
}

void PPU::tick(uint32_t cycles)
{
	uint8_t status = mmu->read(LCD_STATUS);
	uint8_t scanline = mmu->read(LY);
	LCDMode mode, current = (LCDMode)(mmu->read(LCD_CONTROL) & 3);

	bool should_interupt = false;
	
	if (!lcd_enabled()) { // Taken from codeslinger.co.uk
		scanline_counter = 0;

		status &= 0b11111100; // Clear mode bits
		CPU::set_bit(status, 1, 0); // Enter V-Blank because lcd is disabled
		
		mmu->write(LCD_STATUS, status);
		mmu->write(LY, 0);
		return;
	}
	else
		scanline_counter += cycles; // Each scanline takes some clock cycles to complete

	if (scanline >= 0 && scanline <= 143) { // I am currently drawing a frame
		
		if (scanline_counter > 0 && scanline_counter <= 20) { // I am in OAM Search mode
			mode = LCDMode::OAMSearch;

			CPU::set_bit(status, 0, 0);
			CPU::set_bit(status, 1, 1);
			should_interupt = CPU::get_bit(status, 5);
		}
		else if (scanline_counter > 20 && scanline_counter <= 63) { // I am in Pixel Transfer mode
			mode = LCDMode::DataTrans;

			CPU::set_bit(status, 0, 1);
			CPU::set_bit(status, 1, 1);
		}
		else if (scanline_counter > 63 && scanline_counter <= 114) { // I am in H-Blank mode
			mode = LCDMode::HBlank;

			CPU::set_bit(status, 0, 0);
			CPU::set_bit(status, 1, 0);
			should_interupt = CPU::get_bit(status, 3);
		}
	}
	else if (scanline >= 144) { // Entered V-blank
		mode = LCDMode::VBlank;

		CPU::set_bit(status, 0, 1);
		CPU::set_bit(status, 1, 0);
		should_interupt = CPU::get_bit(status, 4);
	}

	if (should_interupt && mode != current) // Generate LCD Interupt if I changed mode
		mmu->gb->cpu.interupt(LCD_INTERUPT);

	if (mmu->read(LY) == mmu->read(LYC)) {  // Coincidence interupt
		CPU::set_bit(status, 2, 1);

		if (CPU::get_bit(status, 6))
			mmu->gb->cpu.interupt(LCD_INTERUPT);
	}
	else {
		CPU::set_bit(status, 2, 0);
	}

	if (scanline == 153 && scanline_counter >= 114) __debugbreak();

	if (scanline_counter >= 114) { // Just finished a scanline ?
		
		scanline_counter = 0;
		scanline++; // Increment scanline

		mmu->get(LY) = scanline;

		if (scanline >= 0 && scanline <= 143)
			draw_line();
		else if (scanline == 144)
			mmu->gb->cpu.interupt(VBLANK_INTERUPT);
		else if (scanline >= 153)
			mmu->write(LY, 0); // Reset scanline
	}

	mmu->write(LCD_STATUS, status); // Set status
}

bool PPU::lcd_enabled()
{
	uint8_t lcd = mmu->read(LCD_CONTROL);
	return CPU::get_bit(lcd, 7);
}

void PPU::draw_tiles()
{
	uint16_t tile_data = 0;
	uint16_t tile_map = 0;

	uint8_t view_x = mmu->read(SCROLL_X);
	uint8_t view_y = mmu->read(SCROLL_Y);

	uint8_t lcd_control = mmu->read(LCD_CONTROL);
	uint8_t scanline = mmu->read(LY);
	bool signed_data = false;

	// Select the tile data needed (we need to say if it signed or not)
	bool tiledata_select = CPU::get_bit(lcd_control, 4);
	tile_data = tiledata_select ? 0x8000 : 0x8800;
	signed_data = !tiledata_select;

	tile_map = CPU::get_bit(lcd_control, 3) ? 0x9C00 : 0x9800;
	uint8_t palette = mmu->read(BG_PALETTE_DATA);

	for (int x = 0; x < 256; x++) {			
		uint8_t offx = x + view_x;
		uint8_t offy = scanline + view_y;
		
		uint8_t offset = (offx / 8);
		offset += offy % 8 == 0 ? (offy / 8 * 32) : 0;

		uint8_t tile_num = mmu->read(tile_map + offset);
		uint8_t tile_x = offx % 8, tile_y = offy % 8;

		uint8_t colorval;
		if (signed_data) {

		}
		else {
			uint8_t tile_addr = tile_data + (tile_num * 16) + (tile_y * 2);
			
			uint8_t bit1 = mmu->read(tile_addr) >> (7 - tile_x);
			uint8_t bit2 = mmu->read(tile_addr + 1) >> (7 - tile_x);

			colorval = (bit2 << 1) | bit1;
		}

		SDL_Color color = get_color(colorval);		
		set_pixel(offx, offy, color);
	}
}

void PPU::draw_sprites()
{
}

void PPU::draw_line()
{
	uint8_t lcd_control = mmu->read(LCD_CONTROL);

	if (CPU::get_bit(lcd_control, 0))
		draw_tiles();
	if (CPU::get_bit(lcd_control, 1))
		draw_sprites();
}

void PPU::blit_pixels()
{
	SDL_UpdateTexture(frame_buffer, NULL, surface->pixels, surface->pitch);
}

SDL_Color PPU::get_color(uint8_t value)
{
	SDL_Color color;
	color.a = 255;
	
	if (value == 0) {
		color.r = 255; color.g = 255; color.b = 255;
	}
	else if (value == 1) {
		color.r = 192; color.g = 192; color.b = 192;
	}
	else if (value == 2) {
		color.r = 96; color.g = 96; color.b = 96;
	}
	else if (value == 3) {
		color.r = 0; color.g = 0; color.b = 0;
	}

	return color;
}

void PPU::set_pixel(int x, int y, SDL_Color c)
{
	int bpp = surface->format->BytesPerPixel;
	uint8_t* p = (uint8_t*)surface->pixels + y * surface->pitch + x * bpp;

	uint32_t pixel = SDL_MapRGB(surface->format, c.r, c.g, c.b);

	if (bpp == 1)
		*p = pixel;
	else if (bpp == 2)
		*(uint16_t*)p = pixel;
	else if (bpp == 3) {
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			p[0] = (pixel >> 16) & 0xff;
			p[1] = (pixel >> 8) & 0xff;
			p[2] = pixel & 0xff;
		}
		else {
			p[0] = pixel & 0xff;
			p[1] = (pixel >> 8) & 0xff;
			p[2] = (pixel >> 16) & 0xff;
		}
	}
	else if (bpp == 4)
		*(uint32_t*)p = pixel;
}

SDL_Color PPU::get_pixel(int x, int y)
{
	int bpp = surface->format->BytesPerPixel;
	uint8_t* p = (uint8_t*)surface->pixels + y * surface->pitch + x * bpp;
	uint8_t pixel;

	if (bpp == 1)
		pixel = *p;
	else if (bpp == 2)
		pixel = *(uint16_t*)p;
	else if (bpp == 3) {
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
			pixel = p[0] << 16 | p[1] << 8 | p[2];
		else
			pixel = p[0] | p[1] << 8 | p[2] << 16;
	}
	else if (bpp == 4)
		pixel = *(uint32_t*)p;
	else
		pixel = 0;

	SDL_Color rgb;
	SDL_GetRGB(pixel, surface->format, &rgb.r, &rgb.g, &rgb.b);

	return rgb;
}
