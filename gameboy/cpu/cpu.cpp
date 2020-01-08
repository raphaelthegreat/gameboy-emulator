#include "cpu.h"
#include <SFML/Window/Keyboard.hpp>
#include <cpu/mmu.h>

#pragma warning(disable : 26812)
#pragma warning(disable : 26495)

std::string to_hex(uint16_t n, int d)
{
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(d) << std::hex << (n | 0);

    return "0x" + ss.str();
}

std::string to_hex_string(uint16_t num)
{
    static const char digits[513] =
        "000102030405060708090A0B0C0D0E0F"
        "101112131415161718191A1B1C1D1E1F"
        "202122232425262728292A2B2C2D2E2F"
        "303132333435363738393A3B3C3D3E3F"
        "404142434445464748494A4B4C4D4E4F"
        "505152535455565758595A5B5C5D5E5F"
        "606162636465666768696A6B6C6D6E6F"
        "707172737475767778797A7B7C7D7E7F"
        "808182838485868788898A8B8C8D8E8F"
        "909192939495969798999A9B9C9D9E9F"
        "A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"
        "B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"
        "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF"
        "D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"
        "E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF"
        "F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF";

    uint32_t x = (uint32_t)num;
    int i = 3;
    char* lut = (char*)digits;

    char buff[8];

    while (i >= 0) {
        int pos = (x & 0xFF) * 2;
        char ch = lut[pos];
        buff[i * 2] = ch;

        ch = lut[pos + 1];
        buff[i * 2 + 1] = ch;

        x >>= 8;
        i -= 1;
    }

    std::string s = std::string(4, ' ');

    s[0] = buff[4];
    s[1] = buff[5];
    s[2] = buff[6];
    s[3] = buff[7];

    return s;
}

Register& Register::operator=(const uint16_t& reg)
{
    l = TU8(reg & 0x00FF);
    h = TU8((reg >> 8) & 0x00FF);

    return *this;
}

inline Register& Register::operator++(int)
{
    uint16_t full = get();
    full++;

    l = full & 0x00FF;
    h = (full >> 8) & 0x00FF;

    return *this;
}

inline Register& Register::operator--(int)
{
    uint16_t full = get();
    full--;

    l = full & 0x00FF;
    h = (full >> 8) & 0x00FF;

    return *this;
}

inline uint16_t Register::get()
{
    uint16_t high = TU16(h);
    uint16_t result = (high << 8) | l;
    return result;
}

CPU::CPU(MMU* _mmu) : mmu(_mmu),
   cpu_timer(_mmu)
{
    std::cout.sync_with_stdio(false);

    this->register_opcodes();
    //out.open("log.txt", std::ofstream::out | std::ofstream::app);
}

void CPU::set_bit(uint8_t& num, int b, bool v)
{
    if (v) num |= (v << b);
    else  num &= ~(1 << b);
}

bool CPU::get_bit(uint8_t& num, int b)
{
    return (num & (1 << b));
}

uint16_t CPU::combine(uint8_t low, uint8_t high)
{
    return (high << 8) | low;
}

uint8_t CPU::combine_nibbles(uint8_t low, uint8_t high)
{
    uint8_t result = (high << 4) | low;
    return result;
}

uint8_t CPU::get_high_byte(uint16_t& val)
{
    uint8_t byte = TU8((val & 0xFF00) >> 8);
    return byte;
}

uint8_t CPU::get_low_byte(uint16_t& val)
{
    uint8_t byte = TU8(val & 0x00FF);
    return byte;
}

void CPU::set_flag(Flag f, bool val)
{
    uint8_t flags = af.l;
    set_bit(flags, f, val);
    af.l = flags;
}

bool CPU::get_flag(Flag f)
{
    uint8_t flags = af.l;
    return (flags & (1 << f));
}

void CPU::reset()
{
    af = 0x0000;
    bc = 0x0000;
    de = 0x0000;
    hl = 0x0000;
    pc = 0x0100;
    sp = 0xFFFE;

    opcode = 0x0000;
    cycles = 0;
}

uint32_t CPU::tick()
{    
    /*if (sf::Keyboard::isKeyPressed(sf::Keyboard::L)) {
        out << to_hex(pc) << " | " << get_flag(Z) << ' ' << get_flag(N);
        out << ' ' << get_flag(H) << ' ' << get_flag(C) << " AF: ";
        out << to_hex(af.get()) << " BC: " << to_hex(bc.get());
        out << " DE: " << to_hex(de.get()) << " HL: " << to_hex(hl.get()) << ' ' << (int)mmu->read(LY) <<'\n';
    }*/

    if (halted) return 1;
    if (pc == 0x00FA) pc = 0x00FC; // Bypass nintendo check

    opcode = mmu->read(pc++);  // Fetch

    if (opcode == 0xCB) {  // Handle 0xCB prefix
        opcode = combine(mmu->read(pc++), 0xCB);
    }        

    Instruction instr = lookup[opcode]; // Decode

    cycles = instr.cycles; // Cycles required
    cycles += instr.exec(); // Execute (may require extra cycles for instructions like jp with condition)    

    return cycles;
}

void CPU::update_timers(uint32_t cycle)
{
    cpu_timer.update(cycle);
}

void CPU::interupt(uint32_t id)
{
    uint8_t req = mmu->read(INTERUPT_FLAG); // Get the interupts flag
    set_bit(req, id, 1); // Enable the appropriate interupt
    mmu->write(INTERUPT_FLAG, req); // Write it to memory

    halted = false;
}

void CPU::handle_interupts()
{   
    if (interupts_enabled) { // Is IME enabled?
        uint8_t req = mmu->read(INTERUPT_FLAG);
        uint8_t enabled = mmu->read(INTERUPT_ENABLE);

        if (req > 0) { // If there are any pending interupts            
            for (int i = 0; i < 5; i++) { // Handle them in the order of priority
                if (get_bit(req, i) && get_bit(enabled, i)) { // If the interupt is enabled and is requested
                    interupts_enabled = false; // Clear IME flag

                    set_bit(req, i, 0); 
                    mmu->write(INTERUPT_FLAG, req);
                    
                    // Push PC to stack
                    sp--; mmu->write(sp, get_high_byte(pc));
                    sp--; mmu->write(sp, get_low_byte(pc));
                    
                    // Set PC to the appropriate interupt location (hardcoded)
                    if (i == 0) pc = 0x40;
                    else if (i == 1) pc = 0x48;
                    else if (i == 2) pc = 0x50;
                    else if (i == 3) pc = 0x58;
                    else if (i == 4) pc = 0x60;

                    out << "Interput!: " << to_hex(pc) << '\n';
                }
            }
        }
    }
}

// NOP
int CPU::opcode00()
{
    return 0;
}

int CPU::opcode01()
{
    uint8_t low = mmu->read(pc++);
    uint8_t high = mmu->read(pc++);

    bc = combine(low, high);
    return 0;
}

int CPU::opcode02()
{
    mmu->write(bc.get(), af.h);
    return 0;
}

int CPU::opcode03()
{
    bc++;
    return 0;
}

