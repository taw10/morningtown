/*
 * morningtown.c
 *
 * Silent alarm clock
 *
 * Copyright © 2023 Thomas White <taw@physics.org>
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

#include <pico/stdlib.h>
#include <hardware/rtc.h>
#include <hardware/watchdog.h>
#include <hardware/pwm.h>
#include <time.h>
#include <stdio.h>

#ifdef PICO_W
#include <pico/cyw43_arch.h>
#endif

#include "ntp_client.h"
#include "terminal.h"
#include "ds3231.h"

/* For cheap and cheerful hardware solution:
 * #define LED_GREEN 21
 * #define LED_RED 22
 *
 * For custom PCB:
 * #define LED_GREEN 19
 * #define LED_RED 22
 */
#define LED_GREEN 19
#define LED_RED 22
#define TEST_BUTTON 16

/* Hard-coded calculations because I can't be bothered to work out all the
 * special cases (leap years etc).  There's a decent chance that DST will have
 * been abolished in Europe by the time this table runs out, anyway.
 */
static int last_sunday_in_march(time_t year)
{
    switch ( year ) {
        case 2024: return 31;
        case 2025: return 30;
        case 2026: return 29;
        case 2027: return 28;
        case 2028: return 26;
        case 2029: return 25;
        case 2030: return 31;
        default: return 28;  /* Guess! */
    }
}


static int last_sunday_in_october(time_t year)
{
    switch ( year ) {
        case 2024: return 27;
        case 2025: return 26;
        case 2026: return 25;
        case 2027: return 31;
        case 2028: return 29;
        case 2029: return 28;
        case 2030: return 27;
        default: return 27;  /* Guess! */
    }
}


/* This is about the simplest possible case of local time handling, and it
 * still makes me want to hurl.  Ugh.
 *
 * Note that we don't care about the *time* of switching to/from DST (01:00 in
 * Europe), because the wakeup time is usually much later.
 */
static int dst(datetime_t t)
{
    /* April to September inclusive.  Note Jan=0 */
    if ( (t.month >= 3) && (t.month <= 8) ) return 1;

    if ( (t.month == 2) && (t.day >= last_sunday_in_march(t.year)) ) return 1;
    if ( (t.month == 9) && (t.day < last_sunday_in_october(t.year)) ) return 1;

    return 0;
}


static void check_clock(int *pre_wake, int *wake_now)
{
    time_t dstoffs;
    datetime_t t = {0};
    rtc_get_datetime(&t);

    /* Time offsets for CET/CEST (Europe) */
    dstoffs = dst(t) ? 2 : 1;

    /* Set wakeup times here */
    if ( (t.hour == 7-dstoffs) && (t.min >= 15) ) {
        *pre_wake = 1;
        *wake_now = 0;
    } else if ( (t.hour >= 8-dstoffs) && (t.hour < 12) ) {
        *pre_wake = 0;
        *wake_now = 1;
    } else {
        *pre_wake = 0;
        *wake_now = 0;
    }

}


static void setup_pwm(int pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.f);
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0);
}


static int all_ok(NTP_T *ntp_state)
{
    return ntp_ok(ntp_state) && rtc_running();
}


static void set_board_led(int level)
{
    #ifdef PICO_W
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, level);
    #else
    gpio_put(PICO_DEFAULT_LED_PIN, level);
    #endif
}


int main()
{
    NTP_T *ntp_state;
    int last_conn, countdown;
    int pre_wake = 0;
    int wake_now = 0;
    int initial_sync;

    const int brightness = 65535;

    stdio_init_all();
    printf("MorningTown initialising\n");

#ifdef PICO_W
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
#else
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif

    watchdog_enable(0x7fffff, 1);
    rtc_init();
    ds3231_init();

    /* Board LED shows we're alive */
    set_board_led(1);

    gpio_init(TEST_BUTTON);
    gpio_set_dir(TEST_BUTTON, GPIO_IN);
    gpio_pull_up(TEST_BUTTON);

    setup_pwm(LED_GREEN);
    setup_pwm(LED_RED);

    /* Red light indicates DS3231 */
    if ( ds3231_found() ) {
	pwm_set_gpio_level(LED_RED, brightness);
    }

    /* Green light indicates RP2040 RTC */
    if ( rtc_running() ) {
        pwm_set_gpio_level(LED_GREEN, brightness);
    }

    /* Wait, then turn everything off */
    sleep_ms(2000);
    set_board_led(0);
    pwm_set_gpio_level(LED_GREEN, 0);
    pwm_set_gpio_level(LED_RED, 0);

    set_picortc_from_ds3231();

    terminal_init();

    ntp_state = ntp_init();
    last_conn = 20000;
    initial_sync = 0;
    countdown = 100;
    while (1) {

        watchdog_update();

#ifdef PICO_W
        int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);

        if ( (st != CYW43_LINK_JOIN) && (last_conn > 10000) ) {
            cyw43_arch_wifi_connect_async(WIFI_SSID,
                    WIFI_PASSWORD,
                    CYW43_AUTH_WPA2_AES_PSK);
            printf("connecting to wifi...\n");
            last_conn = 0;
        }
#endif

        if ( ntp_ok(ntp_state) ) initial_sync = 1;

        /* Check clock every 10 seconds */
        countdown--;
        if ( countdown == 0 ) {
            if ( initial_sync ) check_clock(&pre_wake, &wake_now);
            countdown = 100;
        }

        /* Determine the LED status */
        if ( gpio_get(TEST_BUTTON) == 0 ) {
            /* Button pressed */
            pwm_set_gpio_level(LED_GREEN, all_ok(ntp_state)?brightness:0);
            pwm_set_gpio_level(LED_RED, ntp_err(ntp_state)?brightness:0);
#ifdef PICO_W
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN,
                    (st == CYW43_LINK_JOIN));
#endif
        } else {
            /* Normal operation */
            pwm_set_gpio_level(LED_GREEN, (pre_wake||wake_now)?brightness:0);
            pwm_set_gpio_level(LED_RED, wake_now?brightness:0);
            set_board_led(0);
        }

        last_conn += 1;
#ifdef PICO_W
        cyw43_arch_poll();
#endif
        sleep_ms(100);

    }
}
