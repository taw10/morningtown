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
#include "settings.h"

#define LED_BLUE 21
#define TEST_BUTTON 16

static void check_clock(int *pre_wake, int *wake_now)
{
    int32_t dstoffs;
    datetime_t t = {0};

    if ( ds3231_get_datetime(&t) ) {
        rtc_get_datetime(&t);
    }

    dstoffs = settings.utc_offset + dst(t);

    if ( t.hour >= settings.clear_hour-dstoffs ) {
        *pre_wake = 0;
        *wake_now = 0;

    } else if ( (t.hour >= settings.late_hour-dstoffs) && (t.min >= settings.late_min) ) {
        *pre_wake = 0;
        *wake_now = 1;

    } else if ( (t.hour >= settings.morning_hour-dstoffs) && (t.min >= settings.morning_min) ) {
        *pre_wake = 1;
        *wake_now = 0;

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
    int time_ok = 0;

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

    settings_read();

    setup_pwm(settings.morning_pin);
    setup_pwm(settings.late_pin);
    setup_pwm(LED_BLUE);

    pwm_set_gpio_level(LED_BLUE, brightness);

    if ( !set_picortc_from_ds3231() ) {
        time_ok = 1;
    }

    /* Red light indicates DS3231 */
    sleep_ms(500);
    if ( ds3231_found() ) {
        pwm_set_gpio_level(settings.late_pin, brightness);
    }

    if ( ds3231_osf_set() ) {
        int i;
        for ( i=0; i<25; i++ ) {
            pwm_set_gpio_level(settings.late_pin, brightness);
            sleep_ms(100);
            pwm_set_gpio_level(settings.late_pin, 0);
            sleep_ms(100);
        }
    }

    /* Green light indicates RP2040 RTC */
    sleep_ms(500);
    if ( rtc_running() ) {
        pwm_set_gpio_level(settings.morning_pin, brightness);
    }

    /* Wait, then turn everything off */
    sleep_ms(2000);
    set_board_led(0);
    pwm_set_gpio_level(settings.morning_pin, 0);
    pwm_set_gpio_level(settings.late_pin, 0);
    pwm_set_gpio_level(LED_BLUE, 0);

    Terminal *trm = terminal_init();

    ntp_state = ntp_init();
    last_conn = 20000;
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

        if ( ntp_ok(ntp_state) ) time_ok = 1;

        /* Check clock every 10 seconds */
        countdown--;
        if ( countdown == 0 ) {
            if ( time_ok ) check_clock(&pre_wake, &wake_now);
            countdown = 100;
        }

        /* Determine the LED status */
        if ( gpio_get(TEST_BUTTON) == 0 ) {
            /* Button pressed */
            pwm_set_gpio_level(settings.morning_pin, (time_ok && rtc_running())?brightness:0);
            pwm_set_gpio_level(settings.late_pin, ntp_ok(ntp_state)?brightness:0);
#ifdef PICO_W
            set_board_led(st == CYW43_LINK_JOIN);
#else
            set_board_led(1);
#endif
        } else {
            /* Normal operation */
            pwm_set_gpio_level(settings.morning_pin, (pre_wake||wake_now)?brightness:0);
            pwm_set_gpio_level(settings.late_pin, wake_now?brightness:0);
            set_board_led(0);
        }

        last_conn += 1;
#ifdef PICO_W
        cyw43_arch_poll();
#endif
        terminal_poll(trm);
        if ( stdio_usb_connected() ) {
            set_board_led(1);
            sleep_ms(10);
        } else {
            set_board_led(0);
            sleep_ms(100);
        }

    }
}
