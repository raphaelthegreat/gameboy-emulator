#include "mmu.h"
#include <cartridge/cartridge.h>

#pragma warning(disable : 6385)
#pragma warning(disable : 6386)

uint8_t MMU::read(uint16_t addr)
{
	uint8_t data = 0x00;
	
	if (addr >= 0x0000 && addr <= 0x00FF && booting == true)
		data = bios[addr];
	else if (addr >= 0x0000 && addr <= 0x3FFF)
		data = cartridge->rom[addr];
	else if (addr >= 0x4000 && addr <= 0x7FFF)
		data = cartridge->rom[addr];
	else if (addr >= 0x8000 && addr <= 0x9FFF)
		data = vram[addr - 0x8000];
	else if (addr >= 0xA000 && addr <= 0xBFFF)
		; // Cartridge Ram
	else if (addr >= 0xC000 && addr <= 0xDFFF)
		data = wram[addr - 0xC000];
	else if (addr >= 0xE000 && addr <= 0xFDFF)
		; // For the future
	else if (addr >= 0xFE00 && addr <= 0xFE9F)
		data = oam[addr - 0xFE00];
	else if (addr >= 0xFF00 && addr <= 0xFF7F)
		data = io[addr - 0xFF00];
	else if (addr >= 0xFF80 && addr <= 0xFFFE)
		data = hram[addr - 0xFF80];
	else if (addr == 0xFFFF)
		data = enable_interupts;

	return data;
}

uint8_t& MMU::get(uint16_t addr)
{
	std::reference_wrapper<uint8_t> data = cartridge->rom[0];
	
	if (addr >= 0x0000 && addr <= 0x00FF && booting == true)
		data = bios[addr];
	else if (addr >= 0x0000 && addr <= 0x3FFF)
		data = cartridge->rom[addr];
	else if (addr >= 0x4000 && addr <= 0x7FFF)
		data = cartridge->rom[addr];
	else if (addr >= 0x8000 && addr <= 0x9FFF)
		data = vram[addr - 0x8000];
	else if (addr >= 0xA000 && addr <= 0xBFFF)
		;
	else if (addr >= 0xC000 && addr <= 0xDFFF)
		data = wram[addr - 0xC000];
	else if (addr >= 0xE000 && addr <= 0xFDFF)
		; // For the future
	else if (addr >= 0xFE00 && addr <= 0xFE9F)
		data = oam[addr - 0xFE00];
	else if (addr >= 0xFF00 && addr <= 0xFF7F)
		data = io[addr - 0xFF00];
	else if (addr >= 0xFF80 && addr <= 0xFFFE)
		data = hram[addr - 0xFF80];
	else if (addr == 0xFFFF)
		data = enable_interupts;

	return data.get();
}

void MMU::write(uint16_t addr, uint8_t data)
{
	if (addr == LY)
		io[LY - 0xFF00] = 0;
	if (addr == DMA)
		dma_transfer(addr, data);
	if (addr == DIV)
		io[addr - 0xFF00] = 0;
	else if (addr >= 0x8000 && addr <= 0x9FFF)
		vram[addr - 0x8000] = data;
	else if (addr >= 0xA000 && addr <= 0xBFFF)
		cartridge->get_byte(addr - 0xA000);
	else if (addr >= 0xC000 && addr <= 0xDFFF)
		wram[addr - 0xC000] = data;
	else if (addr >= 0xE000 && addr <= 0xFDFF)
		; // For the future
	else if (addr >= 0xFE00 && addr <= 0xFE9F)
		oam[addr - 0xFE00] = data;
	else if (addr >= 0xFF00 && addr <= 0xFF7F)
		io[addr - 0xFF00] = data;
	else if (addr >= 0xFF80 && addr <= 0xFFFE)
		hram[addr - 0xFF80] = data;
	else if (addr == 0xFFFF)
		enable_interupts = data;
}

void MMU::copy_bootrom(uint8_t* rom)
{
	for (int i = 0; i < 256; i++)
		bios[i] = rom[i];
}

void MMU::dma_transfer(uint16_t addr, uint8_t data)
{
	uint16_t address = data * 100;
	for (int i = 0; i < 160; i++) {
		write(0xFE00 + i, read(address + i));
	}
}
