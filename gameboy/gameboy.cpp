#include "gameboy.h"
#include <sstream>
#include <thread>
#include <iomanip>
#include <logger.h>
#include <imgui/imgui_textcolor.h>

GameBoy::GameBoy() : cpu(&mmu)
{
    mmu.gb = this;
    
    ppu.init(&mmu);
    joypad.init(&mmu);
	cpu.reset();
    
    viewport = sf::Sprite(ppu.frame_buffer);
    viewport.setScale(3.5, 3.5);
}

void GameBoy::load_rom(const std::string& file)
{
    mmu.cartridge = std::make_shared<Cartridge>(&mmu);
    mmu.cartridge->load_rom(file);
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
    bool n = cpu.get_flag(N), z = cpu.get_flag(Z), 
         h = cpu.get_flag(H), c = cpu.get_flag(C);
    
    ImGui::Begin("CPU-Info");
    
    ImVec2 size = ImGui::GetWindowSize();

    NEWLINE;
    std::string dashes = std::string(12, '-');
    ImGui::Text("    %s %s %s", dashes.c_str(), "Flags", dashes.c_str());

    NEWLINE;
    ImGui::Text("       N:"); NO_NEWLINE;
    ImGui::TextAnsiColored(n ? YELLOW : WHITE, "%d", n); NO_NEWLINE;
    ImGui::Text("   Z:"); NO_NEWLINE;
    ImGui::TextAnsiColored(z ? YELLOW : WHITE, "%d", z); NO_NEWLINE;
    ImGui::Text("   H:"); NO_NEWLINE;
    ImGui::TextAnsiColored(h ? YELLOW : WHITE, "%d", h); NO_NEWLINE;
    ImGui::Text("   C:"); NO_NEWLINE;
    ImGui::TextAnsiColored(c ? YELLOW : WHITE, "%d", c); NEWLINE;
    
    ImGui::Text("   %s %s %s", dashes.c_str(), "Registers", dashes.c_str());

    NEWLINE;
	ImGui::Text("       B: %s      C: %s", to_hex_string(cpu.bc.h).c_str(), to_hex_string(cpu.bc.l).c_str());
	ImGui::Text("       D: %s      E: %s", to_hex_string(cpu.de.h).c_str(), to_hex_string(cpu.de.l).c_str());
	ImGui::Text("       H: %s      L: %s", to_hex_string(cpu.hl.h).c_str(), to_hex_string(cpu.hl.l).c_str());
    ImGui::Text("       A: %s      F: %s", to_hex_string(cpu.af.h).c_str(), to_hex_string(cpu.af.l).c_str());
    
    NEWLINE;
    ImGui::Text("       Program Counter: %s", to_hex_string(cpu.pc).c_str());
	ImGui::Text("       Stack Pointer:   %s", to_hex_string(cpu.sp).c_str());

    NEWLINE;
    ImGui::Text("       Opcode:   %s ", to_hex_string(cpu.opcode).c_str());
	ImGui::Text("       Mnemonic: %s", cpu.lookup[cpu.opcode].name.c_str());
	
    ImGui::End();
}

void GameBoy::memory_map(uint16_t from, uint16_t to, uint8_t step)
{
    ImGui::Begin("Memory");

    ImVec2 size = ImGui::GetWindowSize();

    if (mmu.read(BOOTING)) {
        ImGui::Text("Address");
        ImGui::Text("%s", std::string(40, '-').c_str());
        
        std::string line = "";
        for (int i = from; i <= to; i++) {
            if (i == 0) {
                line += "0x0000 | ";
            }
            else if ((i - from) % step == 0) {
                ImGui::Text(line.c_str());
                line.clear();
                line += to_hex_string(i) + " | ";
            }

            line += to_hex_string(mmu.read(i)) + ' ';
        }
    }

    ImGui::End();
}

void GameBoy::display_viewport()
{
    ImGui::Begin("Viewport");

    ImVec2 pos = ImGui::GetCursorScreenPos();
    uint32_t tex = viewport.getTexture()->getNativeHandle();

    ImVec2 size = ImGui::GetWindowSize();
    size.y -= 46;

    ImGui::Image(tex, size, ImVec2(0, 0), ImVec2(0.625, 0.5625));
    ImGui::End();
}

void GameBoy::dockspace(std::function<void()> menu_func)
{
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace Demo", &bool_ptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    // DockSpace
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    menu_func();

    ImGui::End();
}

void GameBoy::log()
{
    logger.draw();
}

void GameBoy::menu_function()
{
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Rom", "  Loads rom file")) {
                file_dialog_opened = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (file_dialog_opened) {
        file.open_dialog();
        bool done = file.draw();

        file_dialog_opened = !done;
        if (file_dialog_opened == false) {
            if (!file.selected().empty()) {
                load_rom(file.selected()[0]);

                rom_loaded = true;
                if (mmu.read(BOOTING)) cpu.reset();
            }
        }
    }
}

void GameBoy::tick()
{
    uint32_t current_cycle = 0;

    if (rom_loaded) {

        while (current_cycle < cycles_per_frame) {
            uint32_t cycle = cpu.tick();
            current_cycle += cycle;

            cpu.update_timers(cycle);
            ppu.tick(cycle);

            cpu.handle_interupts();
        }
    }

    ppu.blit_pixels();
}
