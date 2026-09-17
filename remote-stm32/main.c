#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../Common/Include/stm32l051xx.h"
#include "../Common/Include/serial.h"

#define MAXBUFFER 64
typedef struct tagComBuffer{
    unsigned char Buffer[MAXBUFFER];
    unsigned Head, Tail;
    unsigned Count;
} ComBuffer;

extern ComBuffer ComRXBuffer;
unsigned int GetBufCount(ComBuffer *Buf);

#define F_CPU 32000000L

void wait_1ms(void)
{
    SysTick->LOAD = (F_CPU/1000L) - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while((SysTick->CTRL & BIT16)==0);
    SysTick->CTRL = 0x00;
}

void delayms(int len)
{
    while(len--) wait_1ms();
}

int map_val(int x, int in_min, int in_max, int out_min, int out_max)
{
    if(x < in_min) x = in_min;
    if(x > in_max) x = in_max;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void Joystick_Init(void)
{
    RCC->IOPENR |= BIT0;   // GPIOA
    RCC->APB2ENR |= BIT9;  // ADC1

    // PA0, PA1 = analog
    GPIOA->MODER |= (3 << (0 * 2)) | (3 << (1 * 2));

    // PA2 = digital input with pull-up (Joystick Push Button)
    GPIOA->MODER &= ~(3 << (2 * 2));
    GPIOA->PUPDR &= ~(3 << (2 * 2));
    GPIOA->PUPDR |=  (1 << (2 * 2));

    if((ADC1->CR & BIT0) == 0)
    {
        ADC1->CR |= BIT28;
        delayms(10);

        ADC1->CR |= BIT31;
        while((ADC1->ISR & BIT11) == 0);
        ADC1->ISR |= BIT11;

        ADC1->ISR |= BIT0;
        ADC1->CR |= BIT0;
        while((ADC1->ISR & BIT0) == 0);
    }
}

unsigned int ADC_Read(unsigned char channel)
{
    ADC1->CHSELR = (1 << channel);
    ADC1->CR |= BIT2;
    while((ADC1->ISR & BIT2) == 0);
    return ADC1->DR;
}

void Buttons_Init(void)
{
    RCC->IOPENR |= BIT0; // GPIOAEN
    RCC->IOPENR |= BIT1; // GPIOBEN

    // GPIOA buttons: PA3, PA4, PA5, PA6, PA7, PA11, PA12, PA13
    GPIOA->MODER &= ~(
        (0x3<<(3*2))  | (0x3<<(4*2))  | (0x3<<(5*2))  | (0x3<<(6*2))  |
        (0x3<<(7*2))  | (0x3<<(11*2)) | (0x3<<(12*2)) | (0x3<<(13*2))
    );

    GPIOA->PUPDR &= ~(
        (0x3<<(3*2))  | (0x3<<(4*2))  | (0x3<<(5*2))  | (0x3<<(6*2))  |
        (0x3<<(7*2))  | (0x3<<(11*2)) | (0x3<<(12*2)) | (0x3<<(13*2))
    );

    GPIOA->PUPDR |= (
        (0x1<<(3*2))  | (0x1<<(4*2))  | (0x1<<(5*2))  | (0x1<<(6*2))  |
        (0x1<<(7*2))  | (0x1<<(11*2)) | (0x1<<(12*2)) | (0x1<<(13*2))
    );

    // GPIOB buttons: PB0, PB1
    GPIOB->MODER &= ~((0x3<<(0*2)) | (0x3<<(1*2)));
    GPIOB->PUPDR &= ~((0x3<<(0*2)) | (0x3<<(1*2)));
    GPIOB->PUPDR |=  ((0x1<<(0*2)) | (0x1<<(1*2)));
}

void IR_Carrier_Init(void)
{
    RCC->IOPENR  |= BIT0; // GPIOA clock
    RCC->APB1ENR |= BIT0; // TIM2 clock

    GPIOA->MODER &= ~(0x3<<(15*2));
    GPIOA->MODER |=  (0x1<<(15*2));
    GPIOA->OTYPER &= ~BIT15;
    GPIOA->OSPEEDR |= (0x3<<(15*2));

    GPIOA->PUPDR &= ~(0x3<<(15*2));
    GPIOA->PUPDR |=  (0x2<<(15*2));

    GPIOA->ODR &= ~BIT15;

    GPIOA->MODER &= ~(0x3<<(15*2));
    GPIOA->MODER |=  (0x2<<(15*2));

    GPIOA->AFR[1] &= ~(0xF<<28);
    GPIOA->AFR[1] |=  (0x5<<28);

    TIM2->CR1  = 0x0000;
    TIM2->PSC  = 0;
    TIM2->ARR  = 841;
    TIM2->CCR1 = 421;
    TIM2->CNT  = 0;

    TIM2->CCMR1 &= ~((0x3 << 0) | (0x1 << 3) | (0x7 << 4));
    TIM2->CCMR1 |=  ((0x1 << 3) | (0x6 << 4));

    TIM2->CR1 |= BIT7;
    TIM2->EGR = BIT0;
    TIM2->CCER |= BIT0;
    TIM2->CR1 |= BIT0;
}

void send_cmd(char c)
{
    putchar(c);
    putchar('\n');
    fflush(stdout);
}

//=============================================================
// PC Serial Parser (Translates Python scripts cleanly to IR)
//=============================================================
void handle_serial_command(char c)
{
    // *** THE QUOTE FILTER ***
    // Destroys accidental string quotes from Python before they cause trouble
    if(c == '\'' || c == '\"') return; 

    static char path_buf[10];   
    static unsigned char path_pos = 0;
    static char in_path_frame = 0;

    static char in_j_frame = 0;
    static char j_buf[2];
    static unsigned char j_pos = 0;

    /* ---------- Collect complete P...X packet ---------- */
    if(in_path_frame)
    {
        if(c == '\r' || c == '\n') return; // ignore line endings inside packet

        if(c == 'L' || c == 'R' || c == 'S')
        {
            if(path_pos < 9) path_buf[path_pos++] = c;
            else { in_path_frame = 0; path_pos = 0; }
            return;
        }

        if(c == 'X')
        {
            int i;
            path_buf[path_pos++] = 'X';
            for(i = 0; i < path_pos; i++) putchar(path_buf[i]);
            fflush(stdout); // Send whole packet to EFM8 silently
            
            in_path_frame = 0;
            path_pos = 0;
            return;
        }
        in_path_frame = 0;
        path_pos = 0;
        return;
    }

    /* ---------- Collect complete Jxx packet (From Python) ---------- */
    if(in_j_frame)
    {
        if(c == '\r' || c == '\n') return; // ignore line endings inside packet
        
        j_buf[j_pos++] = c;
        if(j_pos == 2)
        {
            // Successfully caught J + dir + spd! Fire it off to EFM8 silently.
            printf("J%c%c\n", j_buf[0], j_buf[1]);
            fflush(stdout);
            
            in_j_frame = 0;
            j_pos = 0;
        }
        return;
    }

    /* ---------- Normal Character Passthrough ---------- */
    switch(c)
    {
        case 'P':
            in_path_frame = 1;
            path_pos = 0;
            path_buf[path_pos++] = 'P';
            break;

        case 'J': // Triggers the J parser above
            in_j_frame = 1;
            j_pos = 0;
            break;

        case 'B': case 'b':
            send_cmd('B'); // Forward the joystick push button correctly
            break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            send_cmd(c); // Works for Python sending '0' (Stop) or keypads
            break;

        case '\r':
        case '\n':
            break;

        default:
            // Purposely do nothing. No debug text allowed!
            break;
    }
}

int button_pressed(volatile uint32_t *idr, uint32_t bitmask)
{
    if((*idr & bitmask) == 0)
    {
        delayms(20); 
        if((*idr & bitmask) == 0)
        {
            while((*idr & bitmask) == 0); 
            delayms(20); 
            return 1;
        }
    }
    return 0;
}

void process_button(volatile uint32_t *idr, uint32_t bitmask, char cmd,
                    char *l_dir, char *l_spd, int *l_x, int *l_y)
{
    if(button_pressed(idr, bitmask))
    {
        send_cmd(cmd);

        *l_dir = '0';
        *l_spd = '0';
        *l_x = 0;
        *l_y = 0;

        delayms(150);
    }
}

int main(void)
{
    char rx;
    unsigned int vrx_val, vry_val;
    int norm_x, norm_y;
    int mag;

    char dir_char = '0';
    char spd_char = '0';

    char last_dir = 'X';
    char last_speed = 'X';
    int last_sent_x = 0;
    int last_sent_y = 0;

    const int THRESH = 30;

    delayms(500);

    Joystick_Init();
    Buttons_Init();
    IR_Carrier_Init();

    // Removed all the noisy "Ready" startup text. 
    // The IR line is now completely silent unless a valid command is fired.

    while(1)
    {
        /* -------- PC serial command handling -------- */
        if(GetBufCount(&ComRXBuffer) > 0)
        {
            rx = egetc();
            handle_serial_command(rx);
        }

        /* -------- Joystick handling -------- */
        vrx_val = ADC_Read(0);
        vry_val = ADC_Read(1);

        if(vrx_val < 1800) norm_x = map_val(vrx_val, 0, 1800, -100, 0);
        else               norm_x = map_val(vrx_val, 1800, 4000, 0, 100);

        if(vry_val < 1900) norm_y = map_val(vry_val, 0, 1900, -100, 0);
        else               norm_y = map_val(vry_val, 1900, 4050, 0, 100);

        mag = (abs(norm_x) > abs(norm_y)) ? abs(norm_x) : abs(norm_y);
        mag = (mag * mag) / 100;

        if(mag > 70) spd_char = '2';
        else         spd_char = '1';

        if(abs(norm_x) < THRESH && abs(norm_y) < THRESH)
        {
            dir_char = '0';
            spd_char = '0';
        }
        else
        {
            if(norm_y >= THRESH && norm_x >= THRESH)        dir_char = '5'; // FR
            else if(norm_y <= -THRESH && norm_x >= THRESH)  dir_char = '7'; // BR
            else if(norm_y >= THRESH && norm_x <= -THRESH)  dir_char = '6'; // FL
            else if(norm_y <= -THRESH && norm_x <= -THRESH) dir_char = '8'; // BL
            else if(norm_y >= THRESH)                       dir_char = '1'; // F
            else if(norm_y <= -THRESH)                      dir_char = '2'; // B
            else if(norm_x >= THRESH)                       dir_char = '4'; // R
            else if(norm_x <= -THRESH)                      dir_char = '3'; // L
        }

        /* -------- Transmission Logic -------- */
        {
            int state_changed = (dir_char != last_dir) || (spd_char != last_speed);
            int big_change = (abs(norm_x - last_sent_x) >= 30) || (abs(norm_y - last_sent_y) >= 30);

            if(state_changed || big_change)
            {
                if(dir_char == '0' && spd_char == '0')
                {
                    printf("0\n");
                }
                else
                {
                    printf("J%c%c\n", dir_char, spd_char);
                }

                fflush(stdout);

                last_dir = dir_char;
                last_speed = spd_char;
                last_sent_x = norm_x;
                last_sent_y = norm_y;
            }
        }

        /* -------- Button handling -------- */
        process_button(&GPIOA->IDR, BIT2,  'B', &last_dir, &last_speed, &last_sent_x, &last_sent_y);
        process_button(&GPIOA->IDR, BIT11, '0', &last_dir, &last_speed, &last_sent_x, &last_sent_y);
        process_button(&GPIOA->IDR, BIT12, '1', &last_dir, &last_speed, &last_sent_x, &last_sent_y);
        process_button(&GPIOA->IDR, BIT13, '2', &last_dir, &last_speed, &last_sent_x, &last_sent_y);
        process_button(&GPIOA->IDR, BIT3,  '3', &last_dir, &last_speed, &last_sent_x, &last_sent_y);
        process_button(&GPIOA->IDR, BIT4,  '4', &last_dir, &last_speed, &last_sent_x, &last_sent_y);
        process_button(&GPIOA->IDR, BIT5,  '5', &last_dir, &last_speed, &last_sent_x, &last_sent_y);
        process_button(&GPIOA->IDR, BIT6,  '6', &last_dir, &last_speed, &last_sent_x, &last_sent_y);
        process_button(&GPIOA->IDR, BIT7,  '7', &last_dir, &last_speed, &last_sent_x, &last_sent_y);
        process_button(&GPIOB->IDR, BIT0,  '8', &last_dir, &last_speed, &last_sent_x, &last_sent_y);
        process_button(&GPIOB->IDR, BIT1,  '9', &last_dir, &last_speed, &last_sent_x, &last_sent_y);

        delayms(50);
    }
}