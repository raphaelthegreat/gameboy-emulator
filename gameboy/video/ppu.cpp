#include "ppu.h"
#include <gameboy.h>
#include <video/window.h>
#include <cpu/mmu.h>
#include <fstream>

void PPU::init(MMU* _mmu)
{
	mmu = _mmu;

	pixels.create(256, 256, sf::Color::White);
	frame_buffer.loadFromImage(pixels);
}

void PPU::tick(uint32_t cycles)
{
	uint8_t status = mmu->read(LCD_STATUS);
	uint8_t scanline = mmu->read(LY);
	LCDMode mode, current = (LCDMode)(mmu->read(LCD_CONTROL) & 3);

	uint8_t view_x = mmu->read(SCROLL_X);
	uint8_t view_y = mmu->read(SCROLL_Y);

	mmu->gb->view_area = sf::Rect<int>(view_x, view_y, 160, 144);

	bool should_interupt = false;

	if (!lcd_enabled()) { 
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
	uint8_t view_x = mmu->read(SCROLL_X);
	uint8_t view_y = mmu->read(SCROLL_Y);

	uint8_t win_x = mmu->read(WINDOW_X) - 7;
	uint8_t win_y = mmu->read(WINDOW_Y);

	uint8_t lcd_control = mmu->read(LCD_CONTROL);
	uint8_t scanline = mmu->read(LY);
	
	bool signed_data = false;
	bool window = false;

	bool tiledata_select = CPU::get_bit(lcd_control, 4);
	uint16_t tile_data = tiledata_select ? 0x8000 : 0x8800;
	signed_data = !tiledata_select;

	uint16_t tile_map = 0;

	uint8_t palette = mmu->read(BG_PALETTE_DATA);
	uint8_t offx = 0, offy = 0;

	for (int x = 0; x < 256; x++) {
		
		if (CPU::get_bit(lcd_control, 5)) {
			if (x >= win_x && scanline >= win_y)
				window = true;
		}
		
		if (!window) {
			if (CPU::get_bit(lcd_control, 3))
				tile_map = 0x9C00;
			else
				tile_map = 0x9800;
		}
		else {
			if (CPU::get_bit(lcd_control, 6))
				tile_map = 0x9C00;
			else
				tile_map = 0x9800;
		}

		if (window) {
			offx = x - win_x;
			offy = scanline - win_y;
		}
		else {
			offx = x + view_x;
			offy = view_y + scanline;
		}

		uint8_t tilex = offx / 8, tiley = offy / 8;
		uint8_t tilexc = offx % 8, tileyc = offy % 8;
		
		uint16_t offset = (tiley * 32) + tilex;

		uint8_t tilen = mmu->read(tile_map + offset);
		uint8_t colorval = 0;
		
		if (tile_data == 0x8800) {
			int8_t tile_num = T8(tilen);
			uint16_t tileaddr = tile_data + 0x800 + (tile_num * 16);
			uint16_t tile_line = tileaddr + (tileyc * 2);

			uint8_t byte1 = mmu->read(tile_line);
			uint8_t byte2 = mmu->read(tile_line + 1);

			uint8_t bit1 = (byte1 >> (7 - tilexc) & 0x1);
			uint8_t bit2 = (byte2 >> (7 - tilexc) & 0x1);

			colorval = (bit2 << 1) | bit1;
		}
		else {
			uint16_t tileaddr = tile_data + (tilen * 16);
			uint16_t tile_line = tileaddr + (tileyc * 2);

			uint8_t byte1 = mmu->read(tile_line);
			uint8_t byte2 = mmu->read(tile_line + 1);

			uint8_t bit1 = (byte1 >> (7 - tilexc)) & 0x1;
			uint8_t bit2 = (byte2 >> (7 - tilexc)) & 0x1;
			
			colorval = (bit2 << 1) | bit1;
		}

		sf::Color color = get_color(colorval, palette, Background);
		pixels.setPixel(x, scanline, color);
	}
}

void PPU::draw_sprites()
{
	uint8_t lcd_control = mmu->read(LCD_CONTROL);
	uint8_t scanline = mmu->read(LY);
	uint16_t sprite_data = 0x8000;

	for (int sprite = 0; sprite < 40; sprite++) {
		uint8_t index = sprite * 4;
		uint8_t y_pos = mmu->read(SPRITE_ATTR + index) - 16;
		uint8_t x_pos = mmu->read(SPRITE_ATTR + index + 1) - 8;
		
		uint8_t tile_num = mmu->read(SPRITE_ATTR + index + 2);
		uint8_t attr = mmu->read(SPRITE_ATTR + index + 3);

		uint8_t sprite_height = CPU::get_bit(lcd_control, 2) ? 16 : 8;
		uint16_t palette_addr = CPU::get_bit(attr, 4) ? SPRITE_PALETTE1 : SPRITE_PALETTE0;
		uint8_t palette = mmu->read(palette_addr);

		bool x_flip = CPU::get_bit(attr, 5);
		bool y_flip = CPU::get_bit(attr, 6);

		if (scanline >= y_pos && scanline < (y_pos + sprite_height)) {
			int line = scanline - y_pos;

			if (y_flip) {
				line -= sprite_height;
				line *= -1;
			}

			line *= 2;
			uint16_t tile_addr = (0x8000 + (tile_num * 16)) + line;
			
			uint8_t byte1 = mmu->read(tile_addr);
			uint8_t byte2 = mmu->read(tile_addr + 1);

			for (int row_pixel = 7; row_pixel >= 0; row_pixel--) {
				
				int colourbit = row_pixel;
				if (x_flip) {
					colourbit -= 7;
					colourbit *= -1;
				}

				uint8_t bit2 = CPU::get_bit(byte2, colourbit);
				uint8_t bit1 = CPU::get_bit(byte1, colourbit);

				int colorval = (bit2 << 1) | bit1;
				sf::Color color = get_color(colorval, palette, Sprite);

				if (colorval == 0)
					continue;

				int pixel = x_pos + (7 - row_pixel);

				if ((scanline < 0) || (scanline > 143) || (pixel < 0) || (pixel > 159)) {
					continue;
				}

				pixels.setPixel(pixel, scanline, color);
			}
		}
	}
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
	frame_buffer.loadFromImage(pixels);
}

sf::Color PPU::get_color(uint8_t value, uint8_t palette, Object obj)
{
	uint8_t colorfrompal = (palette >> (2 * value)) & 3;
	
	if (colorfrompal == 0)
		return sf::Color::White;
	else if (colorfrompal == 1)
		return sf::Color(192, 192, 192, 255);
	else if (colorfrompal == 2)
		return sf::Color(96, 96, 96, 255);
	else if (colorfrompal == 3)
		return sf::Color::Black;
}
