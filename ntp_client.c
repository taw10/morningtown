/*
 * ntp_client.c
 *
 * Copyright © 2023 Thomas White <taw@physics.org>
 *
 * This file is part of MorningTown
 *
 * Based on pico-examples/pico_w/ntp_client/picow_ntp_client.c
 * Original license: BSD-3-Clause
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
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

#include <string.h>
#include <time.h>

#include <hardware/rtc.h>
#include <pico/stdlib.h>
#include <pico/util/datetime.h>
#include <pico/cyw43_arch.h>

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "ntp_client.h"


typedef struct NTP_T_ {
	ip_addr_t ntp_server_address;
	bool dns_request_sent;
	struct udp_pcb *ntp_pcb;
	alarm_id_t ntp_resend_alarm;
	void (*callback)(int status);
	time_t utc;
	int have_new_reply;
	int last_status;
} NTP_T;


#define NTP_SERVER "pool.ntp.org"
#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800 // seconds between 1 Jan 1900 and 1 Jan 1970
#define NTP_RESEND_TIME (10 * 1000)


static void ntp_result(NTP_T *state, int status, time_t *result)
{
	debug_print("NTP result:\n");
	state->have_new_reply = 1;
	state->last_status = status;
	if (status == NTP_REPLY_RECEIVED && result) {

		state->utc = *result;
		debug_print("yay!\n");

		if ( state->ntp_resend_alarm > 0 ) {
			cancel_alarm(state->ntp_resend_alarm);
			state->ntp_resend_alarm = 0;
		}

	} else {
		debug_print("other reply.\n");
	}

	state->dns_request_sent = false;
}


static void ntp_request(NTP_T *state)
{
	debug_print("sending NTP UDP\n");
	cyw43_arch_lwip_begin();
	struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
	uint8_t *req = (uint8_t *) p->payload;
	memset(req, 0, NTP_MSG_LEN);
	req[0] = 0x1b;
	udp_sendto(state->ntp_pcb, p, &state->ntp_server_address, NTP_PORT);
	pbuf_free(p);
	cyw43_arch_lwip_end();
	state->callback(NTP_REQUEST_SENT);
}


static void ntp_dns_found(const char *hostname,
                          const ip_addr_t *ipaddr,
                          void *arg)
{
	NTP_T *state = (NTP_T*)arg;
	if (ipaddr) {
		debug_print("DNS ok (%i)\n", *ipaddr);
		state->ntp_server_address = *ipaddr;
		ntp_request(state);
	} else {
		debug_print("NTP failed: DNS no address\n");
		ntp_result(state, NTP_DNS_NO_ADDR, NULL);
	}
}


static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                     const ip_addr_t *addr, u16_t port)
{
	NTP_T *state = (NTP_T*)arg;
	uint8_t mode = pbuf_get_at(p, 0) & 0x7;
	uint8_t stratum = pbuf_get_at(p, 1);

	debug_print("got UDP: %i %i\n", addr, state->ntp_server_address);
	debug_print("port %i len %i mode %i stratum %i\n", port, p->tot_len, mode, stratum);

	if (ip_addr_cmp(addr, &state->ntp_server_address)
	 && port == NTP_PORT
	 && p->tot_len == NTP_MSG_LEN
	 && mode == 0x4 && stratum != 0)
	{
		uint8_t seconds_buf[4] = {0};
		pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
		uint32_t seconds_since_1900 = seconds_buf[0] << 24
		                            | seconds_buf[1] << 16
		                            | seconds_buf[2] << 8
		                            | seconds_buf[3];
		uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
		time_t epoch = seconds_since_1970;
		debug_print("second since 1970 = %i\n", epoch);
		ntp_result(state, NTP_REPLY_RECEIVED, &epoch);
	} else {
		ntp_result(state, NTP_BAD_REPLY, NULL);
	}
	pbuf_free(p);
}


static int64_t ntp_failed_handler(alarm_id_t id, void *user_data)
{
	NTP_T *state = (NTP_T*)user_data;
	ntp_result(state, NTP_TIMEOUT, NULL);
	return 0;
}


NTP_T *ntp_init(void (*reply_func)(int))
{
	NTP_T *state = calloc(1, sizeof(NTP_T));
	if (!state) return NULL;

	state->callback = reply_func;
	state->have_new_reply = 0;

	state->ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
	if (!state->ntp_pcb) {
		free(state);
		return NULL;
	}
	udp_recv(state->ntp_pcb, ntp_recv, state);
	return state;
}


void ntp_send_request(NTP_T *state)
{
	if ( state == NULL ) return;

	cyw43_arch_lwip_begin();
	int err = dns_gethostbyname(NTP_SERVER,
	                            &state->ntp_server_address,
	                            ntp_dns_found,
	                            state);
	cyw43_arch_lwip_end();

	state->ntp_resend_alarm = add_alarm_in_ms(NTP_RESEND_TIME,
	                                          ntp_failed_handler,
	                                          state,
	                                          true);

	state->dns_request_sent = true;
	if ( err == ERR_OK ) {
		/* Immediate reply -> cached result */
		ntp_request(state);
	} else if (err != ERR_INPROGRESS) {
		/* Error sending DNS request
		 * (ERR_INPROGRESS means expect a callback) */
		ntp_result(state, NTP_DNS_ERROR, NULL);
	}
}


void ntp_poll(NTP_T *state)
{
	if ( !state->have_new_reply ) return;
	state->have_new_reply = 0;

	if ( state->last_status == NTP_REPLY_RECEIVED ) {

		time_t tv = state->utc + UTC_OFFSET_SEC;
		struct tm *utc = gmtime(&tv);
		datetime_t t;

		t.year = utc->tm_year + 1900;
		t.month = utc->tm_mon + 1;
		t.day = utc->tm_mday;
		t.dotw = utc->tm_wday;
		t.hour = utc->tm_hour;
		t.min = utc->tm_min;
		t.sec = utc->tm_sec;

		debug_print("time is %i/%i/%i   %i   %i:%i:%i\n",
		            t.year, t.month, t.day, t.dotw, t.hour, t.min, t.sec);

		rtc_set_datetime(&t);

	}
	state->callback(state->last_status);
}