int CPU::opcode04()
{
    bc.h++;
    bool half_carry = (bc.h & 0x0F) == 0x00;

    set_flag(Z, bc.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode05()
{
    bc.h--;
    bool half_carry = (bc.h & 0x0F) == 0x0F;
    
    set_flag(Z, bc.h == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode06()
{
    uint8_t data = mmu->read(pc++);
    bc.h = data;

    return 0;
}

int CPU::opcode07()
{
    set_flag(C, get_bit(af.h, 7));
    
    af.h <<= 1;
    set_bit(af.h, 0, get_flag(C));

    set_flag(Z, 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcode08()
{
    uint8_t low = mmu->read(pc++);
    uint8_t high = mmu->read(pc++);

    uint16_t addr = combine(low, high);
    mmu->write(addr, get_low_byte(sp));
    mmu->write(addr + 1, get_high_byte(sp));

    return 0;
}

int CPU::opcode09()
{
    uint32_t result = hl.get() + bc.get();
    bool half_carry = (hl.get() & 0xfff) + (bc.get() & 0xfff) > 0xfff;

    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x10000) != 0);

    hl = TU16(result);
    return 0;
}

int CPU::opcode0A()
{
    uint8_t data = mmu->read(bc.get());
    af.h = data;
    
    return 0;
}

int CPU::opcode0B()
{
    bc--;
    return 0;
}

int CPU::opcode0C()
{
    bc.l++;
    bool half_carry = (bc.l & 0x0F) == 0x00;
    
    set_flag(Z, bc.l == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode0D()
{
    bc.l--;
    bool half_carry = (bc.l & 0x0F) == 0x0F;

    set_flag(Z, bc.l == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode0E()
{
    uint8_t data = mmu->read(pc++);
    bc.l = data;
    
    return 0;
}

int CPU::opcode0F()
{
    bool carry = get_bit(af.h, 0);
    af.h >>= 1;

    set_flag(C, carry);
    set_bit(af.h, 7, carry);

    set_flag(Z, 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcode10()
{
    halted = true;
    return 0;
}

int CPU::opcode11()
{
    uint8_t low = mmu->read(pc++);
    uint8_t high = mmu->read(pc++);

    de = combine(low, high);
    return 0;
}

int CPU::opcode12()
{
    mmu->write(de.get(), af.h);
    return 0;
}

int CPU::opcode13()
{
    de++;
    return 0;
}

int CPU::opcode14()
{
    de.h++;
    bool half_carry = (de.h & 0x0F) == 0x00;

    set_flag(Z, de.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode15()
{
    de.h--;
    bool half_carry = (de.h & 0x0F) == 0x0F;

    set_flag(Z, de.h == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode16()
{
    uint8_t data = mmu->read(pc++);
    
    de.h = data;
    return 0;
}

int CPU::opcode17()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(af.h, 7));

    af.h <<= 1;
    set_bit(af.h, 0, carry);

    set_flag(N, 0);
    set_flag(Z, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcode18()
{
    int8_t offset = T8(mmu->read(pc++));
    pc += offset;

    return 0;
}

int CPU::opcode19()
{
    uint32_t result = hl.get() + de.get();
    bool half_carry = (hl.get() & 0xfff) + (de.get() & 0xfff) > 0xfff;

    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x10000) != 0);

    hl = TU16(result);
    return 0;
}

int CPU::opcode1A()
{
    uint8_t data = mmu->read(de.get());
    af.h = data;
    
    return 0;
}

int CPU::opcode1B()
{
    de--;
    return 0;
}

int CPU::opcode1C()
{
    de.l++;
    bool half_carry = (de.l & 0x0F) == 0x00;

    set_flag(Z, de.l == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode1D()
{
    de.l--;
    bool half_carry = (de.l & 0x0F) == 0x0F;

    set_flag(Z, de.l == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode1E()
{
    uint8_t data = mmu->read(pc++);
    de.l = data;

    return 0;
}

int CPU::opcode1F()
{
    bool carry = get_flag(C);
    set_flag(C, get_bit(af.h, 0));

    af.h >>= 1;
    set_bit(af.h, 7, carry);

    set_flag(Z, 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcode20()
{
    if (!get_flag(Z)) {
        int8_t offset = T8(mmu->read(pc++));
        pc += offset;
        
        return 1;
    }
    
    pc++; return 0;
}

int CPU::opcode21()
{
    uint8_t low = mmu->read(pc++);
    uint8_t high = mmu->read(pc++);

    hl = combine(low, high);
    return 0;
}

int CPU::opcode22()
{
    mmu->write(hl.get(), af.h); hl++;
    return 0;
}

int CPU::opcode23()
{
    hl++;
    return 0;
}

int CPU::opcode24()
{
    uint8_t h = hl.h;

    uint8_t result = h + 1;
    bool half_carry = ((h ^ 1 ^ result) & 0x10) == 0x10;

    set_flag(Z, (result == 0));
    set_flag(N, 0);
    set_flag(H, half_carry);

    hl.h = result;
    return 0;
}

int CPU::opcode25()
{
    hl.h--;
    bool half_carry = (hl.h & 0x0F) == 0x0F;

    set_flag(Z, hl.h == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode26()
{
    uint8_t data = mmu->read(pc++);

    hl.h = data;
    return 0;
}

int CPU::opcode27()
{
    int a = af.h;
    if (!get_flag(N)) {
        if (get_flag(H) || (a & 0xF) > 9)       
            a += 0x06;        

        if (get_flag(C) || (a > 0x9F))        
            a += 0x60;        
    }
    else {
        if (get_flag(H))       
            a = (a - 0x06) & 0xFF;        

        if (get_flag(C))        
            a -= 0x60;       
    }

    set_flag(H, 0);
    if ((a & 0x100) == 0x100)   
        set_flag(C, 1);
   
    a &= 0xFF;
    set_flag(Z, a == 0x00);
    af.h = a;

    return 0;
}

int CPU::opcode28()
{
    if (get_flag(Z)) {
        int8_t r8 = T8(mmu->read(pc++));
        pc += r8;
        return 1;
    }
    
    pc++; return 0;
}

int CPU::opcode29()
{
    uint32_t result = hl.get() + hl.get();
    bool half_carry = (hl.get() & 0xfff) + (hl.get() & 0xfff) > 0xfff;

    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x10000) != 0);

    hl = TU16(result);
    return 0;
}

int CPU::opcode2A()
{
    uint8_t data = mmu->read(hl.get()); hl++;

    af.h = data;    
    return 0;
}

int CPU::opcode2B()
{
    hl--;
    return 0;
}

int CPU::opcode2C()
{
    hl.l++;
    bool half_carry = (hl.l & 0x0F) == 0x00;

    set_flag(Z, hl.l == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode2D()
{
    hl.l--;
    bool half_carry = (hl.l & 0x0F) == 0x0F;

    set_flag(Z, hl.l == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode2E()
{
    uint8_t data = mmu->read(pc++);
    hl.l = data;

    return 0;
}

int CPU::opcode2F()
{
    af.h = ~af.h;

    set_flag(N, 1);
    set_flag(H, 1);
    
    return 0;
}

int CPU::opcode30()
{
    if (!get_flag(C)) {
        int8_t offset = T8(mmu->read(pc++));
        pc += offset;

        return 1;
    }

    pc++; return 0;
}

int CPU::opcode31()
{
    uint8_t low = mmu->read(pc++);
    uint8_t high = mmu->read(pc++);

    sp = combine(low, high);
    return 0;
}

int CPU::opcode32()
{
    mmu->write(hl.get(), af.h); hl--;
    return 0;
}

int CPU::opcode33()
{
    sp++;
    return 0;
}

int CPU::opcode34()
{
    uint8_t value = mmu->read(hl.get());
    uint8_t result = TU8(value + 1);
    
    mmu->write(hl.get(), result);

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, (result & 0x0F) == 0x00);
    return 0;
}

int CPU::opcode35()
{
    uint8_t value = mmu->read(hl.get());
    uint8_t result = TU8(value - 1);
    
    mmu->write(hl.get(), result);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, (result & 0x0F) == 0x0F);

    return 0;
}

int CPU::opcode36()
{
    uint8_t data = mmu->read(pc++);
    
    mmu->write(hl.get(), data);
    return 0;
}

int CPU::opcode37()
{
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 1);

    return 0;
}

int CPU::opcode38()
{
    if (get_flag(C)) {
        int8_t r8 = T8(mmu->read(pc++));
        pc += r8;

        return 1;
    }

    pc++; return 0;
}

int CPU::opcode39()
{
    uint32_t result = hl.get() + sp;
    bool half_carry = (hl.get() & 0xFFF) + (sp & 0xFFF) > 0xFFF;

    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x10000) != 0);

    hl = TU16(result);
    return 0;
}

int CPU::opcode3A()
{
    uint8_t data = mmu->read(hl.get()); hl--;
    af.h = data;
    
    return 0;
}

int CPU::opcode3B()
{
    sp--;
    return 0;
}

int CPU::opcode3C()
{
    af.h++;
    bool half_carry = (af.h & 0x0F) == 0x00;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode3D()
{
    af.h--;
    bool half_carry = (af.h & 0x0F) == 0x0F;

    set_flag(Z, af.h == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);

    return 0;
}

int CPU::opcode3E()
{
    uint8_t data = mmu->read(pc++);
    af.h = data;

    return 0;
}

int CPU::opcode3F()
{
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, !get_flag(C));
    
    return 0;
}

int CPU::opcode40()
{
    return 0;
}

int CPU::opcode41()
{
    bc.h = bc.l;
    return 0;
}

int CPU::opcode42()
{
    bc.h = de.h;
    return 0;
}

int CPU::opcode43()
{
    bc.h = de.l;
    return 0;
}

int CPU::opcode44()
{
    bc.h = hl.h;
    return 0;
}

int CPU::opcode45()
{
    bc.h = hl.l;
    return 0;
}

int CPU::opcode46()
{
    bc.h = mmu->read(hl.get());
    return 0;
}

int CPU::opcode47()
{
    bc.h = af.h;
    return 0;
}

int CPU::opcode48()
{
    bc.l = bc.h;
    return 0;
}

int CPU::opcode49()
{
    return 0;
}

int CPU::opcode4A()
{
    bc.l = de.h;
    return 0;
}

int CPU::opcode4B()
{
    bc.l = de.l;
    return 0;
}

int CPU::opcode4C()
{
    bc.l = hl.h;
    return 0;
}

int CPU::opcode4D()
{
    bc.l = hl.l;
    return 0;
}

int CPU::opcode4E()
{
    bc.l = mmu->read(hl.get());
    return 0;
}

int CPU::opcode4F()
{
    bc.l = af.h;
    return 0;
}

int CPU::opcode50()
{
    de.h = bc.h;
    return 0;
}

int CPU::opcode51()
{
    de.h = bc.l;
    return 0;
}

int CPU::opcode52()
{
    return 0;
}

int CPU::opcode53()
{
    de.h = de.l;
    return 0;
}

int CPU::opcode54()
{
    de.h = hl.h;
    return 0;
}

int CPU::opcode55()
{
    de.h = hl.l;
    return 0;
}

int CPU::opcode56()
{
    de.h = mmu->read(hl.get());
    return 0;
}

int CPU::opcode57()
{
    de.h = af.h;
    return 0;
}

int CPU::opcode58()
{
    de.l = bc.h;
    return 0;
}

int CPU::opcode59()
{
    de.l = bc.l;
    return 0;
}

int CPU::opcode5A()
{
    de.l = de.h;
    return 0;
}

int CPU::opcode5B()
{
    return 0;
}

int CPU::opcode5C()
{
    de.l = hl.h;
    return 0;
}

int CPU::opcode5D()
{
    de.l = hl.l;
    return 0;
}

int CPU::opcode5E()
{
    de.l = mmu->read(hl.get());
    return 0;
}

int CPU::opcode5F()
{
    de.l = af.h;
    return 0;
}

int CPU::opcode60()
{
    hl.h = bc.h;
    return 0;
}

int CPU::opcode61()
{
    hl.h = bc.l;
    return 0;
}

int CPU::opcode62()
{
    hl.h = de.h;
    return 0;
}

int CPU::opcode63()
{
    hl.h = de.l;
    return 0;
}

int CPU::opcode64()
{
    return 0;
}

int CPU::opcode65()
{
    hl.h = hl.l;
    return 0;
}

int CPU::opcode66()
{
    hl.h = mmu->read(hl.get());
    return 0;
}

int CPU::opcode67()
{
    hl.h = af.h;
    return 0;
}

int CPU::opcode68()
{
    hl.l = bc.h;
    return 0;
}

int CPU::opcode69()
{
    hl.l = bc.l;
    return 0;
}

int CPU::opcode6A()
{
    hl.l = de.h;
    return 0;
}

int CPU::opcode6B()
{
    hl.l = de.l;
    return 0;
}

int CPU::opcode6C()
{
    hl.l = hl.h;
    return 0;
}

int CPU::opcode6D()
{
    return 0;
}

int CPU::opcode6E()
{
    hl.l = mmu->read(hl.get());
    return 0;
}

int CPU::opcode6F()
{
    hl.l = af.h;
    return 0;
}

int CPU::opcode70()
{
    mmu->write(hl.get(), bc.h);
    return 0;
}

int CPU::opcode71()
{
    mmu->write(hl.get(), bc.l);
    return 0;
}

int CPU::opcode72()
{
    mmu->write(hl.get(), de.h);
    return 0;
}

int CPU::opcode73()
{
    mmu->write(hl.get(), de.l);
    return 0;
}

int CPU::opcode74()
{
    mmu->write(hl.get(), hl.h);
    return 0;
}

int CPU::opcode75()
{
    mmu->write(hl.get(), hl.l);
    return 0;
}

int CPU::opcode76()
{
    halted = true;
    return 0;
}

int CPU::opcode77()
{
    mmu->write(hl.get(), af.h);
    return 0;
}

int CPU::opcode78()
{
    af.h = bc.h;
    return 0;
}

int CPU::opcode79()
{
    af.h = bc.l;
    return 0;
}

int CPU::opcode7A()
{
    af.h = de.h;
    return 0;
}

int CPU::opcode7B()
{
    af.h = de.l;
    return 0;
}

int CPU::opcode7C()
{
    af.h = hl.h;
    return 0;
}

int CPU::opcode7D()
{
    af.h = hl.l;
    return 0;
}

int CPU::opcode7E()
{
    af.h = mmu->read(hl.get());
    return 0;
}

int CPU::opcode7F()
{
    return 0;
}

int CPU::opcode80()
{
    uint32_t result = af.h + bc.h;
    bool half_carry = (af.h & 0xf) + (bc.h & 0xf) > 0xf;
    af.h = TU8(result);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x100) != 0);
    
    return 0;
}

int CPU::opcode81()
{
    uint32_t result = af.h + bc.l;
    bool half_carry = (af.h & 0xf) + (bc.l & 0xf) > 0xf;
    af.h = TU8(result);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x100) != 0);

    return 0;
}

int CPU::opcode82()
{
    uint32_t result = af.h + de.h;
    bool half_carry = (af.h & 0xf) + (de.h & 0xf) > 0xf;
    af.h = TU8(result);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x100) != 0);

    return 0;
}

int CPU::opcode83()
{
    uint32_t result = af.h + de.l;
    bool half_carry = (af.h & 0xf) + (de.l & 0xf) > 0xf;
    af.h = TU8(result);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x100) != 0);

    return 0;
}

int CPU::opcode84()
{
    uint32_t result = af.h + hl.h;
    bool half_carry = (af.h & 0xf) + (hl.h & 0xf) > 0xf;
    af.h = TU8(result);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x100) != 0);

    return 0;
}

int CPU::opcode85()
{
    uint32_t result = af.h + hl.l;
    bool half_carry = (af.h & 0xf) + (hl.l & 0xf) > 0xf;
    af.h = TU8(result);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x100) != 0);

    return 0;
}

int CPU::opcode86()
{
    uint8_t value = mmu->read(hl.get());
    uint32_t result = af.h + value;
    bool half_carry = (af.h & 0xf) + (value & 0xf) > 0xf;
    af.h = TU8(result);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x100) != 0);

    return 0;
}

int CPU::opcode87()
{
    uint32_t result = af.h + af.h;
    bool half_carry = (af.h & 0xf) + (af.h & 0xf) > 0xf;
    af.h = TU8(result);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x100) != 0);

    return 0;
}

int CPU::opcode88()
{
    uint8_t carry = get_flag(C);
    uint32_t result32 = af.h + bc.h + carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) + (bc.h & 0xf) + carry) > 0xf;

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, result32 > 0xff);

    af.h = result;
    return 0;
}

