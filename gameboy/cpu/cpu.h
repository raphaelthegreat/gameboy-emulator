#pragma once
#include <map>
#include <cstdint>
#include <string>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>
#include <functional>

std::string to_hex(uint16_t n);

#define BIND(x) std::bind(x, this)

#define TU8(x) static_cast<uint8_t>(x)
#define TU16(x) static_cast<uint16_t>(x)

#define C(t, x) static_cast<t>(x)

using std::map;

enum Flag : uint8_t {
	C = 4,
	H = 5,
	N = 6,
	Z = 7
};

struct Instruction {
	std::string name;
	std::function<int()> exec = nullptr;
	
	uint32_t cycles = 0;
};

struct Register {

    Register& operator = (const uint16_t& reg);
    Register& operator ++(int);
    Register& operator --(int);

    uint16_t get();

public:
    uint8_t l = 0x00, h = 0x00;
};

#define VBLANK_INTERUPT 0
#define LCD_INTERUPT 1
#define TIMER_INTERUPT 2
#define SERIAL_INTERUPT 3
#define JOYPAD_INTERUPT 4

#define DIV 0xFF04
#define TIMA 0xFF05
#define TMA 0xFF06
#define TAC 0xFF07

class MMU;
class CPU {
public:
	CPU();
	~CPU() = default;

	void register_opcodes();

	static void set_bit(uint8_t& num, int b, bool v);
    static bool get_bit(uint8_t& num, int b);
	static uint16_t combine(uint8_t low, uint8_t high);
    static uint8_t combine_nibbles(uint8_t low, uint8_t high);

    uint8_t get_high_byte(uint16_t& val);
    uint8_t get_low_byte(uint16_t& val);

	void set_flag(Flag f, bool val);
	bool get_flag(Flag f);

	/* Opcodes */
    int opcode00(); int opcode01(); int opcode02(); int opcode03(); int opcode04(); int opcode05(); int opcode06(); int opcode07(); int opcode08(); int opcode09(); int opcode0A(); int opcode0B(); int opcode0C(); int opcode0D(); int opcode0E(); int opcode0F();
    int opcode10(); int opcode11(); int opcode12(); int opcode13(); int opcode14(); int opcode15(); int opcode16(); int opcode17(); int opcode18(); int opcode19(); int opcode1A(); int opcode1B(); int opcode1C(); int opcode1D(); int opcode1E(); int opcode1F();
    int opcode20(); int opcode21(); int opcode22(); int opcode23(); int opcode24(); int opcode25(); int opcode26(); int opcode27(); int opcode28(); int opcode29(); int opcode2A(); int opcode2B(); int opcode2C(); int opcode2D(); int opcode2E(); int opcode2F();
    int opcode30(); int opcode31(); int opcode32(); int opcode33(); int opcode34(); int opcode35(); int opcode36(); int opcode37(); int opcode38(); int opcode39(); int opcode3A(); int opcode3B(); int opcode3C(); int opcode3D(); int opcode3E(); int opcode3F();
    int opcode40(); int opcode41(); int opcode42(); int opcode43(); int opcode44(); int opcode45(); int opcode46(); int opcode47(); int opcode48(); int opcode49(); int opcode4A(); int opcode4B(); int opcode4C(); int opcode4D(); int opcode4E(); int opcode4F();
    int opcode50(); int opcode51(); int opcode52(); int opcode53(); int opcode54(); int opcode55(); int opcode56(); int opcode57(); int opcode58(); int opcode59(); int opcode5A(); int opcode5B(); int opcode5C(); int opcode5D(); int opcode5E(); int opcode5F();
    int opcode60(); int opcode61(); int opcode62(); int opcode63(); int opcode64(); int opcode65(); int opcode66(); int opcode67(); int opcode68(); int opcode69(); int opcode6A(); int opcode6B(); int opcode6C(); int opcode6D(); int opcode6E(); int opcode6F();
    int opcode70(); int opcode71(); int opcode72(); int opcode73(); int opcode74(); int opcode75(); int opcode76(); int opcode77(); int opcode78(); int opcode79(); int opcode7A(); int opcode7B(); int opcode7C(); int opcode7D(); int opcode7E(); int opcode7F();
    int opcode80(); int opcode81(); int opcode82(); int opcode83(); int opcode84(); int opcode85(); int opcode86(); int opcode87(); int opcode88(); int opcode89(); int opcode8A(); int opcode8B(); int opcode8C(); int opcode8D(); int opcode8E(); int opcode8F();
    int opcode90(); int opcode91(); int opcode92(); int opcode93(); int opcode94(); int opcode95(); int opcode96(); int opcode97(); int opcode98(); int opcode99(); int opcode9A(); int opcode9B(); int opcode9C(); int opcode9D(); int opcode9E(); int opcode9F();
    int opcodeA0(); int opcodeA1(); int opcodeA2(); int opcodeA3(); int opcodeA4(); int opcodeA5(); int opcodeA6(); int opcodeA7(); int opcodeA8(); int opcodeA9(); int opcodeAA(); int opcodeAB(); int opcodeAC(); int opcodeAD(); int opcodeAE(); int opcodeAF();
    int opcodeB0(); int opcodeB1(); int opcodeB2(); int opcodeB3(); int opcodeB4(); int opcodeB5(); int opcodeB6(); int opcodeB7(); int opcodeB8(); int opcodeB9(); int opcodeBA(); int opcodeBB(); int opcodeBC(); int opcodeBD(); int opcodeBE(); int opcodeBF();
    int opcodeC0(); int opcodeC1(); int opcodeC2(); int opcodeC3(); int opcodeC4(); int opcodeC5(); int opcodeC6(); int opcodeC7(); int opcodeC8(); int opcodeC9(); int opcodeCA(); int opcodeCB(); int opcodeCC(); int opcodeCD(); int opcodeCE(); int opcodeCF();
    int opcodeD0(); int opcodeD1(); int opcodeD2(); int opcodeD3(); int opcodeD4(); int opcodeD5(); int opcodeD6(); int opcodeD7(); int opcodeD8(); int opcodeD9(); int opcodeDA(); int opcodeDB(); int opcodeDC(); int opcodeDD(); int opcodeDE(); int opcodeDF();
    int opcodeE0(); int opcodeE1(); int opcodeE2(); int opcodeE3(); int opcodeE4(); int opcodeE5(); int opcodeE6(); int opcodeE7(); int opcodeE8(); int opcodeE9(); int opcodeEA(); int opcodeEB(); int opcodeEC(); int opcodeED(); int opcodeEE(); int opcodeEF();
    int opcodeF0(); int opcodeF1(); int opcodeF2(); int opcodeF3(); int opcodeF4(); int opcodeF5(); int opcodeF6(); int opcodeF7(); int opcodeF8(); int opcodeF9(); int opcodeFA(); int opcodeFB(); int opcodeFC(); int opcodeFD(); int opcodeFE(); int opcodeFF();

