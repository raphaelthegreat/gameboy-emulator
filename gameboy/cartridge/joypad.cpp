#include "joypad.h"
#include <gameboy.h>
#include <cpu/mmu.h>

void Joypad::init(MMU* _mmu)
{
	mmu = _mmu;
}

void Joypad::key_pressed(Key key)
{
	uint8_t view_x = mmu->read(SCROLL_X);
	uint8_t view_y = mmu->read(SCROLL_Y);

	int key_id = get_key(key);

	bool directional = false;
	if (key == Key_Up || key == Key_Down || key == Key_Left || key == Key_Right)	
		directional = true;

	uint8_t joypad = directional ? joypad_arrows : joypad_buttons;
	bool unpressed = CPU::get_bit(joypad, key_id);

	if (!unpressed)
		return;

	CPU::set_bit(joypad, key_id, 0);

	if (directional) joypad_arrows = joypad;
	else joypad_buttons = joypad;

	mmu->gb->cpu.interupt(JOYPAD_INTERUPT);
}

void Joypad::key_released(Key key)
{
	int key_id = get_key(key);
	
	bool directional = false;
	if (key == Key_Up || key == Key_Down || key == Key_Left || key == Key_Right)	
		directional = true;	

	uint8_t joypad = directional ? joypad_arrows : joypad_buttons;
	bool unpressed = CPU::get_bit(joypad, key_id);

	if (unpressed) return;

	CPU::set_bit(joypad, key_id, 1);

	if (directional) joypad_arrows = joypad;
	else joypad_buttons = joypad;
}

int Joypad::get_key(Key key)
{
	if (key == Key_Down || key == Key_Start)
		return 3;
	if (key == Key_Up || key == Key_Select)
		return 2;
	if (key == Key_Left || key == Key_B)
		return 1;
	if (key == Key_Right || key == Key_A)
		return 0;
}

uint8_t Joypad::read() const
{
	uint8_t joypad_state = mmu->memory[JOYPAD];
	
	switch (joypad_state) {
	case 0x10:
		return joypad_buttons;
	case 0x20:
		return joypad_arrows;
	default:
		return 0xCF;
	}
}