int CPU::opcode89()
{
    uint8_t carry = get_flag(C);
    uint32_t result32 = af.h + bc.l + carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) + (bc.l & 0xf) + carry) > 0xf;

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, result32 > 0xff);

    af.h = result;
    return 0;
}

int CPU::opcode8A()
{
    uint8_t carry = get_flag(C);
    uint32_t result32 = af.h + de.h + carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) + (de.h & 0xf) + carry) > 0xf;

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, result32 > 0xff);

    af.h = result;
    return 0;
}

int CPU::opcode8B()
{
    uint8_t carry = get_flag(C);
    uint32_t result32 = af.h + de.l + carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) + (de.l & 0xf) + carry) > 0xf;

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, result32 > 0xff);

    af.h = result;
    return 0;
}

int CPU::opcode8C()
{
    uint8_t carry = get_flag(C);
    uint32_t result32 = af.h + hl.h + carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) + (hl.h & 0xf) + carry) > 0xf;

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, result32 > 0xff);

    af.h = result;
    return 0;
}

int CPU::opcode8D()
{
    uint8_t carry = get_flag(C);
    uint32_t result32 = af.h + hl.l + carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) + (hl.l & 0xf) + carry) > 0xf;

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, result32 > 0xff);

    af.h = result;
    return 0;
}

int CPU::opcode8E()
{
    uint8_t value = mmu->read(hl.get());
    uint8_t carry = get_flag(C);
    uint32_t result32 = af.h + value + carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) + (value & 0xf) + carry) > 0xf;

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, result32 > 0xff);

    af.h = result;
    return 0;
}

int CPU::opcode8F()
{
    uint8_t carry = get_flag(C);
    uint32_t result32 = af.h + af.h + carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) + (af.h & 0xf) + carry) > 0xf;

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, result32 > 0xff);

    af.h = result;
    return 0;
}

int CPU::opcode90()
{
    uint16_t result = af.h - bc.h;
    bool half_carry = (af.h & 0xF) < (bc.h & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, bc.h > af.h);

    af.h = TU8(result);
    return 0;
}

int CPU::opcode91()
{
    uint16_t result = af.h - bc.l;
    bool half_carry = (af.h & 0xF) < (bc.l & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, bc.l > af.h);

    af.h = TU8(result);
    return 0;
}

int CPU::opcode92()
{
    uint16_t result = af.h - de.h;
    bool half_carry = (af.h & 0xF) < (de.h & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, de.h > af.h);

    af.h = TU8(result);
    return 0;
}

int CPU::opcode93()
{
    uint16_t result = af.h - de.l;
    bool half_carry = (af.h & 0xF) < (de.l & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, de.l > af.h);

    af.h = TU8(result);
    return 0;
}

int CPU::opcode94()
{
    uint16_t result = af.h - hl.h;
    bool half_carry = (af.h & 0xF) < (hl.h & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, hl.h > af.h);

    af.h = TU8(result);
    return 0;
}

int CPU::opcode95()
{
    uint16_t result = af.h - hl.l;
    bool half_carry = (af.h & 0xF) < (hl.l & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, hl.l > af.h);

    af.h = TU8(result);
    return 0;
}

int CPU::opcode96()
{
    uint8_t value = mmu->read(hl.get());
    uint16_t result = af.h - value;
    bool half_carry = (af.h & 0xF) < (value & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, value > af.h);

    af.h = TU8(result);
    return 0;
}

int CPU::opcode97()
{
    af.h = 0;
    
    set_flag(Z, 1);
    set_flag(N, 1);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcode98()
{
    uint8_t carry = get_flag(C);
    int32_t result32 = af.h - bc.h - carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) - (bc.h & 0xf) - carry) < 0;

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(C, result32 < 0);
    set_flag(H, half_carry);

    af.h = result;
    return 0;
}

int CPU::opcode99()
{
    uint8_t carry = get_flag(C);
    int32_t result32 = af.h - bc.l - carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) - (bc.l & 0xf) - carry) < 0;

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(C, result32 < 0);
    set_flag(H, half_carry);

    af.h = result;
    return 0;
}

int CPU::opcode9A()
{
    uint8_t carry = get_flag(C);
    int32_t result32 = af.h - de.h - carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) - (de.h & 0xf) - carry) < 0;

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(C, result32 < 0);
    set_flag(H, half_carry);

    af.h = result;
    return 0;
}

int CPU::opcode9B()
{
    uint8_t carry = get_flag(C);
    int32_t result32 = af.h - de.l - carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) - (de.l & 0xf) - carry) < 0;

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(C, result32 < 0);
    set_flag(H, half_carry);

    af.h = result;
    return 0;
}

int CPU::opcode9C()
{
    uint8_t carry = get_flag(C);
    int32_t result32 = af.h - hl.h - carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) - (hl.h & 0xf) - carry) < 0;

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(C, result32 < 0);
    set_flag(H, half_carry);

    af.h = result;
    return 0;
}

int CPU::opcode9D()
{
    uint8_t carry = get_flag(C);
    int32_t result32 = af.h - hl.l - carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) - (hl.l & 0xf) - carry) < 0;

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(C, result32 < 0);
    set_flag(H, half_carry);

    af.h = result;
    return 0;
}

int CPU::opcode9E()
{
    uint8_t carry = get_flag(C);
    uint8_t value = mmu->read(hl.get());
    int32_t result32 = af.h - value - carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) - (value & 0xf) - carry) < 0;

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(C, result32 < 0);
    set_flag(H, half_carry);

    af.h = result;
    return 0;
}

int CPU::opcode9F()
{
    uint8_t carry = get_flag(C);
    int32_t result32 = af.h - af.h - carry;
    uint8_t result = TU8(result32);
    bool half_carry = ((af.h & 0xf) - (af.h & 0xf) - carry) < 0;

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(C, result32 < 0);
    set_flag(H, half_carry);

    af.h = result;
    return 0;
}

