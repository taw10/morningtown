/*
 * ds3231.c
 *
 * Copyright © 2024 Thomas White <taw@physics.org>
 *
 * This file is part of MorningTown
 *
 * MorningTown is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MorningTown is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MorningTown.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <time.h>
#include <stdio.h>

#include <pico/stdlib.h>
#include <hardware/rtc.h>
#include <hardware/i2c.h>


static int have_ds3231 = 0;

uint8_t clear_bit(uint8_t byte, int bit)
{
    return (byte & ~(1<<bit));
}


static int clear_32khz_bit()
{
    uint8_t buf[2];
    int r;

    buf[0] = 0x0f;
    i2c_write_blocking(i2c_default, 0x68, buf, 1, true);
    r = i2c_read_blocking(i2c_default, 0x68, buf, 1, false);
    if ( r == PICO_ERROR_GENERIC ) {
        printf("ds3231 not found\n");
        return 0;
    }

    buf[1] = clear_bit(buf[0], 3);
    buf[0] = 0x0f;
    i2c_write_blocking(i2c_default, 0x68, buf, 2, false);
    return 1;
}


void ds3231_init()
{
    i2c_init(i2c0, 1000000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);

    have_ds3231 = clear_32khz_bit();
}


static uint8_t to_bcd(int n)
{
    return (n/10)<<4 | (n%10);
}


static int from_bcd(uint8_t n)
{
    return ((n&0xf0)>>4)*10 + (n&0x0f);
}


void set_ds3231_from_picortc()
{
    uint8_t buf[8];
    datetime_t t = {0};

    if ( !have_ds3231 ) return;

    rtc_get_datetime(&t);

    buf[0] = 0;
    buf[1] = to_bcd(t.sec);
    buf[2] = to_bcd(t.min);
    buf[3] = to_bcd(t.hour);
    buf[4] = t.dotw;
    buf[5] = to_bcd(t.day);
    buf[6] = to_bcd(t.month);
    buf[7] = to_bcd(t.year%100);

    i2c_write_blocking(i2c_default, 0x68, buf, 8, true);
}


void ds3231_get_datetime(datetime_t *t)
{
    uint8_t buf[7];

    if ( !have_ds3231 ) return;

    buf[0] = 0x00;
    i2c_write_blocking(i2c_default, 0x68, buf, 1, true);
    i2c_read_blocking(i2c_default, 0x68, buf, 7, false);

    t->year = 2000+from_bcd(buf[6]);
    t->month = from_bcd(buf[5] & 0x1f);
    t->day = from_bcd(buf[4]);
    t->dotw = buf[3];
    t->hour = from_bcd(buf[2]);
    t->min = from_bcd(buf[1]);
    t->sec = from_bcd(buf[0]);
}


void set_picortc_from_ds3231()
{
    datetime_t t;
    if ( !have_ds3231 ) return;
    ds3231_get_datetime(&t);
    rtc_set_datetime(&t);

}


static signed int conv_signed(uint8_t lsb)
{
    return *((int8_t *)(&lsb));
}


static float conv_temp(uint8_t msb, uint8_t lsb)
{
    float t = conv_signed(msb);
    if ( t < 0 ) {
        if ( lsb & 1<<7 ) t -= 0.5;
        if ( lsb & 1<<6 ) t -= 0.25;
    } else {
        if ( lsb & 1<<7 ) t += 0.5;
        if ( lsb & 1<<6 ) t += 0.25;
    }
    return t;
}


static void print_flag(uint8_t byte, int bit, const char *name)
{
    if ( byte & 1<<bit ) {
        printf("*%s*  ", name);
    } else {
        printf("(%s)  ", name);
    }
}


void ds3231_status()
{
    uint8_t buf[19];
    int j;
    int r;
    datetime_t t;

    buf[0] = 0x00;
    i2c_write_blocking(i2c_default, 0x68, buf, 1, true);
    r = i2c_read_blocking(i2c_default, 0x68, buf, 19, false);
    if ( r == PICO_ERROR_GENERIC ) {
        printf("DS3231 not found\n");
        return;
    }

    t.year = 2000+from_bcd(buf[6]);
    t.month = from_bcd(buf[5] & 0x1f);
    t.day = from_bcd(buf[4]);
    t.dotw = buf[3];
    t.hour = from_bcd(buf[2]);
    t.min = from_bcd(buf[1]);
    t.sec = from_bcd(buf[0]);
    printf("Time: %2i:%2i:%2i  Date: %i/%i/%i  DoW=%i\n",
            t.hour, t.min, t.sec, t.day, t.month, t.year, t.dotw);

    printf("Flags: ");
    print_flag(buf[14], 7, "/EOSC");
    print_flag(buf[14], 6, "BBSQW");
    print_flag(buf[14], 5, "CONV");
    print_flag(buf[14], 2, "INTCN");
    print_flag(buf[14], 1, "A2IE");
    print_flag(buf[14], 0, "A1IE");
    print_flag(buf[15], 7, "OSF");
    print_flag(buf[15], 3, "EN32KHZ");
    print_flag(buf[15], 2, "BSY");
    print_flag(buf[15], 1, "A2F");
    print_flag(buf[15], 0, "A1F");
    printf("\n");

    printf("Aging offset: %i\n", conv_signed(buf[16]));
    printf("Temperature: %f\n", conv_temp(buf[17], buf[18]));
}


void ds3231_reset_osf()
{
    uint8_t buf[2];
    int r;

    buf[0] = 0x0f;
    i2c_write_blocking(i2c_default, 0x68, buf, 1, true);
    r = i2c_read_blocking(i2c_default, 0x68, buf, 1, false);
    if ( r == PICO_ERROR_GENERIC ) {
        printf("ds3231 not found\n");
        return;
    }

    buf[1] = clear_bit(buf[0], 7);
    buf[0] = 0x0f;
    i2c_write_blocking(i2c_default, 0x68, buf, 2, false);
}
