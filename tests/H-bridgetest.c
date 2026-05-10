//============================================================
// EFM8LB1 H-Bridge Test
// Goal: match the "CW Rotation" slide state
//
// Left control  = ACTIVE (output low)
// Right control = INACTIVE (output high)
//
// Because optocoupler input is wired as:
// 3.3V -> RD -> LED -> MCU pin
// so the MCU must SINK current to turn it on.
//
// Pin assignment:
//   LEFT_SIG  -> P1.6
//   RIGHT_SIG -> P1.7
//
// Target: EFM8LB1 (CrossIDE C51)
//============================================================
// ~C51~

#include <EFM8LB1.h>

#define SYSCLK 72000000L

// ---------------- Pin assignment ----------------
#define LEFT_SIG   P1_6
#define RIGHT_SIG  P1_7

//============================================================
// Startup
//============================================================
char _c51_external_startup (void)
{
    SFRPAGE = 0x00;

    // Disable watchdog
    WDTCN = 0xDE;
    WDTCN = 0xAD;

    // Enable VDD monitor
    VDM0CN |= 0x80;
    RSTSRC = 0x02;

#if (SYSCLK == 48000000L)
    SFRPAGE = 0x10;
    PFE0CN  = 0x10;
    SFRPAGE = 0x00;
#elif (SYSCLK == 72000000L)
    SFRPAGE = 0x10;
    PFE0CN  = 0x20;
    SFRPAGE = 0x00;
#endif

#if (SYSCLK == 12250000L)
    CLKSEL = 0x10; CLKSEL = 0x10; while ((CLKSEL & 0x80) == 0);
#elif (SYSCLK == 24500000L)
    CLKSEL = 0x00; CLKSEL = 0x00; while ((CLKSEL & 0x80) == 0);
#elif (SYSCLK == 48000000L)
    CLKSEL = 0x00; CLKSEL = 0x00; while ((CLKSEL & 0x80) == 0);
    CLKSEL = 0x07; CLKSEL = 0x07; while ((CLKSEL & 0x80) == 0);
#elif (SYSCLK == 72000000L)
    CLKSEL = 0x00; CLKSEL = 0x00; while ((CLKSEL & 0x80) == 0);
    CLKSEL = 0x03; CLKSEL = 0x03; while ((CLKSEL & 0x80) == 0);
#else
#error SYSCLK must be 12250000L, 24500000L, 48000000L, or 72000000L
#endif

    // Make P1.6 and P1.7 push-pull outputs
    P1MDOUT |= 0b11000000;

    // Enable crossbar, weak pull-ups
    XBR2 = 0x40;

    return 0;
}

//============================================================
// Delay using Timer3
//============================================================
void Timer3us(unsigned char us)
{
    unsigned char i;

    CKCON0 |= 0b01000000;          // Timer3 uses SYSCLK
    TMR3RL = -(SYSCLK / 1000000L); // reload for 1 us
    TMR3   = TMR3RL;
    TMR3CN0 = 0x04;                // start Timer3

    for(i = 0; i < us; i++)
    {
        while(!(TMR3CN0 & 0x80));
        TMR3CN0 &= ~(0x80);
    }

    TMR3CN0 = 0;
}

void waitms(unsigned int ms)
{
    unsigned int j;
    unsigned char k;

    for(j = 0; j < ms; j++)
        for(k = 0; k < 4; k++)
            Timer3us(250);
}

//============================================================
// H-Bridge control
//============================================================
void motor_off(void)
{
    // both optocouplers OFF
    LEFT_SIG  = 1;
    RIGHT_SIG = 1;
}

void motor_cw(void)
{
    // Match your second slide:
    // left side active, right side inactive
    LEFT_SIG  = 0;   // optocoupler ON
    RIGHT_SIG = 1;   // optocoupler OFF
}

void motor_ccw(void)
{
    // opposite direction
    LEFT_SIG  = 1;
    RIGHT_SIG = 0;
}

//============================================================
// Main
//============================================================
void main(void)
{
    // Safety: start from OFF
    motor_off();
    waitms(500);

    // Apply the exact state from the slide
    motor_cw();

    while(1)
    {
        // keep running in this direction
    }
}