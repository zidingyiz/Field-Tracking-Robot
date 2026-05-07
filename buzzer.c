#include <EFM8LB1.h>
#include "variable.h"
#include "collision.h"
#include "vl53l0x.h"
#include "buzzer.h"

static volatile bit tone_enable = 0;
static volatile unsigned long ms_ticks = 0;
static volatile unsigned char tick_div4 = 0;

//------------------------------------------------------------
// Timer2 ISR
// Interrupt rate = 2 * BUZZER_TONE_HZ
// Output toggles each interrupt, so buzzer tone = BUZZER_TONE_HZ
//------------------------------------------------------------
void Timer2_ISR(void) interrupt 5
{
    TF2H = 0; // Clear Timer2 interrupt flag

    if (tone_enable)
    {
        BUZZER = !BUZZER;
    }
    else
    {
        BUZZER = 0;
    }

    tick_div4++;
    if (tick_div4 >= 4)
    {
        tick_div4 = 0;
        ms_ticks++;
    }
}

//------------------------------------------------------------
// Timer2 init for buzzer tone generation + 1ms time base
//------------------------------------------------------------
void buzzer_init(void)
{
    unsigned int reload;

    tone_enable = 0;
    ms_ticks = 0;
    tick_div4 = 0;
    BUZZER = 0;

    // Timer2 overflow frequency = 2 * tone frequency
    // Use SYSCLK/12
    reload = 0x10000L - (SYSCLK / 12L / (2L * BUZZER_TONE_HZ));

    TMR2CN0 = 0x00;
    CKCON0 &= ~(0b00110000);   // Timer2 uses SYSCLK/12
    TMR2RL = reload;
    TMR2   = reload;

    ET2 = 1;
    TR2 = 1;
    EA  = 1;
}

//------------------------------------------------------------
// Distance-based buzzer pattern
//------------------------------------------------------------
void buzzer_task(void)
{
    static unsigned char mode = 0;     // 0=off, 1=pulsed, 2=continuous
    static bit beep_on = 0;
    static unsigned long next_change = 0;

    int distance;
    unsigned int period;
    unsigned long now;

    distance = collision_get_distance_mm();
    now = ms_ticks;

    // Invalid / out of range / too far => silent
    if ((distance <= 0) ||
        (distance == VL53L0X_OUT_OF_RANGE) ||
        (distance > COLLISION_WARN_MM))
    {
        tone_enable = 0;
        BUZZER = 0;
        mode = 0;
        beep_on = 0;
        next_change = now;
        return;
    }

    // 100 mm and closer => continuous tone
    if (distance <= COLLISION_STOP_MM)
    {
        tone_enable = 1;
        mode = 2;
        return;
    }

    // Between 100 mm and 200 mm => pulsed tone, faster as it gets closer
    period = BUZZER_BEEP_PERIOD_FAST +
            ((unsigned int)(distance - COLLISION_STOP_MM) *
            (BUZZER_BEEP_PERIOD_SLOW - BUZZER_BEEP_PERIOD_FAST)) /
            (COLLISION_WARN_MM - COLLISION_STOP_MM);

    if (mode != 1)
    {
        mode = 1;
        beep_on = 1;
        tone_enable = 1;
        next_change = now + BUZZER_BEEP_ON_MS;
        return;
    }

    if ((long)(now - next_change) >= 0)
    {
        if (beep_on)
        {
            beep_on = 0;
            tone_enable = 0;
            next_change = now + (period - BUZZER_BEEP_ON_MS);
        }
        else
        {
            beep_on = 1;
            tone_enable = 1;
            next_change = now + BUZZER_BEEP_ON_MS;
        }
    }
}