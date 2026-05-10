#ifndef VARIABLE_H
#define VARIABLE_H

#include <EFM8LB1.h>

// ---------------- System ----------------
#define SYSCLK       72000000L
#define UART0_BAUD   115200L
#define UART1_BAUD   1200L

// ---------------- Motor Pin Assignments ----------------
#define M1_A   P3_2
#define M1_B   P3_1
#define M2_A   P3_0
#define M2_B   P2_6


// ---------------- Buzzer Pin ----------------
#define BUZZER P2_1

// ---------------- Collision Sensor Pins ----------------
#define CAL_BUTTON P3_7

// ---------------- Manual Driving Speeds ----------------
#define DRIVE_SPEED  40
#define TURN_SPEED   35

// ---------------- Auto ADC Channels ----------------
#define ADC_LEFT_CH    0x0F   // P2.2
#define ADC_RIGHT_CH   0x10   // P2.3
#define ADC_MIDDLE_CH  0x11   // P2.4

// ---------------- Auto Thresholds ----------------
#define THRESH_INTERSECTION      650
#define THRESH_SIDE_LOW          600
#define THRESH_WIRE_DETECT       1100
#define THRESH_DEFAULT           400
#define THRESH_DRIFT             100
#define SENSITIVITY              1000

// ---------------- Auto Speeds ----------------
#define AUTO_DRIVE_SPEED         75  //65
#define AUTO_TURN_SPEED          50   //45
#define AUTO_ADJUST_SPEED        50   //35  those promising

// ---------------- Auto Timing ----------------
#define INTERSECTION_COOLDOWN_MS  500
#define MID_RETRIGGER_THRESHOLD   900
#define TRACK_BIAS_LIMIT          8
#define TRACK_BIAS_TRIGGER        5
#define RECOVERY_SPEED            50
#define RECOVERY_PULSES           150

// ---------------- Mode ----------------
#define MODE_MANUAL  0
#define MODE_AUTO  1
#define MODE_JOYSTICK 2

// ---------------- Collision Thresholds ----------------
#define COLLISION_WARN_MM          200
#define COLLISION_STOP_MM          100
#define COLLISION_RELEASE_MM        30
#define COLLISION_CAL_REFERENCE_MM 250

// ---------------- Buzzer Tone / Pattern ----------------
#define BUZZER_TONE_HZ             1000
#define BUZZER_BEEP_ON_MS            60
#define BUZZER_BEEP_PERIOD_SLOW     360
#define BUZZER_BEEP_PERIOD_FAST      90

// ---------------- Shared delay ----------------
void Timer3us(unsigned char us);
void waitms(unsigned int ms);

#endif
