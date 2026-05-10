#include <EFM8LB1.h>
#include <stdio.h>
#include "variable.h"
#include "collision.h"
#include "vl53l0x.h"

// ------------------------------------------------------------
// Persistent offset storage (optional calibration support)
// ------------------------------------------------------------
static unsigned char key1, key2;
static volatile int offset = 0;
static volatile int last_range_mm = VL53L0X_OUT_OF_RANGE;
static volatile bit blocked = 0;
static volatile bit sensor_ready = 0;

#define CONST_SIZE 4
#define BASE_FDATA 0xF800

#define SaveFdata(X,Y) \
{ FLKEY = 0xA5; FLKEY = 0xF1; PSCTL = 0x01; *((unsigned char xdata *) X)=Y; PSCTL = 0x00; }

#define EraseFdataPage(X) \
{ FLKEY = 0xA5; FLKEY = 0xF1; PSCTL = 0x03; *((unsigned char xdata *) X)=0; PSCTL = 0x00; }

#define ReadFdata(X) (*((unsigned char code *) X))

// ------------------------------------------------------------
// Software I2C on P1.2 / P1.3
// ------------------------------------------------------------
#define I2C_SDA P1_2
#define I2C_SCL P1_3

static void i2c_delay(void)
{
    Timer3us(5);
}

static void SDA_release(void)
{
    I2C_SDA = 1;
    i2c_delay();
}

static void SDA_low(void)
{
    I2C_SDA = 0;
    i2c_delay();
}

static void SCL_release(void)
{
    I2C_SCL = 1;
    i2c_delay();
}

static void SCL_low(void)
{
    I2C_SCL = 0;
    i2c_delay();
}

void I2C_start(void)
{
    SDA_release();
    SCL_release();
    SDA_low();
    SCL_low();
}

void I2C_stop(void)
{
    SDA_low();
    SCL_release();
    SDA_release();
}

static bit I2C_write_byte(unsigned char dat)
{
    unsigned char i;
    bit ack;

    for(i = 0; i < 8; i++)
    {
        if(dat & 0x80) SDA_release();
        else           SDA_low();

        SCL_release();
        SCL_low();
        dat <<= 1;
    }

    SDA_release();
    SCL_release();
    ack = !I2C_SDA;
    SCL_low();

    return ack;
}

static unsigned char I2C_read_byte(bit ack)
{
    unsigned char i, dat = 0;

    SDA_release();
    for(i = 0; i < 8; i++)
    {
        dat <<= 1;
        SCL_release();
        if(I2C_SDA) dat |= 0x01;
        SCL_low();
    }

    if(ack) SDA_low();
    else    SDA_release();

    SCL_release();
    SCL_low();
    SDA_release();

    return dat;
}

bit i2c_read_addr8_data8(unsigned char address, unsigned char * value)
{
    I2C_start();
    if(!I2C_write_byte(0x52)) { I2C_stop(); return 0; }
    if(!I2C_write_byte(address)) { I2C_stop(); return 0; }

    I2C_start();
    if(!I2C_write_byte(0x53)) { I2C_stop(); return 0; }
    *value = I2C_read_byte(0);
    I2C_stop();

    return 1;
}

bit i2c_read_addr8_data16(unsigned char address, unsigned int * value)
{
    unsigned char hi, lo;

    I2C_start();
    if(!I2C_write_byte(0x52)) { I2C_stop(); return 0; }
    if(!I2C_write_byte(address)) { I2C_stop(); return 0; }

    I2C_start();
    if(!I2C_write_byte(0x53)) { I2C_stop(); return 0; }
    hi = I2C_read_byte(1);
    lo = I2C_read_byte(0);
    I2C_stop();

    *value = ((unsigned int)hi << 8) | lo;
    return 1;
}

bit i2c_write_addr8_data8(unsigned char address, unsigned char value)
{
    I2C_start();
    if(!I2C_write_byte(0x52)) { I2C_stop(); return 0; }
    if(!I2C_write_byte(address)) { I2C_stop(); return 0; }
    if(!I2C_write_byte(value)) { I2C_stop(); return 0; }
    I2C_stop();

    return 1;
}

// ------------------------------------------------------------
// Flash-backed offset helpers
// ------------------------------------------------------------
static void Save_Vars(void)
{
    bit saved_EA;
    unsigned int j;
    unsigned int address;
    unsigned char * ptr;

    saved_EA = EA;
    EA = 0;
    EraseFdataPage(BASE_FDATA);

    key1 = 0x55;
    key2 = 0xAA;
    address = BASE_FDATA;
    ptr = &key1;

    for(j = 0; j < CONST_SIZE; j++)
    {
        SaveFdata(address++, *ptr);
        ptr++;
    }

    EA = saved_EA;
}

static void Restore_Vars(void)
{
    unsigned int j;
    unsigned int address;
    unsigned char * ptr;

    if ((ReadFdata(BASE_FDATA) != 0x55) || (ReadFdata(BASE_FDATA + 1) != 0xAA))
    {
        offset = 0;
    }
    else
    {
        address = BASE_FDATA;
        ptr = &key1;
        for(j = 0; j < CONST_SIZE; j++)
        {
            *ptr = ReadFdata(address++);
            ptr++;
        }
    }
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
void collision_init(void)
{
    unsigned char success;

    Restore_Vars();
    blocked = 0;
    last_range_mm = VL53L0X_OUT_OF_RANGE;

    success = vl53l0x_init();
    if(success)
    {
        success = vl53l0x_start_continuous();
    }

    sensor_ready = success ? 1 : 0;

    if(sensor_ready)
    {
        printf("--- VL53L0X collision sensor ready ---\n");
    }
    else
    {
        printf("--- VL53L0X init failed; collision detect disabled ---\n");
    }
}

void collision_task(void)
{
    unsigned int raw_range;
    int corrected_range;

    if(!sensor_ready) return;
    if(!vl53l0x_measurement_ready()) return;
    if(!vl53l0x_read_range_continuous(&raw_range)) return;

    corrected_range = (int)raw_range - offset;
    last_range_mm = corrected_range;

    if((corrected_range > 0) && (corrected_range < COLLISION_STOP_MM))
    {
        blocked = 1;
    }
    else if(corrected_range > (COLLISION_STOP_MM + COLLISION_RELEASE_MM))
    {
        blocked = 0;
    }

    if(CAL_BUTTON == 0)
    {
        waitms(20);
        if(CAL_BUTTON == 0)
        {
            offset = (int)raw_range - COLLISION_CAL_REFERENCE_MM;
            Save_Vars();
            printf("Collision sensor offset saved: %d\n", offset);
            while(CAL_BUTTON == 0);
        }
    }
}

bit collision_blocked(void)
{
    return blocked;
}

int collision_get_distance_mm(void)
{
    return last_range_mm;
}
