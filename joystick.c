#include "motor.h"
#include "variable.h"
#include "joystick.h"

extern unsigned char joy_cmd;

void joystick_mode_task(void)
{
    switch (joy_cmd)
	{
    case 00:  car_stop(); break;
    case 11: car_forward_pwm(50); break;
    case 12: car_forward_pwm(80); break;
    case 21: car_reverse_pwm(50); break;
    case 22: car_reverse_pwm(80); break;
    case 31: car_turn_left_pwm(30); break;
    case 32: car_turn_left_pwm(50); break;
    case 41: car_turn_right_pwm(30); break;
    case 42: car_turn_right_pwm(50); break;
    case 51: car_curve_right_pwm(30); break;
	case 52: car_curve_right_pwm(30);break;
	case 61: car_curve_left_pwm(30); break;
	case 62: car_curve_left_pwm(30);break;    
	case 71: car_reverse_right_pwm(30); break;
	case 72: car_reverse_right_pwm(30); break;    
	case 81: car_reverse_left_pwm(30); break;
	case 82: car_reverse_left_pwm(30); break;   
    default: car_stop(); break;
	}

}