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
#include <pico/cyw43_arch.h>

#include "ntp_client.h"

#define LED_GREEN 21
#define LED_RED 22

int ntp_sent = 0;
int ntp_ok = 0;


static void flash_red(int n)
{
	int i;

	gpio_put(LED_GREEN, 0);
	for ( i=0; i<10; i++ ) {
		gpio_put(LED_RED, 1);
		sleep_ms(100);
		gpio_put(LED_RED, 0);
		sleep_ms(100);
	}
}


static void celebrate()
{
	int i;

	for ( i=0; i<10; i++ ) {
		gpio_put(LED_RED, 1);
		gpio_put(LED_GREEN, 0);
		sleep_ms(300);
		gpio_put(LED_RED, 0);
		gpio_put(LED_GREEN, 1);
		sleep_ms(300);
	}

	gpio_put(LED_GREEN, 0);
	gpio_put(LED_RED, 0);
}


static void check_clock()
{
	datetime_t t = {0};
	rtc_get_datetime(&t);

	if ( (t.hour == 7) && (t.min >= 15) ) {
		gpio_put(LED_RED, 0);
		gpio_put(LED_GREEN, 1);
	} else if ( (t.hour >= 8) && (t.hour < 12) ) {
		gpio_put(LED_RED, 1);
		gpio_put(LED_GREEN, 1);
	} else {
		gpio_put(LED_RED, 0);
		gpio_put(LED_GREEN, 0);
	}
}


static void ntp_callback(int status)
{
	switch ( status ) {

		case NTP_DNS_ERROR:
		flash_red(5);
		ntp_sent = 0;
		break;

		case NTP_DNS_NO_ADDR:
		flash_red(10);
		ntp_sent = 0;
		break;

		case NTP_BAD_REPLY:
		flash_red(15);
		ntp_sent = 0;
		break;

		case NTP_TIMEOUT:
		flash_red(20);
		ntp_sent = 0;
		break;

		case NTP_REQUEST_SENT:
		break;

		case NTP_REPLY_RECEIVED:
		celebrate();
		ntp_ok = 1;
		break;
	}
}


int main()
{
	NTP_T *ntp_state;
	int last_conn;

	gpio_init(LED_GREEN);
	gpio_init(LED_RED);
	gpio_set_dir(LED_GREEN, GPIO_OUT);
	gpio_set_dir(LED_RED, GPIO_OUT);

	cyw43_arch_init();
	cyw43_arch_enable_sta_mode();

	stdio_init_all();

	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
	gpio_put(LED_RED, 1);
	gpio_put(LED_GREEN, 1);
	sleep_ms(2000);
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
	gpio_put(LED_RED, 0);
	gpio_put(LED_GREEN, 0);

	rtc_init();

	ntp_state = ntp_init(ntp_callback);
	last_conn = 10;
	while (1) {

		int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
		cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN,
		                    !ntp_ok && (st == CYW43_LINK_JOIN));

		if ( (st != CYW43_LINK_JOIN) && (last_conn > 10) ) {
			cyw43_arch_wifi_connect_async(WIFI_SSID,
			                              WIFI_PASSWORD,
			                              CYW43_AUTH_WPA2_AES_PSK);
			debug_print("connecting to wifi...\n");
			last_conn = 0;
		}

		if ( (st == CYW43_LINK_JOIN) && !ntp_sent ) {
			ntp_send_request(ntp_state);
			debug_print("sending NTP request\n");
			ntp_sent = 1;
		}

		if ( ntp_ok ) {
			debug_print("connected OK.  Checking clock..\n");
			check_clock();
		}

		ntp_poll(ntp_state);

		debug_print("tick\n");

		last_conn += 1;
		if ( ntp_ok ) {
			sleep_ms(10000);
		} else {
			sleep_ms(1000);
		}

	}
}
