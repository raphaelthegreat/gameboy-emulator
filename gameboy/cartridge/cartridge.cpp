#include "cartridge.h"

Cartridge::~Cartridge()
{
    delete mbc;
}

bool Cartridge::load_rom(const std::string& path)
{
    memset(data, 0, sizeof(data));
    memset(memory, 0, sizeof(memory));

    FILE* in = fopen(path.c_str(), "rb");
    fread(data, 1, 0x200000, in);
    fclose(in);

    uint8_t banking_type = data[0x147];
    if (banking_type == 0) banking = Banking::None;
    else if (banking_type == 1 || banking_type == 2 || banking_type == 3) banking = Banking::MBC1;

    std::cout << "Banking Type: " << (int)banking_type << '\n';

    uint8_t ram_type = data[0x149];
    if (ram_type == 0) ram_size = 0;
    else if (ram_type == 1) ram_size = 2048;
    else if (ram_type == 2) ram_size = 8192;

    if (banking == Banking::None) mbc = new MBCNone(this);
    if (banking == Banking::MBC1) mbc = new MBC1(this);

    loaded = true;

    return true;
}

uint8_t Cartridge::read(uint16_t addr)
{
    uint8_t value = 0;
    
    if (addr >= 0x0000 && addr <= 0x3FFF) {
        value = data[addr];
    }
    else if (addr >= 0x4000 && addr <= 0x7FFF) {
        uint16_t address = (addr - 0x4000) + (current_rom_bank * 0x4000);
        value = data[address];
    }
    else if (addr >= 0xA000 && addr <= 0xBFFF) {
        uint16_t address = addr - 0xA000 + (current_ram_bank * 0x2000);
        value = memory[address];
    }

    return value;
}

uint8_t& Cartridge::get(uint16_t addr)
{
    std::reference_wrapper<uint8_t> value = data[0];
    
    if (addr >= 0x0000 && addr <= 0x3FFF) {
        value = data[addr];
    }
    else if (addr >= 0x4000 && addr <= 0x7FFF) {
        uint16_t address = addr - 0x4000;
        value = data[address + (current_rom_bank * 0x4000)];
    }
    else if (addr >= 0xA000 && addr <= 0xBFFF) {
        uint16_t address = addr - 0xA000;
        value = memory[address + (current_ram_bank * 0x2000)];
    }

    return value.get();
}

void Cartridge::write(uint16_t addr, uint8_t data)
{
    if (addr < 0x8000)    
        mbc->handle_banking(addr, data);   
    else if (addr >= 0xA000 && addr < 0xC000) {
        if (memory_enabled) {
            uint16_t address = addr - 0xA000;
            memory[address + (current_ram_bank * 0x2000)] = data;
        }
    }
}
