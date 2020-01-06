#include "timer.h"
#include <cpu/mmu.h>
#include <gameboy.h>

Timer::Timer(MMU* mmu) :
    _mmu(mmu),
    controller_(mmu->get(TAC)),
    counter_(mmu->get(TIMA)),
    modulo_(mmu->get(TMA)),
    divider_(mmu->get(DIV)),
    t_clock_(0),
    base_clock_(0),
    div_clock_(0)
{
}

void Timer::update(const uint8_t machine_cycles)
{
    // M clock increments at 1/4 the T clock rate
    t_clock_ += machine_cycles * 4;

    // timer ticks occur at 1/16 the CPU cycles
    while (t_clock_ >= 16)
    {
        t_clock_ -= 16;
        tick();
    }
}

void Timer::tick()
{
    // base clock dividers
    static constexpr int freqs[] = {
        64, // 4   KHz
        1,  // 262 KHz (base)
        4,  // 65  KHz
        16  // 16  KHz
    };

    base_clock_++;
    div_clock_++;

    // do divider clock
    if (div_clock_ == 16)
    {
        divider_++;
        div_clock_ = 0;
    }

    // only if timer is enabled
    if (controller_ & 0x04)
    {
        // get frequency 
        auto freq = freqs[controller_ & 0x03];

        // increment counter
        while (base_clock_ >= freq)
        {
            base_clock_ -= freq;

            if (counter_ == 0xFF)
            {
                counter_ = modulo_;
                _mmu->gb->cpu.interupt(TIMER_INTERUPT);
            }
            else
            {
                counter_++;
            }
        }
    }
}