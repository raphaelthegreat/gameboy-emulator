#include "mmu.h"
#include <cartridge/cartridge.h>
#include <gameboy.h>
#include <cpu/timer.h>

#pragma warning(disable : 6385)
#pragma warning(disable : 6386)

uint8_t MMU::read(uint16_t address)
{
	uint8_t data = 0x00;
	LCDMode mode = gb->ppu.mode;

	if (address >= 0x0000 && address <= 0x00FF && !memory[BOOTING]) {
		data = bios[address];
	}
	else if (address == JOYPAD) {
		data = gb->joypad.read();
	}
	else if (address >= 0x0000 && address <= 0x7FFF) {
		data = cartridge->read(address);
	}
	else if (address >= 0xA000 && address <= 0xBFFF) {
		data = cartridge->read(address);
	}
	else {
		data = memory[address];
	}

	return data;
}

uint8_t& MMU::get(uint16_t address)
{
	std::reference_wrapper<uint8_t> data = cartridge->data[0];

	data = memory[address];
	return data.get();
}

void MMU::write(uint16_t address, uint8_t data)
{
	LCDMode mode = gb->ppu.mode;

	if (address >= 0xFEA0 && address < 0xFEFF) return;

	if (address < 0x8000 || (address >= 0xA000 && address <= 0xBFFF)) {
		cartridge->write(address, data);
	}
	else if (address == DMA) {
		dma_transfer(data);
	}
	else if (address == DIV || address == LY) {
		memory[address] = 0;
	}
	if (address >= 0xE000 && address < 0xFE00) { // Echo Ram
		memory[address - 0x2000] = data;
		memory[address] = data;
	}
	else if (address == JOYPAD) {
		memory[address] = data & 0x30;
	}
	else {
		memory[address] = data;
	}
}

void MMU::copy_bootrom(uint8_t* rom)
{
	for (int i = 0; i < 256; i++)
		bios[i] = rom[i];
}

void MMU::dma_transfer(uint8_t data)
{
	uint16_t address = data << 8;
	for (int i = 0; i < 160; i++) {
		write(0xFE00 + i, read(address + i));
	}
}
