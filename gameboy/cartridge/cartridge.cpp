#include "cartridge.h"

Cartridge::Cartridge()
{
    current_rom_bank = 0;
}

bool Cartridge::load_rom(const std::string& path)
{
    memset(rom, 0, sizeof(rom));

    FILE* in = fopen(path.c_str(), "rb");
    fread(rom, 1, 0x200000, in);
    fclose(in);

    uint8_t has_ram = rom[0x0149];
    if (has_ram == 1) {
        //ram = std::vector<uint8_t>(2 * 1024, 0);
        ram_size = 2048;
    }
    else if (has_ram == 2) {
        //ram = std::vector<uint8_t>(8 * 1024, 0);
        ram_size = 8192;
    }

    return true;
}

void Cartridge::enable_ram_banks(uint16_t addr, uint8_t data)
{
    if (mbc1) {
        uint8_t low = data & 0x0F;

        if (low == 0xA) enable_ram = true;        
        else if (low == 0x0) enable_ram = false;
    }
}
