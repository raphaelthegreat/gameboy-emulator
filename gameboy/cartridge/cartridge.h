#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>

#include <cartridge/mbc.h>

using std::unique_ptr;
using std::vector;

#define CAST(to, source) static_cast<to>(source)
#define RCAST(to, source) reinterpret_cast<to>(source)

enum class Banking{
	MBC1, 
	MBC2,
	None
};

class MMU;
class Cartridge {
public:
	Cartridge(MMU* _mmu) : mmu(_mmu) {}
	~Cartridge();

	bool load_rom(const std::string& file);
	bool has_ram() { return ram_size != 0; }

	uint8_t read(uint16_t addr);
	uint8_t& get(uint16_t addr);
	void write(uint16_t addr, uint8_t data);

public:
	Banking banking = Banking::None;
	MBC* mbc;
	MMU* mmu;

	uint8_t current_rom_bank = 1;
	uint8_t current_ram_bank = 0;
	
	uint32_t rom_size = 0;
	uint32_t ram_size = 0;

	uint8_t data[0x200000];
	uint8_t memory[0x8000];

	bool loaded = false;
	bool memory_enabled = false;
	bool rom_banking = true;
};