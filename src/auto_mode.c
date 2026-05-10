#include <EFM8LB1.h>
#include <stdio.h>
#include "variable.h"
#include "motor.h"
#include "auto_mode.h"

#define ACT_FORWARD  0
#define ACT_LEFT     1
#define ACT_RIGHT    2
#define ACT_STOP     3

#define AUTO_SUBMODE_DEFAULT  0
#define AUTO_SUBMODE_PATH1    1
#define AUTO_SUBMODE_PATH2    2
#define AUTO_SUBMODE_PATH3    3
#define AUTO_SUBMODE_PATH4    4
#define AUTO_SUBMODE_IDLE     5   

static volatile unsigned char path4_len = 0;

// MOVED TO XDATA RAM (Saves 8 bytes)
static unsigned char xdata path4_table[8];

void auto_mode_set_path4(unsigned char *src, unsigned char len)
{
    unsigned char i;
    if(len > 7) len = 7;   
    for(i = 0; i < len; i++) path4_table[i] = src[i];
    path4_table[len] = ACT_STOP;   
    path4_len = len + 1;
}

static const unsigned char code path1_table[8] = {
    ACT_FORWARD, ACT_LEFT, ACT_LEFT, ACT_FORWARD,
    ACT_RIGHT, ACT_LEFT, ACT_RIGHT, ACT_STOP
};

static const unsigned char code path2_table[7] = {
    ACT_LEFT, ACT_RIGHT, ACT_LEFT, ACT_RIGHT,
    ACT_FORWARD, ACT_FORWARD, ACT_STOP
};

static const unsigned char code path3_table[8] = {
    ACT_RIGHT, ACT_FORWARD, ACT_RIGHT, ACT_LEFT,
    ACT_RIGHT, ACT_LEFT, ACT_FORWARD, ACT_STOP
};

static volatile unsigned char currentcmd = 0;   
static volatile unsigned char intersection_lock = 0;
static volatile unsigned int  intersection_cooldown_timer = 0;
static volatile int track_bias = 0;
static volatile unsigned char recovery_used = 0;
static volatile unsigned char auto_submode = AUTO_SUBMODE_IDLE; 
static volatile unsigned char path_index = 0;

static void Timer3us_auto(unsigned char us)
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

static void Init_ADC(void)
{
    SFRPAGE = 0x00;
    ADC0CN0 = 0x00;
    ADC0CF0 = 0x20;
    ADC0CF1 = 0x00;
    ADC0CN0 |= 0x80;
    REF0CN  = 0x08;
}

static unsigned int Read_ADC(unsigned char channel)
{
    SFRPAGE = 0x00;
    ADC0MX = channel;
    Timer3us_auto(20);
    ADC0CN0 &= ~0x20;
    ADC0CN0 |= 0x10;
    while (!(ADC0CN0 & 0x20));
    ADC0CN0 &= ~0x20;
    return (ADC0H << 8) | ADC0L;
}

static void update_track_bias(signed char dir)
{
    if (dir > 0)
    {
        if (track_bias < TRACK_BIAS_LIMIT) track_bias++;
    }
    else if (dir < 0)
    {
        if (track_bias > -TRACK_BIAS_LIMIT) track_bias--;
    }
    else
    {
        if (track_bias > 0) track_bias--;
        else if (track_bias < 0) track_bias++;
    }
}

static void post_intersection_recovery(void)
{
    unsigned int k;
    if (recovery_used) return;

    if (track_bias >= TRACK_BIAS_TRIGGER)
    {
        for (k = 0; k < RECOVERY_PULSES; k++) car_curve_right_pwm(RECOVERY_SPEED);
        recovery_used = 1;
        track_bias = 0;
    }
    else if (track_bias <= -TRACK_BIAS_TRIGGER)
    {
        for (k = 0; k < RECOVERY_PULSES; k++) car_curve_left_pwm(RECOVERY_SPEED);
        recovery_used = 1;
        track_bias = 0;
    }
}

static void linetrack(void)
{
    unsigned int val_left  = Read_ADC(ADC_LEFT_CH) + 50;
    unsigned int val_right = Read_ADC(ADC_RIGHT_CH);
    int field_difference = (int)val_left - (int)val_right;

    if (field_difference > THRESH_DRIFT)
    {
        update_track_bias(+1);
        car_curve_left_pwm(AUTO_ADJUST_SPEED);
    }
    else if (field_difference < -THRESH_DRIFT)
    {
        update_track_bias(-1);
        car_curve_right_pwm(AUTO_ADJUST_SPEED);
    }
    else
    {
        update_track_bias(0);
        car_forward_pwm(AUTO_DRIVE_SPEED);
    }
}

static void linetrack2(void)
{
    unsigned int i;
    for(i = 0; i < 30; i++)
    {
        unsigned int val_left  = Read_ADC(ADC_LEFT_CH);
        unsigned int val_right = Read_ADC(ADC_RIGHT_CH);
        int field_difference = (int)val_left - (int)val_right;

        if (field_difference > SENSITIVITY)
        {
            update_track_bias(+1);
            car_curve_left_pwm(10);
        }
        else if (field_difference < -SENSITIVITY)
        {
            update_track_bias(-1);
            car_curve_right_pwm(10);
        }
        else
        {
            update_track_bias(0);
            car_forward_pwm(10);
        }
    }
}

