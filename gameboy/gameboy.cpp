#include "gameboy.h"
#include <sstream>
#include <thread>
#include <iomanip>

const char* title() { return "File Dialog"; }

GameBoy::GameBoy() : cpu(&mmu), file(title)
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
    mmu.cartridge = std::make_shared<Cartridge>();
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
	ImGui::Begin("CPU-Info");
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
	ImGui::End();
}

void GameBoy::memory_map(uint16_t from, uint16_t to, uint8_t step)
{
    ImGui::Begin("Memory");

    if (mmu.read(BOOTING)) {
        std::string line = "";
        line += "0x0000 | ";
        for (int i = from; i <= to; i++) {
            if ((i - from) % step == 0) {
                ImGui::Text(line.c_str());
                line.clear();
                line += "0x" + to_hex_string(i) + " | ";
            }

            line += "0x" + to_hex_string(mmu.read(i)) + ' ';
        }
    }

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

void GameBoy::menu_function()
{
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Rom", "")) {
                file_dialog_opened = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (file_dialog_opened) {
        file.openDialog();
        bool done = file.draw();

        file_dialog_opened = !done;
        if (file_dialog_opened == false) {
            load_rom(file.selected()[0]);
            
            rom_loaded = true;
            if (mmu.read(BOOTING)) cpu.reset();
        }
    }
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

void GameBoy::display_viewport()
{
    ImGui::Begin("Viewport");

    ImVec2 pos = ImGui::GetCursorScreenPos();
    uint32_t tex = viewport.getTexture()->getNativeHandle();

    ImVec2 size = ImGui::GetWindowSize();
    size.y -= 34.5;
    
    ImGui::Image(tex, size, ImVec2(0, 0), ImVec2(0.625, 0.5625));
    ImGui::End();
}
