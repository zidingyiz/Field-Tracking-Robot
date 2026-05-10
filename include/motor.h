#ifndef MOTOR_H
#define MOTOR_H

#include "variable.h"

// Raw motor control
void motor1_forward(void);
void motor2_forward(void);
void motor1_reverse(void);
void motor2_reverse(void);
void motor1_stop(void);
void motor2_stop(void);

// Basic car control
void car_stop(void);
void car_forward(void);
void car_reverse(void);
void car_turn_left(void);
void car_turn_right(void);
void car_reverse_right(void);
void car_reverse_left(void);

// Extra curve control for auto mode
void car_curve_left(void);
void car_curve_right(void);

// PWM control
void car_forward_pwm(unsigned int speed_percent);
void car_reverse_pwm(unsigned int speed_percent);
void car_turn_left_pwm(unsigned int speed_percent);
void car_turn_right_pwm(unsigned int speed_percent);
void car_curve_left_pwm(unsigned int speed_percent);
void car_curve_right_pwm(unsigned int speed_percent);
void car_reverse_right_pwm(unsigned int speed_percent);
void car_reverse_left_pwm(unsigned int speed_percent);

#endif