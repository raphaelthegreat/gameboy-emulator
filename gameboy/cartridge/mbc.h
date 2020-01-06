#pragma once
#include <cstdint>

// Base class
class Cartridge;
class MBC {
public:
	MBC(Cartridge* cart) : cartridge(cart) {}

	virtual void handle_banking(uint16_t addr, uint8_t data) = 0;

protected:
	Cartridge* cartridge;
};

class MBCNone : public MBC {
public:
	MBCNone(Cartridge* cart) : MBC(cart) {}

	void handle_banking(uint16_t addr, uint8_t data) override;
};

class MBC1 : public MBC {
public:
	MBC1(Cartridge* cart) : MBC(cart) {}

	void handle_banking(uint16_t addr, uint8_t data) override;
};