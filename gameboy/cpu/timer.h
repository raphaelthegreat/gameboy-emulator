#pragma once
#include <cstdint>

class MMU;
class Timer
{
public:
    Timer(MMU* mmu);
    ~Timer() = default;

    void update(const uint8_t cycles);

private:
    void tick();

    uint8_t& controller_; // TAC
    uint8_t& counter_;    // TIMA
    uint8_t& modulo_;     // TMA
    uint8_t& divider_;    // DIV

    int t_clock_;
    int base_clock_;
    int div_clock_;

    MMU* _mmu;
};