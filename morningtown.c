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
#include <pico/cyw43_arch.h>

#include "ntp_client.h"

#define LED_GREEN 21
#define LED_RED 22
#define TEST_BUTTON 16


static void check_clock(int *pre_wake, int *wake_now)
{
	datetime_t t = {0};
	rtc_get_datetime(&t);

	if ( (t.hour == 7) && (t.min >= 15) ) {
		*pre_wake = 1;
		*wake_now = 0;
	} else if ( (t.hour >= 8) && (t.hour < 12) ) {
		*pre_wake = 0;
		*wake_now = 1;
	} else {
		*pre_wake = 0;
		*wake_now = 0;
	}
}


int main()
{
	NTP_T *ntp_state;
	int last_conn;
	int pre_wake = 0;
	int wake_now = 0;

	gpio_init(LED_GREEN);
	gpio_init(LED_RED);
	gpio_init(TEST_BUTTON);
	gpio_set_dir(LED_GREEN, GPIO_OUT);
	gpio_set_dir(LED_RED, GPIO_OUT);
	gpio_set_dir(TEST_BUTTON, GPIO_IN);
	gpio_pull_up(TEST_BUTTON);

	cyw43_arch_init();
	cyw43_arch_enable_sta_mode();

	stdio_init_all();
	watchdog_enable(0x7fffff, 1);

	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
	gpio_put(LED_RED, 1);
	gpio_put(LED_GREEN, 1);
	sleep_ms(2000);
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
	gpio_put(LED_RED, 0);
	gpio_put(LED_GREEN, 0);

	rtc_init();

	ntp_state = ntp_init();
	last_conn = 200;
	while (1) {

		watchdog_update();

		int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);

		if ( (st != CYW43_LINK_JOIN) && (last_conn > 100) ) {
			cyw43_arch_wifi_connect_async(WIFI_SSID,
			                              WIFI_PASSWORD,
			                              CYW43_AUTH_WPA2_AES_PSK);
			debug_print("connecting to wifi...\n");
			last_conn = 0;
		}

		if ( ntp_ok(ntp_state) ) {
			check_clock(&pre_wake, &wake_now);
		}

		/* Determine the LED status */
		if ( gpio_get(TEST_BUTTON) == 0 ) {
			/* Button pressed */
			gpio_put(LED_GREEN, ntp_ok(ntp_state));
			gpio_put(LED_RED, ntp_err(ntp_state));
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN,
			                    (st == CYW43_LINK_JOIN));
		} else {
			if ( !ntp_ok(ntp_state) ) {
				/* Booting up */
				cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN,
				                    (st == CYW43_LINK_JOIN));
				gpio_put(LED_RED, 1);
				gpio_put(LED_GREEN, 0);
			} else {
				/* Normal operation */
				gpio_put(LED_GREEN, pre_wake || wake_now);
				gpio_put(LED_RED, wake_now);
				cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
			}
		}

		last_conn += 1;
		cyw43_arch_poll();
		sleep_ms(100);

	}
}
