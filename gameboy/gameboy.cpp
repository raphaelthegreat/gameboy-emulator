#include "gameboy.h"
#include <sstream>
#include <thread>
#include <iomanip>

GameBoy::GameBoy() : cpu(&mmu)
{
    mmu.gb = this;
    
    ppu.init(&mmu);
    joypad.init(&mmu);
	cpu.reset();
    
    viewport = sf::Sprite(ppu.frame_buffer);
    viewport.setScale(3.5, 3.5);
}

ref<Cartridge> GameBoy::load_rom(const std::string& file)
{
    auto cart = std::make_shared<Cartridge>();
    cart->load_rom(file);

    mmu.cartridge = cart.get();

    return cart;
}

void GameBoy::boot(const std::string& boot)
{
    uint8_t bootrom[256];
    
    FILE* in = fopen(boot.c_str(), "rb");
    fread(bootrom, 1, 256, in);
    fclose(in);

    cpu.pc = 0;

    mmu.copy_bootrom(bootrom);
}

void GameBoy::cpu_stats()
{
	/*ImGui::Begin("CPU-Info");
	ImGui::Text("N: %d Z: %d H: %d C: %d",
		cpu.get_flag(N),
		cpu.get_flag(Z),
		cpu.get_flag(H),
		cpu.get_flag(C));

	ImGui::Text("A: %d", cpu.af.h);
	ImGui::Text("B: %d C: %d", cpu.bc.h, cpu.bc.l);
	ImGui::Text("D: %d E: %d", cpu.de.h, cpu.de.l);
	ImGui::Text("H: %d L: %d", cpu.hl.h, cpu.hl.l);
	ImGui::Text("PC: %s", to_hex(cpu.pc).c_str());
	ImGui::Text("Stack Pointer: %s", to_hex(cpu.sp).c_str());
	ImGui::Text("Opcode: %s", to_hex(cpu.opcode).c_str());
	ImGui::NewLine();
	ImGui::Text("Instuction: %s", cpu.lookup[cpu.opcode].name.c_str());
	ImGui::End();*/
}

void GameBoy::memory_map(uint16_t from, uint16_t to, uint8_t step)
{
    /*ImGui::Begin("Memory");

    std::string line = "";
    for (int i = from; i <= to; i++) {
        
        if ((i - from) % step == 0) {
            ImGui::Text(line.c_str());           
            line.clear();
        }

        line += std::to_string(mmu.read(i)) + ' ';
    }

    ImGui::End();*/
}

void GameBoy::tick()
{
    uint32_t current_cycle = 0;

    while (current_cycle < cycles_per_frame) {
        uint32_t cycle = cpu.tick();
        current_cycle += cycle;

        cpu.update_timers(cycle);
        ppu.tick(cycle);
        
        cpu.handle_interupts();
    }
    
    ppu.blit_pixels();
}
