#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>

using std::unique_ptr;
using std::vector;

class Cartridge {
public:
	Cartridge();
	~Cartridge() = default;

	bool load_rom(const std::string& file);
	bool has_ram() { return false; }

	uint8_t get_byte(uint16_t addr) { return 0x00; }
	void set_byte(uint16_t addr, uint8_t data) {}

	void enable_ram_banks(uint16_t addr, uint8_t data);

public:
	bool mbc1 = false;
	
	bool enable_ram = false;
	bool ram_banking = false;
	bool rom_banking = true;
	
	bool loaded = false;

	uint8_t current_rom_bank = 1;
	uint8_t current_ram_bank = 0;
	
	uint32_t rom_size;
	uint32_t ram_size;

	uint8_t rom[0x200000];
};