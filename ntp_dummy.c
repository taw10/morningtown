/*
 * ntp_dummy.c
 *
 * Copyright © 2024 Thomas White <taw@physics.org>
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

#include <stddef.h>

#include "ntp_client.h"


typedef struct NTP_T_ {
	int nothing;
} NTP_T;


NTP_T *ntp_init()
{
	return NULL;
}

int ntp_ok(NTP_T *state)
{
	return 1;
}


int ntp_err(NTP_T *state)
{
	return 0;
}
