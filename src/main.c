//============================================================
// EFM8LB1 Unified Main (RAM OPTIMIZED)
//============================================================
// ~C51~

#include <EFM8LB1.h>
#include <stdio.h>
#include "variable.h"
#include "motor.h"
#include "auto_mode.h"
#include "collision.h"
#include "joystick.h"
#include "buzzer.h"
#include "scanner.h"

static unsigned char current_mode = MODE_MANUAL;
static char active_cmd = '0';
static bit last_blocked = 0;

unsigned char joy_cmd = 0;

static bit joy_wait_a = 0;
static bit joy_wait_b = 0;
static char joy_a = '0';

static bit path4_rx_mode = 0;
static unsigned char path4_rx_len = 0;

// MOVED TO XDATA RAM
static unsigned char xdata path4_rx_buf[8];

char _c51_external_startup(void)
{
    SFRPAGE = 0x00;
    WDTCN = 0xDE;
    WDTCN = 0xAD;

    VDM0CN = 0x80;
    RSTSRC = 0x02 | 0x04;

    SFRPAGE = 0x10;
    PFE0CN  = 0x20;
    SFRPAGE = 0x00;

    CLKSEL = 0x00;
    CLKSEL = 0x00;
    while ((CLKSEL & 0x80) == 0);

    CLKSEL = 0x03;
    CLKSEL = 0x03;
    while ((CLKSEL & 0x80) == 0);

    P0MDOUT |= 0x05;        
    P3MDOUT |= 0b00000111;  
    P2MDOUT |= 0b01000010;  
    BUZZER = 0;

    P2MDIN &= ~0x1C;
    P2SKIP |= 0x1C;

    P1MDOUT &= ~0x0C;
    P1 |= 0x0C;

    XBR0 = 0x01;            
    XBR1 = 0x00;
    XBR2 = 0x41;            

    SCON0 = 0x10;
    TMOD &= ~0xF0;
    TMOD |=  0x20;
    TH1 = 0x100 - ((SYSCLK/UART0_BAUD)/(2L*12L));
    TL1 = TH1;
    TR1 = 1;
    TI = 1;

    SFRPAGE = 0x10;
    TMR3CN1 |= 0b01100000;
    SFRPAGE = 0x00;

    return 0;
}

void UART1_Init(unsigned long baudrate)
{
    unsigned int reload = 0x10000L - (SYSCLK / (2L * 12L * baudrate));

    SFRPAGE = 0x20;
    SMOD1  = 0x0C;
    SCON1  = 0x10;
    SBCON1 = 0x00;
    SBRLH1 = (reload >> 8) & 0xFF;
    SBRLL1 = reload & 0xFF;
    RI1 = 0;
    TI1 = 1;
    SBCON1 |= 0x40;
    SFRPAGE = 0x00;
}

bit UART1_Available(void)
{
    bit b;
    SFRPAGE = 0x20;
    b = RI1;
    SFRPAGE = 0x00;
    return b;
}

char UART1_GetChar(void)
{
    char c;
    SFRPAGE = 0x20;
    while (!RI1);
    c = SBUF1;
    RI1 = 0;
    SCON1 &= 0x3F;
    SFRPAGE = 0x00;
    return c;
}

// ------------------------------------------------------------
// UART0 (PC) Helpers
// ------------------------------------------------------------
bit UART0_Available(void)
{
    return RI; 
}

char UART0_GetChar(void)
{
    char c;
    while (!RI);
    c = SBUF;
    RI = 0; 
    return c;
}

void putchar(char c)
{
    while (!TI);
    TI = 0;
    SBUF = c;
    if (c == '\n')
    {
        while (!TI);
        TI = 0;
        SBUF = '\r';
    }
}

