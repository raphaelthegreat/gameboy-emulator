#pragma once
#include <cstdint>
#include <SFML/Window/Keyboard.hpp>

#define JOYPAD 0xFF00

enum Key {
	Key_A,
	Key_B,
	Key_Select,
	Key_Start,
	Key_Up,
	Key_Down,
	Key_Left,
	Key_Right
};

class MMU;
class Joypad {
public:
	Joypad() = default;
	~Joypad() = default;

	void init(MMU* _mmu);

	void key_pressed(Key key);
	void key_released(Key key);

	int get_key(Key key);
	uint8_t read() const;

protected:
	MMU* mmu;

	uint8_t joypad_arrows = 0xF;
	uint8_t joypad_buttons = 0xF;
};