static void turncar(int leftright)
{
    car_stop();
    if (leftright == 0)
    {
        car_turn_left();
        waitms(480);
    }
    else if (leftright == 1)
    {
        car_turn_right();
        waitms(480);
    }
    car_stop();
    linetrack2();
}

static void reset_intersection_runtime(void)
{
    intersection_lock = 0;
    intersection_cooldown_timer = 0;
    track_bias = 0;
    recovery_used = 0;
}

static void finish_path_and_stop(void)
{
    auto_submode = AUTO_SUBMODE_IDLE;
    path_index = 0;
    reset_intersection_runtime();
    car_stop();
}

static unsigned char get_path_length(unsigned char submode)
{
    switch(submode)
    {
        case AUTO_SUBMODE_PATH1: return 8;
        case AUTO_SUBMODE_PATH2: return 7;
        case AUTO_SUBMODE_PATH3: return 8;
        case AUTO_SUBMODE_PATH4: return path4_len;
        default:                 return 0;
    }
}

static unsigned char get_path_action(unsigned char submode, unsigned char idx)
{
    switch(submode)
    {
        case AUTO_SUBMODE_PATH1: return path1_table[idx];
        case AUTO_SUBMODE_PATH2: return path2_table[idx];
        case AUTO_SUBMODE_PATH3: return path3_table[idx];
        case AUTO_SUBMODE_PATH4: return path4_table[idx];
        default:                 return ACT_STOP;
    }
}

static void execute_action(unsigned char action)
{
    int i;
    switch(action)
    {
        case ACT_FORWARD:
            car_forward();
            car_stop();
            linetrack2();
            break;
        case ACT_LEFT:
            car_forward();
            turncar(0);
            break;
        case ACT_RIGHT:
            car_forward();
            turncar(1);
            break;
        case ACT_STOP:
            car_stop();
            finish_path_and_stop(); 
            return;
        default:
            break;
    }

    for(i = 0; i < 25; i++) car_forward_pwm(AUTO_DRIVE_SPEED);
    car_stop();
}

static void handle_default_intersection(void)
{
    switch (currentcmd)
    {
        case 0:
            execute_action(ACT_FORWARD);
            currentcmd = 1;
            break;
        case 1:
            execute_action(ACT_RIGHT);
            currentcmd = 2;
            break;
        case 2:
            execute_action(ACT_LEFT);
            currentcmd = 0;
            break;
        default:
            currentcmd = 0;
            break;
    }
}

static void handle_path_intersection(void)
{
    unsigned char len = get_path_length(auto_submode);
    unsigned char action;

    if(path_index >= len)
    {
        finish_path_and_stop();
        return;
    }

    action = get_path_action(auto_submode, path_index);
    execute_action(action);

    if(auto_submode != AUTO_SUBMODE_IDLE)
    {
        path_index++;
        if(path_index >= len) finish_path_and_stop();
    }
}

void auto_mode_init(void)
{
    Init_ADC();
    currentcmd = 0;
    auto_submode = AUTO_SUBMODE_IDLE; 
    path_index = 0;
    reset_intersection_runtime();
    car_stop();
    waitms(200);
}

void auto_mode_request_path(char cmd)
{
    car_stop();
    waitms(50);
    switch(cmd)
    {
	    case '6':
	    if(path4_len > 0)
	    {
	        auto_submode = AUTO_SUBMODE_PATH4;
	        path_index = 0;
	        reset_intersection_runtime();
	    }
	    break;
        case '7':
            auto_submode = AUTO_SUBMODE_PATH1;
            path_index = 0;
            reset_intersection_runtime();
            break;
        case '8':
            auto_submode = AUTO_SUBMODE_PATH2;
            path_index = 0;
            reset_intersection_runtime();
            break;
        case '9':
            auto_submode = AUTO_SUBMODE_PATH3;
            path_index = 0;
            reset_intersection_runtime();
            break;
        default:
            break;
    }
}

void auto_mode_task(void)
{
    unsigned int val_mid;

    if (auto_submode == AUTO_SUBMODE_IDLE)
    {
        car_stop();
        return;
    }

    val_mid = Read_ADC(ADC_MIDDLE_CH);

    if (intersection_lock)
    {
        if (intersection_cooldown_timer > 0) intersection_cooldown_timer--;
        else intersection_lock = 0;
    }

    if (intersection_lock && (val_mid > MID_RETRIGGER_THRESHOLD)) post_intersection_recovery();

    linetrack();

    if ((intersection_lock == 0) && (val_mid > THRESH_INTERSECTION))
    {
        intersection_lock = 1;
        intersection_cooldown_timer = INTERSECTION_COOLDOWN_MS;
        recovery_used = 0;
        track_bias = 0;

        if(auto_submode == AUTO_SUBMODE_DEFAULT) handle_default_intersection();
        else handle_path_intersection();
    }
}