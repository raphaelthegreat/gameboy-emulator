#include "mbc.h"
#include "cartridge.h"

void MBCNone::handle_banking(uint16_t addr, uint8_t data)
{
	// Nothing to do
}

void MBC1::handle_banking(uint16_t addr, uint8_t data)
{
	if (addr < 0x2000) {
		uint8_t lnibble = data & 0x0F;

		if (lnibble == 0xA) cartridge->memory_enabled = true;
		else if (lnibble == 0) cartridge->memory_enabled = false;
	}
	else if (addr >= 0x2000 && addr < 0x4000) {
		uint8_t lower5 = data & 0b00011111;
		uint8_t& current = cartridge->current_rom_bank;

		current &= 0b11100000; // turn off the lower 5
		current |= lower5;
		if (current == 0) current++;
	}
	else if (addr >= 0x4000 && addr < 0x6000) {
		if (cartridge->rom_banking) {
			uint8_t& current = cartridge->current_rom_bank;
			current &= 31;

			// turn off the lower 5 bits of the data
			data &= 224;
			current |= data;
			if (current == 0) current++;
		}
		else {
			cartridge->current_ram_bank = data & 0x3;
		}
	}
	else if (addr >= 0x6000 && addr < 0x8000) {
		uint8_t bit = data & 1;

		if (bit == 0) cartridge->rom_banking = true;
		else cartridge->rom_banking = false;
	}
}