int CPU::opcodeA0()
{
    af.h &= bc.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 1);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeA1()
{
    af.h &= bc.l;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 1);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeA2()
{
    af.h &= de.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 1);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeA3()
{
    af.h &= de.l;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 1);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeA4()
{
    af.h &= hl.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 1);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeA5()
{
    af.h &= hl.l;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 1);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeA6()
{
    af.h &= mmu->read(hl.get());

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 1);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeA7()
{
    af.h &= af.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 1);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeA8()
{
    af.h ^= bc.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeA9()
{
    af.h ^= bc.l;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeAA()
{
    af.h ^= de.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeAB()
{
    af.h ^= de.l;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeAC()
{
    af.h ^= hl.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeAD()
{
    af.h ^= hl.l;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeAE()
{
    af.h ^= mmu->read(hl.get());

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeAF()
{
    af.h = 0;

    set_flag(Z, 1);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeB0()
{
    af.h |= bc.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeB1()
{
    af.h |= bc.l;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeB2()
{
    af.h |= de.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeB3()
{
    af.h |= de.l;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeB4()
{
    af.h |= hl.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeB5()
{
    af.h |= hl.l;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeB6()
{
    af.h |= mmu->read(hl.get());

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeB7()
{
    af.h |= af.h;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeB8()
{
    uint8_t result = af.h - bc.h;
    bool half_carry = (af.h & 0xF) < (bc.h & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, bc.h > af.h);

    return 0;
}

int CPU::opcodeB9()
{
    uint8_t result = af.h - bc.l;
    bool half_carry = (af.h & 0xF) < (bc.l & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, bc.l > af.h);

    return 0;
}

int CPU::opcodeBA()
{
    uint8_t result = af.h - de.h;
    bool half_carry = (af.h & 0xF) < (de.h & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, de.h > af.h);

    return 0;
}

int CPU::opcodeBB()
{
    uint8_t result = af.h - de.l;
    bool half_carry = (af.h & 0xF) < (de.l & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, de.l > af.h);

    return 0;
}

int CPU::opcodeBC()
{
    uint8_t result = af.h - hl.h;
    bool half_carry = (af.h & 0xF) < (hl.h & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, hl.h > af.h);

    return 0;
}

int CPU::opcodeBD()
{
    uint8_t result = af.h - hl.l;
    bool half_carry = (af.h & 0xF) < (hl.l & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, hl.l > af.h);

    return 0;
}

int CPU::opcodeBE()
{
    uint8_t value = mmu->read(hl.get());
    uint8_t result = af.h - value;
    bool half_carry = (af.h & 0xF) < (value & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, value > af.h);

    return 0;
}

int CPU::opcodeBF()
{
    set_flag(Z, 1);
    set_flag(N, 1);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeC0()
{
    if (!get_flag(Z)) {
        uint8_t low = mmu->read(sp++);
        uint8_t high = mmu->read(sp++);

        pc = combine(low, high);
        return 3;
    }
    
    return 0;
}

int CPU::opcodeC1()
{
    uint8_t low = mmu->read(sp++);
    uint8_t high = mmu->read(sp++);

    bc = combine(low, high);
    
    return 0;
}

int CPU::opcodeC2()
{
    if (!get_flag(Z)) {
        uint8_t low = mmu->read(pc++);
        uint8_t high = mmu->read(pc++);

        uint16_t addr = combine(low, high);
        pc = addr;

        return 1;
    }
    
    pc += 2; return 0;
}

int CPU::opcodeC3()
{
    uint8_t low = mmu->read(pc++);
    uint8_t high = mmu->read(pc++);

    uint16_t addr = combine(low, high);
    pc = addr;

    return 0;
}

int CPU::opcodeC4()
{
    if (!get_flag(Z)) {
        uint8_t low = mmu->read(pc++);
        uint8_t high = mmu->read(pc++);

        uint16_t addr = combine(low, high);
        
        sp--; mmu->write(sp, get_high_byte(pc));
        sp--; mmu->write(sp, get_low_byte(pc));
        
        pc = addr;

        return 3;
    }

    pc += 2; return 0;
}

int CPU::opcodeC5()
{
    sp--; mmu->write(sp, bc.h);
    sp--; mmu->write(sp, bc.l);
    
    return 0;
}

int CPU::opcodeC6()
{
    uint8_t value = mmu->read(pc++);
    uint32_t result = af.h + value;

    bool half_carry = (af.h & 0xF) + (value & 0xF) > 0xF;
    af.h = TU8(result);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, (result & 0x100) != 0);

    return 0;
}

int CPU::opcodeC7()
{
    sp--; mmu->write(sp, get_high_byte(pc));
    sp--; mmu->write(sp, get_low_byte(pc));
    
    pc = 0x00;
    return 0;
}

int CPU::opcodeC8()
{
    if (get_flag(Z)) {
        uint8_t low = mmu->read(sp++);
        uint8_t high = mmu->read(sp++);

        pc = combine(low, high);
        return 3;
    }
    
    return 0;
}

int CPU::opcodeC9()
{
    uint8_t low = mmu->read(sp++);
    uint8_t high = mmu->read(sp++);

    pc = combine(low, high);
    return 0;
}

int CPU::opcodeCA()
{
    if (get_flag(Z)) {
        uint8_t low = mmu->read(pc++);
        uint8_t high = mmu->read(pc++);

        uint16_t addr = combine(low, high);
        pc = addr;

        return 1;
    }

    pc += 2; return 0;
}

// Prefix
int CPU::opcodeCB()
{
    return 0;
}

int CPU::opcodeCC()
{
    if (get_flag(Z)) {
        uint8_t low = mmu->read(pc++);
        uint8_t high = mmu->read(pc++);

        uint16_t addr = combine(low, high);

        sp--; mmu->write(sp, get_high_byte(pc));
        sp--; mmu->write(sp, get_low_byte(pc));

        pc = addr;

        return 3;
    }

    pc += 2; return 0;
}

int CPU::opcodeCD()
{
    uint8_t low = mmu->read(pc++);
    uint8_t high = mmu->read(pc++);

    uint16_t addr = combine(low, high);

    sp--; mmu->write(sp, get_high_byte(pc));
    sp--; mmu->write(sp, get_low_byte(pc));

    pc = addr;
    
    return 0;
}

int CPU::opcodeCE()
{
    uint8_t value = mmu->read(pc++);
    uint8_t carry = get_flag(C);

    uint32_t result = af.h + value + carry;
    bool half_carry = ((af.h & 0xF) + (value & 0xF) + carry) > 0xF;
    af.h = TU8(result);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, result > 0xff);

    return 0;
}

int CPU::opcodeCF()
{
    sp--; mmu->write(sp, get_high_byte(pc));
    sp--; mmu->write(sp, get_low_byte(pc));

    pc = 0x08;
    return 0;
}

int CPU::opcodeD0()
{
    if (!get_flag(C)) {
        uint8_t low = mmu->read(sp++);
        uint8_t high = mmu->read(sp++);

        pc = combine(low, high);
        return 3;
    }

    return 0;
}

int CPU::opcodeD1()
{
    uint8_t low = mmu->read(sp++);
    uint8_t high = mmu->read(sp++);

    de = combine(low, high);

    return 0;
}

int CPU::opcodeD2()
{
    if (!get_flag(C)) {
        uint8_t low = mmu->read(pc++);
        uint8_t high = mmu->read(pc++);

        uint16_t addr = combine(low, high);
        pc = addr;

        return 1;
    }

    pc += 2; return 0;
}

int CPU::opcodeD3()
{
    return 0;
}

int CPU::opcodeD4()
{
    if (!get_flag(C)) {
        uint8_t low = mmu->read(pc++);
        uint8_t high = mmu->read(pc++);

        uint16_t addr = combine(low, high);

        sp--; mmu->write(sp, get_high_byte(pc));
        sp--; mmu->write(sp, get_low_byte(pc));

        pc = addr;

        return 3;
    }

    pc += 2; return 0;
}

int CPU::opcodeD5()
{
    sp--; mmu->write(sp, de.h);
    sp--; mmu->write(sp, de.l);

    return 0;
}

int CPU::opcodeD6()
{
    uint8_t val = mmu->read(pc++);
    uint8_t result = af.h - val;
    bool half_carry = (af.h & 0xF) < (val & 0xF);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, val > af.h);

    af.h = result;
    return 0;
}

int CPU::opcodeD7()
{
    sp--; mmu->write(sp, get_high_byte(pc));
    sp--; mmu->write(sp, get_low_byte(pc));

    pc = 0x10;
    return 0;
}

int CPU::opcodeD8()
{
    if (get_flag(C)) {
        uint8_t low = mmu->read(sp++);
        uint8_t high = mmu->read(sp++);

        pc = combine(low, high);
        return 3;
    }

    return 0;
}

int CPU::opcodeD9()
{
    uint8_t low = mmu->read(sp++);
    uint8_t high = mmu->read(sp++);

    pc = combine(low, high);
    interupts_enabled = true;
    return 0;
}

int CPU::opcodeDA()
{
    if (get_flag(C)) {
        uint8_t low = mmu->read(pc++);
        uint8_t high = mmu->read(pc++);

        uint16_t addr = combine(low, high);
        pc = addr;

        return 1;
    }

    pc += 2; return 0;
}

int CPU::opcodeDB()
{
    return 0;
}

int CPU::opcodeDC()
{
    if (get_flag(C)) {
        uint8_t low = mmu->read(pc++);
        uint8_t high = mmu->read(pc++);

        uint16_t addr = combine(low, high);

        sp--; mmu->write(sp, get_high_byte(pc));
        sp--; mmu->write(sp, get_low_byte(pc));

        pc = addr;

        return 3;
    }

    pc += 2; return 0;
}

int CPU::opcodeDD()
{
    return 0;
}

int CPU::opcodeDE()
{
    bool carry = get_flag(C);
    uint8_t value = mmu->read(pc++);
    uint8_t result = af.h - value - carry;
    bool half_carry = (af.h & 0xf) < (value & 0xf) + carry;

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, af.h < value + carry);

    af.h = result;
    return 0;
}

int CPU::opcodeDF()
{
    sp--; mmu->write(sp, get_high_byte(pc));
    sp--; mmu->write(sp, get_low_byte(pc));

    pc = 0x18;
    return 0;
}

int CPU::opcodeE0()
{
    uint8_t offset = mmu->read(pc++);
    uint16_t addr = 0xFF00 + offset;

    mmu->write(addr, af.h);
    
    return 0;
}

int CPU::opcodeE1()
{
    uint8_t low = mmu->read(sp++);
    uint8_t high = mmu->read(sp++);

    hl = combine(low, high);

    return 0;
}

int CPU::opcodeE2()
{
    uint16_t addr = 0xFF00 + bc.l;
    mmu->write(addr, af.h);
    
    return 0;
}

int CPU::opcodeE3()
{
    return 0;
}

int CPU::opcodeE4()
{
    return 0;
}

int CPU::opcodeE5()
{
    sp--; mmu->write(sp, hl.h);
    sp--; mmu->write(sp, hl.l);

    return 0;
}

int CPU::opcodeE6()
{
    uint8_t val = mmu->read(pc++);
    af.h &= val;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 1);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeE7()
{
    sp--; mmu->write(sp, get_high_byte(pc));
    sp--; mmu->write(sp, get_low_byte(pc));

    pc = 0x20;
    return 0;
}

int CPU::opcodeE8()
{
    int8_t data = T8(mmu->read(pc++));
    uint16_t result = sp + data;
    
    bool half_carry = (result & 0xF) < (sp & 0xF);
    bool carry = (result & 0xFF) < (sp & 0xFF);

    set_flag(Z, 0);
    set_flag(N, 0);
    set_flag(H, half_carry);
    set_flag(C, carry);

    sp = result;
    return 0;
}

int CPU::opcodeE9()
{
    pc = hl.get();
    return 0;
}

int CPU::opcodeEA()
{
    uint8_t low = mmu->read(pc++);
    uint8_t high = mmu->read(pc++);

    uint16_t addr = combine(low, high);
    mmu->write(addr, af.h);
    
    return 0;
}

int CPU::opcodeEB()
{
    return 0;
}

int CPU::opcodeEC()
{
    return 0;
}

int CPU::opcodeED()
{
    return 0;
}

int CPU::opcodeEE()
{
    uint8_t val = mmu->read(pc++);
    
    af.h ^= val;
    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeEF()
{
    sp--; mmu->write(sp, get_high_byte(pc));
    sp--; mmu->write(sp, get_low_byte(pc));

    pc = 0x28;
    return 0;
}

int CPU::opcodeF0()
{
    uint8_t offset = mmu->read(pc++);
    uint16_t addr = 0xFF00 + offset; 
    af.h = mmu->read(addr);

    return 0;
}

int CPU::opcodeF1()
{
    uint8_t low = mmu->read(sp++);
    uint8_t high = mmu->read(sp++);

    af = combine(low, high);
    af.l &= 0xF0;
    
    return 0;
}

int CPU::opcodeF2()
{
    uint16_t addr = 0xFF00 + bc.l;
    af.h = mmu->read(addr);

    return 0;
}

int CPU::opcodeF3()
{
    interupts_enabled = false;
    return 0;
}

int CPU::opcodeF4()
{
    return 0;
}

int CPU::opcodeF5()
{
    sp--; mmu->write(sp, af.h);
    sp--; mmu->write(sp, af.l);

    return 0;
}

int CPU::opcodeF6()
{
    uint8_t val = mmu->read(pc++);
    af.h |= val;

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeF7()
{
    sp--; mmu->write(sp, get_high_byte(pc));
    sp--; mmu->write(sp, get_low_byte(pc));

    pc = 0x30;
    return 0;
}

int CPU::opcodeF8()
{
    int8_t value = T8(mmu->read(pc++));
    uint16_t check = sp ^ value ^ ((sp + value) & 0xFFFF);
    hl = sp + value;

    set_flag(Z, 0);
    set_flag(N, 0);
    set_flag(H, (check & 0x10) == 0x10);
    set_flag(C, (check & 0x100) == 0x100);

    return 0;
}

int CPU::opcodeF9()
{
    sp = hl.get();
    return 0;
}

int CPU::opcodeFA()
{
    uint8_t low = mmu->read(pc++);
    uint8_t high = mmu->read(pc++);

    uint16_t addr = combine(low, high);
    af.h = mmu->read(addr);

    return 0;
}

int CPU::opcodeFB()
{
    interupts_enabled = true;   
    return 0;
}

int CPU::opcodeFC()
{
    return 0;
}

int CPU::opcodeFD()
{
    return 0;
}

int CPU::opcodeFE()
{
    uint8_t val = mmu->read(pc++);
    uint8_t result = af.h - val;
    bool half_carry = (af.h & 0xf) < (val & 0xf);

    set_flag(Z, result == 0);
    set_flag(N, 1);
    set_flag(H, half_carry);
    set_flag(C, af.h < val);

    return 0;
}

int CPU::opcodeFF()
{
    sp--; mmu->write(sp, get_high_byte(pc));
    sp--; mmu->write(sp, get_low_byte(pc));

    pc = 0x38;
    return 0;
}

int CPU::opcodeCB00()
{
    bool carry = get_bit(bc.h, 7);
    bc.h <<= 1; set_bit(bc.h, 0, carry);

    set_flag(Z, bc.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, carry);
    
    return 0;
}

int CPU::opcodeCB01()
{
    bool carry = get_bit(bc.l, 7);
    bc.l <<= 1; set_bit(bc.l, 0, carry);

    set_flag(Z, bc.l == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, carry);

    return 0;
}

int CPU::opcodeCB02()
{
    bool carry = get_bit(de.h, 7);
    de.h <<= 1; set_bit(de.h, 0, carry);

    set_flag(Z, de.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, carry);

    return 0;
}

int CPU::opcodeCB03()
{
    bool carry = get_bit(de.l, 7);
    de.l <<= 1; set_bit(de.l, 0, carry);

    set_flag(Z, de.l == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, carry);

    return 0;
}

int CPU::opcodeCB04()
{
    bool carry = get_bit(hl.h, 7);
    hl.h <<= 1; set_bit(hl.h, 0, carry);

    set_flag(Z, hl.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, carry);

    return 0;
}

int CPU::opcodeCB05()
{
    bool carry = get_bit(hl.l, 7);
    hl.l <<= 1; set_bit(hl.l, 0, carry);

    set_flag(Z, hl.l == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, carry);

    return 0;
}

int CPU::opcodeCB06()
{
    uint8_t value = mmu->read(hl.get());
    bool carry = get_bit(value, 7);
    value <<= 1; set_bit(value, 0, carry);

    set_flag(Z, value == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, carry);

    mmu->write(hl.get(), value);
    return 0;
}

int CPU::opcodeCB07()
{
    bool carry = get_bit(af.h, 7);
    af.h <<= 1; set_bit(af.h, 0, carry);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, carry);

    return 0;
}

int CPU::opcodeCB08()
{
    uint8_t carry = get_bit(bc.h, 0);
    bc.h >>= 1; set_bit(bc.h, 7, carry);

    set_flag(C, carry);
    set_flag(Z, bc.h == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    return 0;
}

int CPU::opcodeCB09()
{
    uint8_t carry = get_bit(bc.l, 0);
    bc.l >>= 1; set_bit(bc.l, 7, carry);

    set_flag(C, carry);
    set_flag(Z, bc.l == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    return 0;
}

int CPU::opcodeCB0A()
{
    uint8_t carry = get_bit(de.h, 0);
    de.h >>= 1; set_bit(de.h, 7, carry);

    set_flag(C, carry);
    set_flag(Z, de.h == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    return 0;
}

int CPU::opcodeCB0B()
{
    uint8_t carry = get_bit(de.l, 0);
    de.l >>= 1; set_bit(de.l, 7, carry);

    set_flag(C, carry);
    set_flag(Z, de.l == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    return 0;
}

int CPU::opcodeCB0C()
{
    uint8_t carry = get_bit(hl.h, 0);
    hl.h >>= 1; set_bit(hl.h, 7, carry);

    set_flag(C, carry);
    set_flag(Z, hl.h == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    return 0;
}

int CPU::opcodeCB0D()
{
    uint8_t carry = get_bit(hl.l, 0);
    hl.l >>= 1; set_bit(hl.l, 7, carry);

    set_flag(C, carry);
    set_flag(Z, hl.l == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    return 0;
}

int CPU::opcodeCB0E()
{
    uint8_t value = mmu->read(hl.get());
    uint8_t carry = get_bit(value, 0);
    value >>= 1; set_bit(value, 7, carry);

    set_flag(C, carry);
    set_flag(Z, value == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    mmu->write(hl.get(), value);
    return 0;
}

int CPU::opcodeCB0F()
{
    uint8_t carry = get_bit(af.h, 0);
    af.h >>= 1; set_bit(af.h, 7, carry);

    set_flag(C, carry);
    set_flag(Z, af.h == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    return 0;
}

int CPU::opcodeCB10()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(bc.h, 7));

    bc.h <<= 1; set_bit(bc.h, 0, carry);

    set_flag(Z, bc.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB11()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(bc.l, 7));

    bc.l <<= 1; set_bit(bc.l, 0, carry);

    set_flag(Z, bc.l == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB12()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(de.h, 7));

    de.h <<= 1; set_bit(de.h, 0, carry);

    set_flag(Z, de.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB13()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(de.l, 7));

    de.l <<= 1; set_bit(de.l, 0, carry);

    set_flag(Z, de.l == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB14()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(hl.h, 7));

    hl.h <<= 1; set_bit(hl.h, 0, carry);

    set_flag(Z, hl.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB15()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(hl.l, 7));

    hl.l <<= 1; set_bit(hl.l, 0, carry);

    set_flag(Z, hl.l == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB16()
{
    uint8_t value = mmu->read(hl.get());
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(value, 7));

    value <<= 1; set_bit(value, 0, carry);

    set_flag(Z, value == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    mmu->write(hl.get(), value);
    return 0;
}

int CPU::opcodeCB17()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(af.h, 7));

    af.h <<= 1; set_bit(af.h, 0, carry);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB18()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(bc.h, 0));

    bc.h >>= 1; set_bit(bc.h, 7, carry);

    set_flag(Z, bc.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB19()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(bc.l, 0));

    bc.l >>= 1; set_bit(bc.l, 7, carry);

    set_flag(Z, bc.l == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB1A()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(de.h, 0));

    de.h >>= 1; set_bit(de.h, 7, carry);

    set_flag(Z, de.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB1B()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(de.l, 0));

    de.l >>= 1; set_bit(de.l, 7, carry);

    set_flag(Z, de.l == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB1C()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(hl.h, 0));

    hl.h >>= 1; set_bit(hl.h, 7, carry);

    set_flag(Z, hl.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB1D()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(hl.l, 0));

    hl.l >>= 1; set_bit(hl.l, 7, carry);

    set_flag(Z, hl.l == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB1E()
{
    uint8_t value = mmu->read(hl.get());
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(value, 0));

    value >>= 1; set_bit(value, 7, carry);

    set_flag(Z, value == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    mmu->write(hl.get(), value);
    return 0;
}

int CPU::opcodeCB1F()
{
    uint8_t carry = get_flag(C);
    set_flag(C, get_bit(af.h, 0));

    af.h >>= 1; set_bit(af.h, 7, carry);

    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);

    return 0;
}

int CPU::opcodeCB20()
{
    uint8_t reg = bc.h;
    uint8_t carry_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg << 1);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);
    
    bc.h = result;

    return 0;
}

int CPU::opcodeCB21()
{
    uint8_t reg = bc.l;
    uint8_t carry_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg << 1);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    bc.l = result;

    return 0;
}

int CPU::opcodeCB22()
{
    uint8_t reg = de.h;
    uint8_t carry_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg << 1);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    de.h = result;

    return 0;
}

int CPU::opcodeCB23()
{
    uint8_t reg = de.l;
    uint8_t carry_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg << 1);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    de.l = result;

    return 0;
}

int CPU::opcodeCB24()
{
    uint8_t reg = hl.h;
    uint8_t carry_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg << 1);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    hl.h = result;

    return 0;
}

int CPU::opcodeCB25()
{
    uint8_t reg = hl.l;
    uint8_t carry_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg << 1);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    hl.l = result;

    return 0;
}

int CPU::opcodeCB26()
{
    uint8_t reg = mmu->read(hl.get());
    uint8_t carry_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg << 1);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    mmu->write(hl.get(), result);

    return 0;
}

int CPU::opcodeCB27()
{
    uint8_t reg = af.h;
    uint8_t carry_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg << 1);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    af.h = result;

    return 0;
}

int CPU::opcodeCB28()
{
    uint8_t reg = bc.h;
    uint8_t carry_bit = get_bit(reg, 0);
    uint8_t top_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg >> 1);
    set_bit(result, 7, top_bit);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    bc.h = result;

    return 0;
}

int CPU::opcodeCB29()
{
    uint8_t reg = bc.l;
    uint8_t carry_bit = get_bit(reg, 0);
    uint8_t top_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg >> 1);
    set_bit(result, 7, top_bit);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    bc.l = result;

    return 0;
}

int CPU::opcodeCB2A()
{
    uint8_t reg = de.h;
    uint8_t carry_bit = get_bit(reg, 0);
    uint8_t top_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg >> 1);
    set_bit(result, 7, top_bit);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    de.h = result;

    return 0;
}

int CPU::opcodeCB2B()
{
    uint8_t reg = de.l;
    uint8_t carry_bit = get_bit(reg, 0);
    uint8_t top_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg >> 1);
    set_bit(result, 7, top_bit);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    de.l = result;

    return 0;
}

int CPU::opcodeCB2C()
{
    uint8_t reg = hl.h;
    uint8_t carry_bit = get_bit(reg, 0);
    uint8_t top_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg >> 1);
    set_bit(result, 7, top_bit);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    hl.h = result;

    return 0;
}

int CPU::opcodeCB2D()
{
    uint8_t reg = hl.l;
    uint8_t carry_bit = get_bit(reg, 0);
    uint8_t top_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg >> 1);
    set_bit(result, 7, top_bit);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    hl.l = result;

    return 0;
}

int CPU::opcodeCB2E()
{
    uint8_t reg = mmu->read(hl.get());
    uint8_t carry_bit = get_bit(reg, 0);
    uint8_t top_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg >> 1);
    set_bit(result, 7, top_bit);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    mmu->write(hl.get(), result);

    return 0;
}

int CPU::opcodeCB2F()
{
    uint8_t reg = af.h;
    uint8_t carry_bit = get_bit(reg, 0);
    uint8_t top_bit = get_bit(reg, 7);

    uint8_t result = TU8(reg >> 1);
    set_bit(result, 7, top_bit);

    set_flag(Z, result == 0);
    set_flag(C, carry_bit);
    set_flag(H, 0);
    set_flag(N, 0);

    af.h = result;

    return 0;
}

int CPU::opcodeCB30()
{
    uint8_t reg = bc.h;
    uint8_t lower_nibble = reg & 0x0F;
    uint8_t upper_nibble = (reg & 0xF0) >> 4;

    uint8_t result = combine_nibbles(upper_nibble, lower_nibble);

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);
    
    bc.h = result;

    return 0;
}

int CPU::opcodeCB31()
{
    uint8_t reg = bc.l;
    uint8_t lower_nibble = reg & 0x0F;
    uint8_t upper_nibble = (reg & 0xF0) >> 4;

    uint8_t result = combine_nibbles(upper_nibble, lower_nibble);

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    bc.l = result;

    return 0;
}

int CPU::opcodeCB32()
{
    uint8_t reg = de.h;
    uint8_t lower_nibble = reg & 0x0F;
    uint8_t upper_nibble = (reg & 0xF0) >> 4;

    uint8_t result = combine_nibbles(upper_nibble, lower_nibble);

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    de.h = result;

    return 0;
}

int CPU::opcodeCB33()
{
    uint8_t reg = de.l;
    uint8_t lower_nibble = reg & 0x0F;
    uint8_t upper_nibble = (reg & 0xF0) >> 4;

    uint8_t result = combine_nibbles(upper_nibble, lower_nibble);

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    de.l = result;

    return 0;
}

int CPU::opcodeCB34()
{
    uint8_t reg = hl.h;
    uint8_t lower_nibble = reg & 0x0F;
    uint8_t upper_nibble = (reg & 0xF0) >> 4;

    uint8_t result = combine_nibbles(upper_nibble, lower_nibble);

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    hl.h = result;

    return 0;
}

int CPU::opcodeCB35()
{
    uint8_t reg = hl.l;
    uint8_t lower_nibble = reg & 0x0F;
    uint8_t upper_nibble = (reg & 0xF0) >> 4;

    uint8_t result = combine_nibbles(upper_nibble, lower_nibble);

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    hl.l = result;

    return 0;
}

int CPU::opcodeCB36()
{
    uint8_t reg = mmu->read(hl.get());
    uint8_t lower_nibble = reg & 0x0F;
    uint8_t upper_nibble = (reg & 0xF0) >> 4;

    uint8_t result = combine_nibbles(upper_nibble, lower_nibble);

    set_flag(Z, result == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    mmu->write(hl.get(), result);

    return 0;
}

int CPU::opcodeCB37()
{
    af.h = (af.h << 4) | (af.h >> 4);
    
    set_flag(Z, af.h == 0);
    set_flag(N, 0);
    set_flag(H, 0);
    set_flag(C, 0);

    return 0;
}

int CPU::opcodeCB38()
{
    uint8_t reg = bc.h;
    bool least_bit_set = get_bit(reg, 0);

    uint8_t result = (reg >> 1);
    
    set_flag(C, least_bit_set);
    set_flag(Z, result == 0);
    set_flag(H, 0);
    set_flag(N, 0);
    
    bc.h = result;

    return 0;
}

int CPU::opcodeCB39()
{
    uint8_t reg = bc.l;
    bool least_bit_set = get_bit(reg, 0);

    uint8_t result = (reg >> 1);

    set_flag(C, least_bit_set);
    set_flag(Z, result == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    bc.l = result;

    return 0;
}

int CPU::opcodeCB3A()
{
    uint8_t reg = de.h;
    bool least_bit_set = get_bit(reg, 0);

    uint8_t result = (reg >> 1);

    set_flag(C, least_bit_set);
    set_flag(Z, result == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    de.h = result;

    return 0;
}

int CPU::opcodeCB3B()
{
    uint8_t reg = de.l;
    bool least_bit_set = get_bit(reg, 0);

    uint8_t result = (reg >> 1);

    set_flag(C, least_bit_set);
    set_flag(Z, result == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    de.l = result;

    return 0;
}

int CPU::opcodeCB3C()
{
    uint8_t reg = hl.h;
    bool least_bit_set = get_bit(reg, 0);

    uint8_t result = (reg >> 1);

    set_flag(C, least_bit_set);
    set_flag(Z, result == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    hl.h = result;

    return 0;
}

int CPU::opcodeCB3D()
{
    uint8_t reg = hl.l;
    bool least_bit_set = get_bit(reg, 0);

    uint8_t result = (reg >> 1);

    set_flag(C, least_bit_set);
    set_flag(Z, result == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    hl.l = result;

    return 0;
}

int CPU::opcodeCB3E()
{
    uint8_t reg = mmu->read(hl.get());
    bool least_bit_set = get_bit(reg, 0);

    uint8_t result = (reg >> 1);

    set_flag(C, least_bit_set);
    set_flag(Z, result == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    mmu->write(hl.get(), result);

    return 0;
}

int CPU::opcodeCB3F()
{
    uint8_t reg = af.h;
    bool least_bit_set = get_bit(reg, 0);

    uint8_t result = (reg >> 1);

    set_flag(C, least_bit_set);
    set_flag(Z, result == 0);
    set_flag(H, 0);
    set_flag(N, 0);

    af.h = result;

    return 0;
}

int CPU::opcodeCB40()
{
    uint8_t reg = bc.h;
    bool bit = get_bit(reg, 0);
    
    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB41()
{
    uint8_t reg = bc.l;
    bool bit = get_bit(reg, 0);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB42()
{
    uint8_t reg = de.h;
    bool bit = get_bit(reg, 0);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB43()
{
    uint8_t reg = de.l;
    bool bit = get_bit(reg, 0);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB44()
{
    uint8_t reg = hl.h;
    bool bit = get_bit(reg, 0);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB45()
{
    uint8_t reg = hl.l;
    bool bit = get_bit(reg, 0);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB46()
{
    uint8_t reg = mmu->read(hl.get());
    bool bit = get_bit(reg, 0);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB47()
{
    uint8_t reg = af.h;
    bool bit = get_bit(reg, 0);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB48()
{
    uint8_t reg = bc.h;
    bool bit = get_bit(reg, 1);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB49()
{
    uint8_t reg = bc.l;
    bool bit = get_bit(reg, 1);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB4A()
{
    uint8_t reg = de.h;
    bool bit = get_bit(reg, 1);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB4B()
{
    uint8_t reg = de.l;
    bool bit = get_bit(reg, 1);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB4C()
{
    uint8_t reg = hl.h;
    bool bit = get_bit(reg, 1);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB4D()
{
    uint8_t reg = hl.l;
    bool bit = get_bit(reg, 1);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB4E()
{
    uint8_t reg = mmu->read(hl.get());
    bool bit = get_bit(reg, 1);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB4F()
{
    uint8_t reg = af.h;
    bool bit = get_bit(reg, 1);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB50()
{
    uint8_t reg = bc.h;
    bool bit = get_bit(reg, 2);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB51()
{
    uint8_t reg = bc.l;
    bool bit = get_bit(reg, 2);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB52()
{
    uint8_t reg = de.h;
    bool bit = get_bit(reg, 2);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB53()
{
    uint8_t reg = de.l;
    bool bit = get_bit(reg, 2);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB54()
{
    uint8_t reg = hl.h;
    bool bit = get_bit(reg, 2);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB55()
{
    uint8_t reg = hl.l;
    bool bit = get_bit(reg, 2);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB56()
{
    uint8_t reg = mmu->read(hl.get());
    bool bit = get_bit(reg, 2);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB57()
{
    uint8_t reg = af.h;
    bool bit = get_bit(reg, 2);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB58()
{
    uint8_t reg = bc.h;
    bool bit = get_bit(reg, 3);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB59()
{
    uint8_t reg = bc.l;
    bool bit = get_bit(reg, 3);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB5A()
{
    uint8_t reg = de.h;
    bool bit = get_bit(reg, 3);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB5B()
{
    uint8_t reg = de.l;
    bool bit = get_bit(reg, 3);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB5C()
{
    uint8_t reg = hl.h;
    bool bit = get_bit(reg, 3);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB5D()
{
    uint8_t reg = hl.l;
    bool bit = get_bit(reg, 3);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB5E()
{
    uint8_t reg = mmu->read(hl.get());
    bool bit = get_bit(reg, 3);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB5F()
{
    uint8_t reg = af.h;
    bool bit = get_bit(reg, 3);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB60()
{
    uint8_t reg = bc.h;
    bool bit = get_bit(reg, 4);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB61()
{
    uint8_t reg = bc.l;
    bool bit = get_bit(reg, 4);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB62()
{
    uint8_t reg = de.h;
    bool bit = get_bit(reg, 4);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB63()
{
    uint8_t reg = de.l;
    bool bit = get_bit(reg, 4);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB64()
{
    uint8_t reg = hl.h;
    bool bit = get_bit(reg, 4);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB65()
{
    uint8_t reg = hl.l;
    bool bit = get_bit(reg, 4);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB66()
{
    uint8_t reg = mmu->read(hl.get());
    bool bit = get_bit(reg, 4);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB67()
{
    uint8_t reg = af.h;
    bool bit = get_bit(reg, 4);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB68()
{
    uint8_t reg = bc.h;
    bool bit = get_bit(reg, 5);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB69()
{
    uint8_t reg = bc.l;
    bool bit = get_bit(reg, 5);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB6A()
{
    uint8_t reg = de.h;
    bool bit = get_bit(reg, 5);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB6B()
{
    uint8_t reg = de.l;
    bool bit = get_bit(reg, 5);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB6C()
{
    uint8_t reg = hl.h;
    bool bit = get_bit(reg, 5);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB6D()
{
    uint8_t reg = hl.l;
    bool bit = get_bit(reg, 5);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB6E()
{
    uint8_t reg = mmu->read(hl.get());
    bool bit = get_bit(reg, 5);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB6F()
{
    uint8_t reg = af.h;
    bool bit = get_bit(reg, 5);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB70()
{
    uint8_t reg = bc.h;
    bool bit = get_bit(reg, 6);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB71()
{
    uint8_t reg = bc.l;
    bool bit = get_bit(reg, 6);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB72()
{
    uint8_t reg = de.h;
    bool bit = get_bit(reg, 6);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB73()
{
    uint8_t reg = de.l;
    bool bit = get_bit(reg, 6);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB74()
{
    uint8_t reg = hl.h;
    bool bit = get_bit(reg, 6);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB75()
{
    uint8_t reg = hl.l;
    bool bit = get_bit(reg, 6);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB76()
{
    uint8_t reg = mmu->read(hl.get());
    bool bit = get_bit(reg, 6);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB77()
{
    uint8_t reg = af.h;
    bool bit = get_bit(reg, 6);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB78()
{
    uint8_t reg = bc.h;
    bool bit = get_bit(reg, 7);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB79()
{
    uint8_t reg = bc.l;
    bool bit = get_bit(reg, 7);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB7A()
{
    uint8_t reg = de.h;
    bool bit = get_bit(reg, 7);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB7B()
{
    uint8_t reg = de.l;
    bool bit = get_bit(reg, 7);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB7C()
{
    bool bit = get_bit(hl.h, 7);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);
    
    return 0;
}

int CPU::opcodeCB7D()
{
    uint8_t reg = hl.l;
    bool bit = get_bit(reg, 7);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB7E()
{
    uint8_t reg = mmu->read(hl.get());
    bool bit = get_bit(reg, 7);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB7F()
{
    uint8_t reg = af.h;
    bool bit = get_bit(reg, 7);

    set_flag(Z, !bit);
    set_flag(N, 0);
    set_flag(H, 1);

    return 0;
}

int CPU::opcodeCB80()
{
    uint8_t reg = bc.h;
    int bit = 0;
    set_bit(reg, bit, 0);
    
    bc.h = reg;

    return 0;
}

int CPU::opcodeCB81()
{
    uint8_t reg = bc.l;
    int bit = 0;
    set_bit(reg, bit, 0);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCB82()
{
    uint8_t reg = de.h;
    int bit = 0;
    set_bit(reg, bit, 0);

    de.h = reg;

    return 0;
}

int CPU::opcodeCB83()
{
    uint8_t reg = de.l;
    int bit = 0;
    set_bit(reg, bit, 0);

    de.l = reg;

    return 0;
}

int CPU::opcodeCB84()
{
    uint8_t reg = hl.h;
    int bit = 0;
    set_bit(reg, bit, 0);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCB85()
{
    uint8_t reg = hl.l;
    int bit = 0;
    set_bit(reg, bit, 0);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCB86()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 0;
    set_bit(reg, bit, 0);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCB87()
{
    uint8_t reg = af.h;
    int bit = 0;
    set_bit(reg, bit, 0);

    af.h = reg;

    return 0;
}

int CPU::opcodeCB88()
{
    uint8_t reg = bc.h;
    int bit = 1;
    set_bit(reg, bit, 0);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCB89()
{
    uint8_t reg = bc.l;
    int bit = 1;
    set_bit(reg, bit, 0);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCB8A()
{
    uint8_t reg = de.h;
    int bit = 1;
    set_bit(reg, bit, 0);

    de.h = reg;

    return 0;
}

int CPU::opcodeCB8B()
{
    uint8_t reg = de.l;
    int bit = 1;
    set_bit(reg, bit, 0);

    de.l = reg;

    return 0;
}

int CPU::opcodeCB8C()
{
    uint8_t reg = hl.h;
    int bit = 1;
    set_bit(reg, bit, 0);
    
    hl.h = reg;

    return 0;
}

int CPU::opcodeCB8D()
{
    set_bit(hl.l, 1, 0);
    return 0;
}

int CPU::opcodeCB8E()
{
    uint8_t reg = mmu->read(hl.get());
    set_bit(reg, 1, 0);

    mmu->write(hl.get(), reg);
    return 0;
}

int CPU::opcodeCB8F()
{
    set_bit(af.h, 1, 0);
    return 0;
}

int CPU::opcodeCB90()
{
    uint8_t reg = bc.h;
    int bit = 2;
    set_bit(reg, bit, 0);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCB91()
{
    uint8_t reg = bc.l;
    int bit = 2;
    set_bit(reg, bit, 0);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCB92()
{
    uint8_t reg = de.h;
    int bit = 2;
    set_bit(reg, bit, 0);

    de.h = reg;

    return 0;
}

int CPU::opcodeCB93()
{
    uint8_t reg = de.l;
    int bit = 2;
    set_bit(reg, bit, 0);

    de.l = reg;

    return 0;
}

int CPU::opcodeCB94()
{
    uint8_t reg = hl.h;
    int bit = 2;
    set_bit(reg, bit, 0);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCB95()
{
    uint8_t reg = hl.l;
    int bit = 2;
    set_bit(reg, bit, 0);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCB96()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 2;
    set_bit(reg, bit, 0);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCB97()
{
    uint8_t reg = af.h;
    int bit = 2;
    set_bit(reg, bit, 0);

    af.h = reg;

    return 0;
}

int CPU::opcodeCB98()
{
    uint8_t reg = bc.h;
    int bit = 3;
    set_bit(reg, bit, 0);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCB99()
{
    uint8_t reg = bc.l;
    int bit = 3;
    set_bit(reg, bit, 0);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCB9A()
{
    uint8_t reg = de.h;
    int bit = 3;
    set_bit(reg, bit, 0);

    de.h = reg;

    return 0;
}

int CPU::opcodeCB9B()
{
    uint8_t reg = de.l;
    int bit = 3;
    set_bit(reg, bit, 0);

    de.l = reg;

    return 0;
}

int CPU::opcodeCB9C()
{
    uint8_t reg = hl.h;
    int bit = 3;
    set_bit(reg, bit, 0);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCB9D()
{
    uint8_t reg = hl.l;
    int bit = 3;
    set_bit(reg, bit, 0);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCB9E()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 3;
    set_bit(reg, bit, 0);

   mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCB9F()
{
    uint8_t reg = af.h;
    int bit = 3;
    set_bit(reg, bit, 0);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBA0()
{
    uint8_t reg = bc.h;
    int bit = 4;
    set_bit(reg, bit, 0);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBA1()
{
    uint8_t reg = bc.l;
    int bit = 4;
    set_bit(reg, bit, 0);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBA2()
{
    uint8_t reg = de.h;
    int bit = 4;
    set_bit(reg, bit, 0);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBA3()
{
    uint8_t reg = de.l;
    int bit = 4;
    set_bit(reg, bit, 0);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBA4()
{
    uint8_t reg = hl.h;
    int bit = 4;
    set_bit(reg, bit, 0);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBA5()
{
    uint8_t reg = hl.l;
    int bit = 4;
    set_bit(reg, bit, 0);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBA6()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 4;
    set_bit(reg, bit, 0);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBA7()
{
    uint8_t reg = af.h;
    int bit = 4;
    set_bit(reg, bit, 0);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBA8()
{
    uint8_t reg = bc.h;
    int bit = 5;
    set_bit(reg, bit, 0);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBA9()
{
    uint8_t reg = bc.l;
    int bit = 5;
    set_bit(reg, bit, 0);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBAA()
{
    uint8_t reg = de.h;
    int bit = 5;
    set_bit(reg, bit, 0);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBAB()
{
    uint8_t reg = de.l;
    int bit = 5;
    set_bit(reg, bit, 0);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBAC()
{
    uint8_t reg = hl.h;
    int bit = 5;
    set_bit(reg, bit, 0);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBAD()
{
    uint8_t reg = hl.l;
    int bit = 5;
    set_bit(reg, bit, 0);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBAE()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 5;
    set_bit(reg, bit, 0);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBAF()
{
    uint8_t reg = af.h;
    int bit = 5;
    set_bit(reg, bit, 0);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBB0()
{
    uint8_t reg = bc.h;
    int bit = 6;
    set_bit(reg, bit, 0);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBB1()
{
    uint8_t reg = bc.l;
    int bit = 6;
    set_bit(reg, bit, 0);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBB2()
{
    uint8_t reg = de.h;
    int bit = 6;
    set_bit(reg, bit, 0);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBB3()
{
    uint8_t reg = de.l;
    int bit = 6;
    set_bit(reg, bit, 0);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBB4()
{
    uint8_t reg = hl.h;
    int bit = 6;
    set_bit(reg, bit, 0);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBB5()
{
    uint8_t reg = hl.l;
    int bit = 6;
    set_bit(reg, bit, 0);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBB6()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 6;
    set_bit(reg, bit, 0);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBB7()
{
    uint8_t reg = af.h;
    int bit = 6;
    set_bit(reg, bit, 0);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBB8()
{
    uint8_t reg = bc.h;
    int bit = 7;
    set_bit(reg, bit, 0);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBB9()
{
    uint8_t reg = bc.l;
    int bit = 7;
    set_bit(reg, bit, 0);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBBA()
{
    uint8_t reg = de.h;
    int bit = 7;
    set_bit(reg, bit, 0);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBBB()
{
    uint8_t reg = de.l;
    int bit = 7;
    set_bit(reg, bit, 0);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBBC()
{
    uint8_t reg = hl.h;
    int bit = 7;
    set_bit(reg, bit, 0);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBBD()
{
    uint8_t reg = hl.l;
    int bit = 7;
    set_bit(reg, bit, 0);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBBE()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 7;
    set_bit(reg, bit, 0);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBBF()
{
    uint8_t reg = af.h;
    int bit = 7;
    set_bit(reg, bit, 0);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBC0()
{
    uint8_t reg = bc.h;
    int bit = 0;
    set_bit(reg, bit, 1);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBC1()
{
    uint8_t reg = bc.l;
    int bit = 0;
    set_bit(reg, bit, 1);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBC2()
{
    uint8_t reg = de.h;
    int bit = 0;
    set_bit(reg, bit, 1);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBC3()
{
    uint8_t reg = de.l;
    int bit = 0;
    set_bit(reg, bit, 1);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBC4()
{
    uint8_t reg = hl.h;
    int bit = 0;
    set_bit(reg, bit, 1);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBC5()
{
    uint8_t reg = hl.l;
    int bit = 0;
    set_bit(reg, bit, 1);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBC6()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 0;
    set_bit(reg, bit, 1);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBC7()
{
    uint8_t reg = af.h;
    int bit = 0;
    set_bit(reg, bit, 1);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBC8()
{
    uint8_t reg = bc.h;
    int bit = 1;
    set_bit(reg, bit, 1);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBC9()
{
    uint8_t reg = bc.l;
    int bit = 1;
    set_bit(reg, bit, 1);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBCA()
{
    uint8_t reg = de.h;
    int bit = 1;
    set_bit(reg, bit, 1);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBCB()
{
    uint8_t reg = de.l;
    int bit = 1;
    set_bit(reg, bit, 1);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBCC()
{
    uint8_t reg = hl.h;
    int bit = 1;
    set_bit(reg, bit, 1);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBCD()
{
    uint8_t reg = hl.l;
    int bit = 1;
    set_bit(reg, bit, 1);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBCE()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 1;
    set_bit(reg, bit, 1);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBCF()
{
    uint8_t reg = af.h;
    int bit = 1;
    set_bit(reg, bit, 1);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBD0()
{
    uint8_t reg = bc.h;
    int bit = 2;
    set_bit(reg, bit, 1);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBD1()
{
    uint8_t reg = bc.l;
    int bit = 2;
    set_bit(reg, bit, 1);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBD2()
{
    uint8_t reg = de.h;
    int bit = 2;
    set_bit(reg, bit, 1);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBD3()
{
    uint8_t reg = de.l;
    int bit = 2;
    set_bit(reg, bit, 1);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBD4()
{
    uint8_t reg = hl.h;
    int bit = 2;
    set_bit(reg, bit, 1);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBD5()
{
    uint8_t reg = hl.l;
    int bit = 2;
    set_bit(reg, bit, 1);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBD6()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 2;
    set_bit(reg, bit, 1);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBD7()
{
    uint8_t reg = af.h;
    int bit = 2;
    set_bit(reg, bit, 1);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBD8()
{
    uint8_t reg = bc.h;
    int bit = 3;
    set_bit(reg, bit, 1);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBD9()
{
    uint8_t reg = bc.l;
    int bit = 3;
    set_bit(reg, bit, 1);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBDA()
{
    uint8_t reg = de.h;
    int bit = 3;
    set_bit(reg, bit, 1);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBDB()
{
    uint8_t reg = de.l;
    int bit = 3;
    set_bit(reg, bit, 1);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBDC()
{
    uint8_t reg = hl.h;
    int bit = 3;
    set_bit(reg, bit, 1);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBDD()
{
    uint8_t reg = hl.l;
    int bit = 3;
    set_bit(reg, bit, 1);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBDE()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 3;
    set_bit(reg, bit, 1);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBDF()
{
    uint8_t reg = af.h;
    int bit = 3;
    set_bit(reg, bit, 1);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBE0()
{
    uint8_t reg = bc.h;
    int bit = 4;
    set_bit(reg, bit, 1);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBE1()
{
    uint8_t reg = bc.l;
    int bit = 4;
    set_bit(reg, bit, 1);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBE2()
{
    uint8_t reg = de.h;
    int bit = 4;
    set_bit(reg, bit, 1);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBE3()
{
    uint8_t reg = de.l;
    int bit = 4;
    set_bit(reg, bit, 1);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBE4()
{
    uint8_t reg = hl.h;
    int bit = 4;
    set_bit(reg, bit, 1);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBE5()
{
    uint8_t reg = hl.l;
    int bit = 4;
    set_bit(reg, bit, 1);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBE6()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 4;
    set_bit(reg, bit, 1);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBE7()
{
    uint8_t reg = af.h;
    int bit = 4;
    set_bit(reg, bit, 1);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBE8()
{
    uint8_t reg = bc.h;
    int bit = 5;
    set_bit(reg, bit, 1);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBE9()
{
    uint8_t reg = bc.l;
    int bit = 5;
    set_bit(reg, bit, 1);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBEA()
{
    uint8_t reg = de.h;
    int bit = 5;
    set_bit(reg, bit, 1);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBEB()
{
    uint8_t reg = de.l;
    int bit = 5;
    set_bit(reg, bit, 1);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBEC()
{
    uint8_t reg = hl.h;
    int bit = 5;
    set_bit(reg, bit, 1);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBED()
{
    uint8_t reg = hl.l;
    int bit = 5;
    set_bit(reg, bit, 1);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBEE()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 5;
    set_bit(reg, bit, 1);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBEF()
{
    uint8_t reg = af.h;
    int bit = 5;
    set_bit(reg, bit, 1);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBF0()
{
    uint8_t reg = bc.h;
    int bit = 6;
    set_bit(reg, bit, 1);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBF1()
{
    uint8_t reg = bc.l;
    int bit = 6;
    set_bit(reg, bit, 1);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBF2()
{
    uint8_t reg = de.h;
    int bit = 6;
    set_bit(reg, bit, 1);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBF3()
{
    uint8_t reg = de.l;
    int bit = 6;
    set_bit(reg, bit, 1);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBF4()
{
    uint8_t reg = hl.h;
    int bit = 6;
    set_bit(reg, bit, 1);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBF5()
{
    uint8_t reg = hl.l;
    int bit = 6;
    set_bit(reg, bit, 1);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBF6()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 6;
    set_bit(reg, bit, 1);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBF7()
{
    uint8_t reg = af.h;
    int bit = 6;
    set_bit(reg, bit, 1);

    af.h = reg;

    return 0;
}

int CPU::opcodeCBF8()
{
    uint8_t reg = bc.h;
    int bit = 7;
    set_bit(reg, bit, 1);

    bc.h = reg;

    return 0;
}

int CPU::opcodeCBF9()
{
    uint8_t reg = bc.l;
    int bit = 7;
    set_bit(reg, bit, 1);

    bc.l = reg;

    return 0;
}

int CPU::opcodeCBFA()
{
    uint8_t reg = de.h;
    int bit = 7;
    set_bit(reg, bit, 1);

    de.h = reg;

    return 0;
}

int CPU::opcodeCBFB()
{
    uint8_t reg = de.l;
    int bit = 7;
    set_bit(reg, bit, 1);

    de.l = reg;

    return 0;
}

int CPU::opcodeCBFC()
{
    uint8_t reg = hl.h;
    int bit = 7;
    set_bit(reg, bit, 1);

    hl.h = reg;

    return 0;
}

int CPU::opcodeCBFD()
{
    uint8_t reg = hl.l;
    int bit = 7;
    set_bit(reg, bit, 1);

    hl.l = reg;

    return 0;
}

int CPU::opcodeCBFE()
{
    uint8_t reg = mmu->read(hl.get());
    int bit = 7;
    set_bit(reg, bit, 1);

    mmu->write(hl.get(), reg);

    return 0;
}

int CPU::opcodeCBFF()
{
    uint8_t reg = af.h;
    int bit = 7;
    set_bit(reg, bit, 1);

    af.h = reg;

    return 0;
}
