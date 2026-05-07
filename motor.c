#include <EFM8LB1.h>
#include <stdio.h>
#include "variable.h"
#include "motor.h"

//============================================================
// Raw Motor Control
//============================================================
void motor1_forward(void) { M1_A = 0; M1_B = 1; }
void motor2_forward(void) { M2_A = 0; M2_B = 1; }
void motor1_reverse(void) { M1_A = 1; M1_B = 0; }
void motor2_reverse(void) { M2_A = 1; M2_B = 0; }
void motor1_stop(void)    { M1_A = 1; M1_B = 1; }
void motor2_stop(void)    { M2_A = 1; M2_B = 1; }

void car_stop(void)       { motor1_stop(); motor2_stop(); }
void car_forward(void)    { motor1_forward(); motor2_forward(); }
void car_reverse(void)    { motor1_reverse(); motor2_reverse(); }
void car_turn_left(void)  { motor1_forward(); motor2_reverse(); }
void car_turn_right(void) { motor1_reverse(); motor2_forward(); }
void car_reverse_right(void) { motor1_reverse(); motor2_stop(); }
void car_reverse_left(void)  { motor1_stop();    motor2_reverse(); }

// Used by auto mode for smooth correction
void car_curve_left(void)  { motor1_stop();    motor2_forward(); }
void car_curve_right(void) { motor1_forward(); motor2_stop();    }


void car_reverse_right_pwm(unsigned int speed_percent)
{
    unsigned int on_time = (20 * speed_percent) / 100;
    unsigned int off_time = 20 - on_time;

    car_reverse_right();
    if(on_time > 0) waitms(on_time);
    car_stop();
    if(off_time > 0) waitms(off_time);
}

void car_reverse_left_pwm(unsigned int speed_percent)
{
    unsigned int on_time = (20 * speed_percent) / 100;
    unsigned int off_time = 20 - on_time;

    car_reverse_left();
    if(on_time > 0) waitms(on_time);
    car_stop();
    if(off_time > 0) waitms(off_time);
}

//============================================================
// Software PWM Motor Control (Period = 20 ms)
//============================================================
void car_forward_pwm(unsigned int speed_percent)
{
    unsigned int on_time = (20 * speed_percent) / 100;
    unsigned int off_time = 20 - on_time;

    car_forward();
    if(on_time > 0) waitms(on_time);
    car_stop();
    if(off_time > 0) waitms(off_time);
}

void car_reverse_pwm(unsigned int speed_percent)
{
    unsigned int on_time = (20 * speed_percent) / 100;
    unsigned int off_time = 20 - on_time;

    car_reverse();
    if(on_time > 0) waitms(on_time);
    car_stop();
    if(off_time > 0) waitms(off_time);
}

void car_turn_left_pwm(unsigned int speed_percent)
{
    unsigned int on_time = (20 * speed_percent) / 100;
    unsigned int off_time = 20 - on_time;

    car_turn_left();
    if(on_time > 0) waitms(on_time);
    car_stop();
    if(off_time > 0) waitms(off_time);
}

void car_turn_right_pwm(unsigned int speed_percent)
{
    unsigned int on_time = (20 * speed_percent) / 100;
    unsigned int off_time = 20 - on_time;

    car_turn_right();
    if(on_time > 0) waitms(on_time);
    car_stop();
    if(off_time > 0) waitms(off_time);
}

void car_curve_left_pwm(unsigned int speed_percent)
{
    unsigned int on_time = (20 * speed_percent) / 100;
    unsigned int off_time = 20 - on_time;

    car_curve_left();
    if(on_time > 0) waitms(on_time);
    car_stop();
    if(off_time > 0) waitms(off_time);
}

void car_curve_right_pwm(unsigned int speed_percent)
{
    unsigned int on_time = (20 * speed_percent) / 100;
    unsigned int off_time = 20 - on_time;

    car_curve_right();
    if(on_time > 0) waitms(on_time);
    car_stop();
    if(off_time > 0) waitms(off_time);
}