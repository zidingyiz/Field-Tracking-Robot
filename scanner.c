#include <EFM8LB1.h>
#include <stdio.h>
#include "variable.h"
#include "motor.h"
#include "scanner.h"

// ---------------- Scan Parameters ----------------
#define TOTAL_COLUMNS          8
#define STEPS_PER_COLUMN       185
#define STEP_SAMPLE_INTERVAL   20

#define SCAN_DRIVE_SPEED         70
#define SCAN_TURN_RIGHT_SPEED    44
#define SCAN_TURN_LEFT_SPEED     46
#define SCAN_RIGHT_WHEEL_OFFSET  5

#define TOP_UTURN_PULSES       240
#define BOTTOM_UTURN_PULSES    243

#define TURN_SETTLE_MS         150
#define STARTUP_WAIT_MS        1000
#define COLUMN_SETTLE_MS       100

#define ROWS_PER_COLUMN        (STEPS_PER_COLUMN)

static bit scanner_active = 0;
static unsigned long sample_index = 0;

void scanner_toggle(void)
{
    scanner_active = 1; // Force active
}

unsigned char is_scanner_active(void)
{
    return scanner_active;
}

// ---------------- Dedicated Scanner Helpers ----------------
static unsigned int Read_ADC_scan(unsigned char channel)
{
    SFRPAGE = 0x00;
    ADC0MX = channel;
    Timer3us(20); // Uses standard delay from main.c
    ADC0CN0 &= ~0x20;
    ADC0CN0 |= 0x10;
    while (!(ADC0CN0 & 0x20));
    ADC0CN0 &= ~0x20;
    return (ADC0H << 8) | ADC0L;
}

static unsigned int clamp_speed(int speed)
{
    if (speed < 0) return 0;
    if (speed > 100) return 100;
    return (unsigned int)speed;
}

// Custom PWM to drive straight with motor offset
static void scanner_car_forward_pwm_lr(unsigned int right_speed_percent, unsigned int left_speed_percent)
{
    unsigned int right_on = (20 * right_speed_percent) / 100;
    unsigned int left_on  = (20 * left_speed_percent) / 100;
    unsigned int min_on = (right_on < left_on) ? right_on : left_on;
    unsigned int max_on = (right_on > left_on) ? right_on : left_on;
    unsigned int off_time;

    if (right_on > 0) { M1_A = 0; M1_B = 1; } else { M1_A = 1; M1_B = 1; }
    if (left_on > 0)  { M2_A = 0; M2_B = 1; } else { M2_A = 1; M2_B = 1; }

    if (min_on > 0) waitms(min_on);

    if (right_on > left_on)
    {
        M2_A = 1; M2_B = 1; // Left stop
        if ((right_on - left_on) > 0) waitms(right_on - left_on);
    }
    else if (left_on > right_on)
    {
        M1_A = 1; M1_B = 1; // Right stop
        if ((left_on - right_on) > 0) waitms(left_on - right_on);
    }

    car_stop();
    off_time = 20 - max_on;
    if (off_time > 0) waitms(off_time);
}


// Custom PWM for scanner-only left turn
static void scanner_turn_left_pwm(unsigned int speed_percent)
{
    unsigned int on_time  = (20 * speed_percent) / 100;
    unsigned int off_time = 20 - on_time;

    // scanner-specific left turn:
    // right wheel stop, left wheel forward
    M1_A = 1; M1_B = 1;   // motor1 stop
    M2_A = 0; M2_B = 1;   // motor2 forward

    if (on_time > 0) waitms(on_time);

    // stop both motors
    M1_A = 1; M1_B = 1;
    M2_A = 1; M2_B = 1;

    if (off_time > 0) waitms(off_time);
}

// Custom PWM for scanner-only right turn
static void scanner_turn_right_pwm(unsigned int speed_percent)
{
    unsigned int on_time  = (20 * speed_percent) / 100;
    unsigned int off_time = 20 - on_time;

    // scanner-specific right turn:
    // right wheel forward, left wheel stop
    M1_A = 0; M1_B = 1;   // motor1 forward
    M2_A = 1; M2_B = 1;   // motor2 stop

    if (on_time > 0) waitms(on_time);

    // stop both motors
    M1_A = 1; M1_B = 1;
    M2_A = 1; M2_B = 1;

    if (off_time > 0) waitms(off_time);
}
// ---------------- Core Logic ----------------
static unsigned int compute_plot_row(unsigned int step, unsigned char direction_flag)
{
    if (step >= ROWS_PER_COLUMN) step = ROWS_PER_COLUMN - 1;
    if (direction_flag == 0) return step;
    else return (ROWS_PER_COLUMN - 1) - step;
}