void Timer3us(unsigned char us)
{
    unsigned char i;
    CKCON0 |= 0b01000000;
    TMR3RL = -(SYSCLK / 1000000L);
    TMR3   = TMR3RL;
    TMR3CN0 = 0x04;
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

static void manual_mode_task(void)
{
    int i;
    switch(active_cmd)
    {
        case '0': car_stop(); break;
        case '1': car_forward_pwm(DRIVE_SPEED); break;
        case '2': car_reverse_pwm(DRIVE_SPEED); break;
        case '3': car_turn_left_pwm(TURN_SPEED); break;
        case '4': car_turn_right_pwm(TURN_SPEED); break;
        case '5':
            for(i = 0; i < 185; i++) car_turn_right_pwm(TURN_SPEED);
            active_cmd = '0';
            break;
        case 'B':
        {
            unsigned char j;
            static const unsigned int code t[6] = {1000, 160, 500, 300, 400, 1300};

            for (j = 0; j < 6; j++)
            {
                car_turn_left();
                waitms(t[j]);

                car_turn_right();
                waitms(t[j]);
            }

            car_stop();
            active_cmd = '0';
            break;
        }
        default: car_stop(); break;
    }
}

static unsigned char decode_path4_char(char c)
{
    switch(c)
    {
        case 'S': return 0;
        case 'L': return 1;
        case 'R': return 2;
        default:  return 255;
    }
}

void main(void)
{
    char rx;
    bit blocked;

    UART1_Init(UART1_BAUD);
    car_stop();
    buzzer_init();
    collision_init();

    printf("Sys Ready\n"); 

    while (1)
    {
        // --------------------------------------------
        // PC COMMAND LISTENER (UART0 for Scanner)
        // --------------------------------------------
        if (UART0_Available()) 
        {
            char pc_rx = UART0_GetChar();
            if (pc_rx == 'A' || pc_rx == 'a') 
            {
                if (!is_scanner_active()) 
                {
                    printf("\r\n>>> ACK: RECEIVED 'A' FROM PC <<<\r\n");
                    scanner_toggle();
                }
            }
        }

        if (is_scanner_active())
        {
            scanner_run(); 
            continue; 
        }

        if (UART1_Available())
        {
            rx = UART1_GetChar();

            if ((rx != '\n') && (rx != '\r'))
            {
                if (rx == 'P')
                {
                    path4_rx_mode = 1;
                    path4_rx_len = 0;
                }
                else if (path4_rx_mode)
                {
                    if (rx == 'X')
                    {
                        path4_rx_mode = 0;
                        if (path4_rx_len > 0) auto_mode_set_path4(path4_rx_buf, path4_rx_len);
                    }
                    else if ((rx == 'L') || (rx == 'R') || (rx == 'S'))
                    {
                        if (path4_rx_len < 8) path4_rx_buf[path4_rx_len++] = decode_path4_char(rx);
                        else
                        {
                            path4_rx_mode = 0;
                            path4_rx_len = 0;
                        }
                    }
                }
                else if (rx == 'J')
                {
                    joy_wait_a = 1;
                    joy_wait_b = 0;
                }
                else if (joy_wait_a)
                {
                    if (rx >= '0' && rx <= '8')
                    {
                        joy_a = rx;
                        joy_wait_a = 0;
                        joy_wait_b = 1;
                    }
                    else { joy_wait_a = 0; joy_wait_b = 0; }
                }
                else if (joy_wait_b)
                {
                    if (rx >= '0' && rx <= '2')
                    {
                        joy_cmd = (joy_a - '0') * 10 + (rx - '0');
                        current_mode = MODE_JOYSTICK;
                    }
                    joy_wait_a = 0;
                    joy_wait_b = 0;
                }
                else
                {
                    if ((rx == '7') || (rx == '8') || (rx == '9') || (rx == '6'))
                    {
                        current_mode = MODE_AUTO;
                        auto_mode_init();
                        auto_mode_request_path(rx);
                    }
                    else if ((rx >= '0' && rx <= '5') || (rx == 'B'))
                    {
                        current_mode = MODE_MANUAL;
                        active_cmd = rx;
                    }
                }
            }
        }

        collision_task();
        buzzer_task();
        blocked = collision_blocked();

        if (blocked != last_blocked)
        {
            last_blocked = blocked; 
        }

        if (blocked)
        {
            if ((current_mode == MODE_MANUAL) && (active_cmd == '2')) manual_mode_task(); 
            else car_stop();
        }
        else
        {
            if (current_mode == MODE_MANUAL) manual_mode_task();
            else if (current_mode == MODE_JOYSTICK) joystick_mode_task();
            else if (current_mode == MODE_AUTO) auto_mode_task();
            else car_stop();
        }
    }
}