    /* CB Opcodes */
    int opcodeCB00(); int opcodeCB01(); int opcodeCB02(); int opcodeCB03(); int opcodeCB04(); int opcodeCB05(); int opcodeCB06(); int opcodeCB07(); int opcodeCB08(); int opcodeCB09(); int opcodeCB0A(); int opcodeCB0B(); int opcodeCB0C(); int opcodeCB0D(); int opcodeCB0E(); int opcodeCB0F();
    int opcodeCB10(); int opcodeCB11(); int opcodeCB12(); int opcodeCB13(); int opcodeCB14(); int opcodeCB15(); int opcodeCB16(); int opcodeCB17(); int opcodeCB18(); int opcodeCB19(); int opcodeCB1A(); int opcodeCB1B(); int opcodeCB1C(); int opcodeCB1D(); int opcodeCB1E(); int opcodeCB1F();
    int opcodeCB20(); int opcodeCB21(); int opcodeCB22(); int opcodeCB23(); int opcodeCB24(); int opcodeCB25(); int opcodeCB26(); int opcodeCB27(); int opcodeCB28(); int opcodeCB29(); int opcodeCB2A(); int opcodeCB2B(); int opcodeCB2C(); int opcodeCB2D(); int opcodeCB2E(); int opcodeCB2F();
    int opcodeCB30(); int opcodeCB31(); int opcodeCB32(); int opcodeCB33(); int opcodeCB34(); int opcodeCB35(); int opcodeCB36(); int opcodeCB37(); int opcodeCB38(); int opcodeCB39(); int opcodeCB3A(); int opcodeCB3B(); int opcodeCB3C(); int opcodeCB3D(); int opcodeCB3E(); int opcodeCB3F();
    int opcodeCB40(); int opcodeCB41(); int opcodeCB42(); int opcodeCB43(); int opcodeCB44(); int opcodeCB45(); int opcodeCB46(); int opcodeCB47(); int opcodeCB48(); int opcodeCB49(); int opcodeCB4A(); int opcodeCB4B(); int opcodeCB4C(); int opcodeCB4D(); int opcodeCB4E(); int opcodeCB4F();
    int opcodeCB50(); int opcodeCB51(); int opcodeCB52(); int opcodeCB53(); int opcodeCB54(); int opcodeCB55(); int opcodeCB56(); int opcodeCB57(); int opcodeCB58(); int opcodeCB59(); int opcodeCB5A(); int opcodeCB5B(); int opcodeCB5C(); int opcodeCB5D(); int opcodeCB5E(); int opcodeCB5F();
    int opcodeCB60(); int opcodeCB61(); int opcodeCB62(); int opcodeCB63(); int opcodeCB64(); int opcodeCB65(); int opcodeCB66(); int opcodeCB67(); int opcodeCB68(); int opcodeCB69(); int opcodeCB6A(); int opcodeCB6B(); int opcodeCB6C(); int opcodeCB6D(); int opcodeCB6E(); int opcodeCB6F();
    int opcodeCB70(); int opcodeCB71(); int opcodeCB72(); int opcodeCB73(); int opcodeCB74(); int opcodeCB75(); int opcodeCB76(); int opcodeCB77(); int opcodeCB78(); int opcodeCB79(); int opcodeCB7A(); int opcodeCB7B(); int opcodeCB7C(); int opcodeCB7D(); int opcodeCB7E(); int opcodeCB7F();
    int opcodeCB80(); int opcodeCB81(); int opcodeCB82(); int opcodeCB83(); int opcodeCB84(); int opcodeCB85(); int opcodeCB86(); int opcodeCB87(); int opcodeCB88(); int opcodeCB89(); int opcodeCB8A(); int opcodeCB8B(); int opcodeCB8C(); int opcodeCB8D(); int opcodeCB8E(); int opcodeCB8F();
    int opcodeCB90(); int opcodeCB91(); int opcodeCB92(); int opcodeCB93(); int opcodeCB94(); int opcodeCB95(); int opcodeCB96(); int opcodeCB97(); int opcodeCB98(); int opcodeCB99(); int opcodeCB9A(); int opcodeCB9B(); int opcodeCB9C(); int opcodeCB9D(); int opcodeCB9E(); int opcodeCB9F();
    int opcodeCBA0(); int opcodeCBA1(); int opcodeCBA2(); int opcodeCBA3(); int opcodeCBA4(); int opcodeCBA5(); int opcodeCBA6(); int opcodeCBA7(); int opcodeCBA8(); int opcodeCBA9(); int opcodeCBAA(); int opcodeCBAB(); int opcodeCBAC(); int opcodeCBAD(); int opcodeCBAE(); int opcodeCBAF();
    int opcodeCBB0(); int opcodeCBB1(); int opcodeCBB2(); int opcodeCBB3(); int opcodeCBB4(); int opcodeCBB5(); int opcodeCBB6(); int opcodeCBB7(); int opcodeCBB8(); int opcodeCBB9(); int opcodeCBBA(); int opcodeCBBB(); int opcodeCBBC(); int opcodeCBBD(); int opcodeCBBE(); int opcodeCBBF();
    int opcodeCBC0(); int opcodeCBC1(); int opcodeCBC2(); int opcodeCBC3(); int opcodeCBC4(); int opcodeCBC5(); int opcodeCBC6(); int opcodeCBC7(); int opcodeCBC8(); int opcodeCBC9(); int opcodeCBCA(); int opcodeCBCB(); int opcodeCBCC(); int opcodeCBCD(); int opcodeCBCE(); int opcodeCBCF();
    int opcodeCBD0(); int opcodeCBD1(); int opcodeCBD2(); int opcodeCBD3(); int opcodeCBD4(); int opcodeCBD5(); int opcodeCBD6(); int opcodeCBD7(); int opcodeCBD8(); int opcodeCBD9(); int opcodeCBDA(); int opcodeCBDB(); int opcodeCBDC(); int opcodeCBDD(); int opcodeCBDE(); int opcodeCBDF();
    int opcodeCBE0(); int opcodeCBE1(); int opcodeCBE2(); int opcodeCBE3(); int opcodeCBE4(); int opcodeCBE5(); int opcodeCBE6(); int opcodeCBE7(); int opcodeCBE8(); int opcodeCBE9(); int opcodeCBEA(); int opcodeCBEB(); int opcodeCBEC(); int opcodeCBED(); int opcodeCBEE(); int opcodeCBEF();
    int opcodeCBF0(); int opcodeCBF1(); int opcodeCBF2(); int opcodeCBF3(); int opcodeCBF4(); int opcodeCBF5(); int opcodeCBF6(); int opcodeCBF7(); int opcodeCBF8(); int opcodeCBF9(); int opcodeCBFA(); int opcodeCBFB(); int opcodeCBFC(); int opcodeCBFD(); int opcodeCBFE(); int opcodeCBFF();

	void reset();
	uint32_t tick();
    void update_timers(uint32_t cycle);

    void interupt(uint32_t id);
    void handle_interupts();

public:
	map<uint16_t, Instruction> lookup;
	MMU* mmu;

public:
	uint16_t opcode;

	Register af, bc, de, hl;
	uint16_t pc, sp;

	uint32_t cycles;
    uint32_t frequency;
    uint32_t divider_counter;
    uint32_t timer_counter;

    std::ofstream out;

    bool halted = false;
    bool interupts_enabled = true;
    bool gate = true;
};