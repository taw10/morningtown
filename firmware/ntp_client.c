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
	struct udp_pcb *ntp_pcb;
	alarm_id_t send_alarm;
	int err;
	int ok;
	int need_request;
} NTP_T;


#define NTP_SERVER "pool.ntp.org"
#define NTP_MSG_LEN 48
#define NTP_PORT 123

/* Seconds between 1 Jan 1900 and 1 Jan 1970 */
#define NTP_DELTA 2208988800

/* Interval between re-sending NTP requests (in MICROseconds) */
#define NTP_RESEND_TIME (5 * 1000000)

/* How long after a successful NTP sync to do another (in milliseconds) */
#define NTP_UPDATE_INTERVAL ((37*60*60 + 23*60 + 43)*1000)


static void set_rtc(time_t iutc)
{
	struct tm *utc = gmtime(&iutc);
	datetime_t t;

	t.year = utc->tm_year + 1900;
	t.month = utc->tm_mon + 1;
	t.day = utc->tm_mday;
	t.dotw = utc->tm_wday;
	t.hour = utc->tm_hour;
	t.min = utc->tm_min;
	t.sec = utc->tm_sec;

	printf("time is %i/%i/%i   %i   %i:%i:%i\n",
            t.year, t.month, t.day, t.dotw, t.hour, t.min, t.sec);

	rtc_set_datetime(&t);
}


static int64_t request_handler(alarm_id_t id, void *user_data)
{
	NTP_T *state = (NTP_T*)user_data;
	state->need_request = 1;
	return 0;
}


static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                     const ip_addr_t *addr, u16_t port)
{
	NTP_T *state = (NTP_T*)arg;
	uint8_t mode = pbuf_get_at(p, 0) & 0x7;
	uint8_t stratum = pbuf_get_at(p, 1);

	printf("got UDP: %i %i\n", addr, state->ntp_server_address);
	printf("port %i len %i mode %i stratum %i\n", port, p->tot_len, mode, stratum);

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
		printf("second since 1970 = %i\n", epoch);
		set_rtc(epoch);
		state->err = 0;
		state->ok = 1;
		add_alarm_in_ms(NTP_UPDATE_INTERVAL, request_handler, state, false);

	} else {
		state->err = 1;
		state->ok = 0;
	}
	pbuf_free(p);
}


/* The real address must be in state->ntp_server_address by this point */
static void ntp_request(NTP_T *state)
{
	printf("sending NTP UDP\n");
	cyw43_arch_lwip_begin();
	struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
	uint8_t *req = (uint8_t *) p->payload;
	memset(req, 0, NTP_MSG_LEN);
	req[0] = 0x1b;
	udp_sendto(state->ntp_pcb, p, &state->ntp_server_address, NTP_PORT);
	pbuf_free(p);
	cyw43_arch_lwip_end();
	state->err = 0;
}


static void ntp_dns_found(const char *hostname,
                          const ip_addr_t *ipaddr,
                          void *arg)
{
	NTP_T *state = (NTP_T*)arg;
	if (ipaddr) {
		printf("DNS ok (%i)\n", *ipaddr);
		state->ntp_server_address = *ipaddr;
		state->err = 0;
		ntp_request(state);
	} else {
		printf("NTP failed: DNS no address\n");
		state->err = 1;
		state->ok = 0;
	}
}


static int64_t send_handler(alarm_id_t id, void *user_data)
{
	int err;
	NTP_T *state = (NTP_T*)user_data;

	if ( !state->need_request && !state->err ) return NTP_RESEND_TIME;
	state->need_request = 0;

	cyw43_arch_lwip_begin();
	err = dns_gethostbyname(NTP_SERVER,
	                        &state->ntp_server_address,
	                        ntp_dns_found,
	                        state);
	cyw43_arch_lwip_end();

	if ( err == ERR_OK ) {
		/* Immediate reply -> cached result */
		ntp_request(state);
		state->err = 0;
	} else if (err != ERR_INPROGRESS) {
		state->err = 1;
	}
	return NTP_RESEND_TIME;
}


NTP_T *ntp_init()
{
	NTP_T *state = calloc(1, sizeof(NTP_T));
	if (!state) return NULL;

	state->err = 0;
	state->ok = 0;
	state->need_request = 1;

	state->ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
	if (!state->ntp_pcb) {
		free(state);
		return NULL;
	}

	/* Set up UDP callback */
	udp_recv(state->ntp_pcb, ntp_recv, state);

	/* It's probably too early to start resolving hostnames or sending
	 * UDP packets.  Call us back in a moment. */
	state->send_alarm = add_alarm_in_us(NTP_RESEND_TIME,
	                                    send_handler,
	                                    state,
	                                    true);

	return state;
}


int ntp_ok(NTP_T *state)
{
	if ( state == NULL ) return 0;
	return state->ok;
}


int ntp_err(NTP_T *state)
{
	if ( state == NULL ) return 0;
	return state->err;
}