static void log_sample(unsigned int column, unsigned int step, unsigned char direction_flag)
{
    unsigned int val_left  = Read_ADC_scan(ADC_LEFT_CH);
    unsigned int val_mid   = Read_ADC_scan(ADC_MIDDLE_CH);
    unsigned int val_right = Read_ADC_scan(ADC_RIGHT_CH);
    unsigned int plot_row  = compute_plot_row(step, direction_flag);

    printf("%lu,%u,%u,%u,%u,%u,%u,%u\r\n", sample_index, column, step, plot_row, direction_flag, val_left, val_mid, val_right);
    sample_index++;
}

// Checks UART0 for the 'A' Abort command from the PC
static bit check_abort(void)
{
    if (RI) 
    {
        char c = SBUF;
        RI = 0;
        if (c == 'A' || c == 'a') 
        {
            scanner_active = 0;
            car_stop();
            printf("\r\n#EVENT,SCAN_ABORTED\r\n");
            return 1;
        }
    }
    return 0;
}

// ---------------- The Main FSM ----------------
void scanner_run(void)
{
    unsigned int current_column = 0;
    unsigned int current_step_in_column = 0;
    unsigned int i;
    unsigned char scan_state = 0; // 0=UP, 1=TURN_TOP, 2=DOWN, 3=TURN_BOT

    SFRPAGE = 0x00;
    ADC0CN0 = 0x00;
    ADC0CF0 = 0x20;
    ADC0CF1 = 0x00;
    ADC0CN0 |= 0x80;
    REF0CN = 0x08;

    car_stop();
    waitms(STARTUP_WAIT_MS);
    
    printf("\r\n#INFO,SCAN_MODE_START\r\n");
    printf("#CFG,TOTAL_COLUMNS,%u\r\n", TOTAL_COLUMNS);
    printf("#CFG,STEPS_PER_COLUMN,%u\r\n", STEPS_PER_COLUMN);
    printf("#CFG,STEP_SAMPLE_INTERVAL,%u\r\n", STEP_SAMPLE_INTERVAL);
    printf("#CFG,ROWS_PER_COLUMN,%u\r\n", ROWS_PER_COLUMN);
    printf("#DATA,sample,column,raw_step,plot_row,direction,left,middle,right\r\n");

    sample_index = 0;

    while (scanner_active)
    {
        if (check_abort()) return; // Instant Bailout!

        switch (scan_state)
        {
            case 0: // SCAN UP
                if (current_step_in_column < STEPS_PER_COLUMN)
                {
                    scanner_car_forward_pwm_lr(clamp_speed(SCAN_DRIVE_SPEED - SCAN_RIGHT_WHEEL_OFFSET), clamp_speed(SCAN_DRIVE_SPEED));
                    log_sample(current_column, current_step_in_column, 0);
                    current_step_in_column++;
                }
                else
                {
                    car_stop();
                    waitms(COLUMN_SETTLE_MS);
                    if (current_column >= (TOTAL_COLUMNS - 1)) scanner_active = 0;
                    else scan_state = 1;
                }
                break;

            case 1: // TURN TOP
                printf("#EVENT,TOP_UTURN_START,%u\r\n", current_column);
                for (i = 0; i < TOP_UTURN_PULSES; i++)
                {
                    if (check_abort()) return; // Abort mid-turn if requested
                    scanner_turn_right_pwm(SCAN_TURN_RIGHT_SPEED);
                }
                car_stop();
                waitms(TURN_SETTLE_MS);
                printf("#EVENT,TOP_UTURN_DONE,%u\r\n", current_column);
                current_column++;
                current_step_in_column = 0;
                scan_state = 2;
                break;

            case 2: // SCAN DOWN
                if (current_step_in_column < STEPS_PER_COLUMN)
                {
                    scanner_car_forward_pwm_lr(clamp_speed(SCAN_DRIVE_SPEED - SCAN_RIGHT_WHEEL_OFFSET), clamp_speed(SCAN_DRIVE_SPEED));
                    log_sample(current_column, current_step_in_column, 1);
                    current_step_in_column++;
                }
                else
                {
                    car_stop();
                    waitms(COLUMN_SETTLE_MS);
                    if (current_column >= (TOTAL_COLUMNS - 1)) scanner_active = 0;
                    else scan_state = 3;
                }
                break;

            case 3: // TURN BOTTOM
                printf("#EVENT,BOTTOM_UTURN_START,%u\r\n", current_column);
                for (i = 0; i < BOTTOM_UTURN_PULSES; i++)
                {
                    if (check_abort()) return;
                    scanner_turn_left_pwm(SCAN_TURN_LEFT_SPEED);
                }
                car_stop();
                waitms(TURN_SETTLE_MS);
                printf("#EVENT,BOTTOM_UTURN_DONE,%u\r\n", current_column);
                current_column++;
                current_step_in_column = 0;
                scan_state = 0;
                break;
        }
    }

    car_stop();
    printf("#INFO,SCAN_COMPLETE\r\n");
}