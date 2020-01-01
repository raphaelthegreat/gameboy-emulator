#pragma once
#include <cpu/cpu.h>
#include <vector>

#define INTERUPT_FLAG 0xFF0F
#define INTERUPT_ENABLE 0xFFFF

#define LY 0xFF44
#define LYC 0xFF45
#define DMA 0xFF46
#define LCD_STATUS 0xFF41
#define LCD_CONTROL 0xFF40

class Cartridge;
class GameBoy;
class MMU {
public:
	MMU() = default;
	~MMU() = default;

	uint8_t read(uint16_t addr);	
	uint8_t& get(uint16_t addr); // Same as read you just get a ref to memory (This is used so I can increment for example the LY without reseting it)
	void write(uint16_t addr, uint8_t data);

	void copy_bootrom(uint8_t* rom);
	void dma_transfer(uint16_t addr, uint8_t data);

public:
	Cartridge* cartridge;
	GameBoy* gb;

public:
	uint8_t bios[256], ram[8192], vram[8192];
	uint8_t wram[8192], hram[128], oam[160], io[128];

	uint8_t enable_interupts; // 0xFFFF

	bool booting = true;
};