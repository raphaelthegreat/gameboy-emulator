#pragma once
#include <cpu/cpu.h>
#include <vector>
#include <memory>

using std::shared_ptr;

#define INTERUPT_FLAG 0xFF0F
#define INTERUPT_ENABLE 0xFFFF

#define LY 0xFF44
#define LYC 0xFF45
#define DMA 0xFF46
#define LCD_STATUS 0xFF41
#define LCD_CONTROL 0xFF40

#define BOOTING 0xFF50

class Cartridge;
class GameBoy;
class MMU {
public:
	MMU() = default;
	~MMU() = default;

	uint8_t read(uint16_t addr);	
	uint8_t& get(uint16_t addr); // VERY UNSAFE USE ONLY IF NECCESSARY
	void write(uint16_t addr, uint8_t data);

	void copy_bootrom(uint8_t* rom);
	void dma_transfer(uint8_t data);

public:
	shared_ptr<Cartridge> cartridge;
	GameBoy* gb;

public:
	uint8_t bios[256], memory[0x10000];
};