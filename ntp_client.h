/*
 * ntp_client.h
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

typedef struct NTP_T_ NTP_T;

/* Set your offset from UTC here, in seconds
 * UTC+1 = 3600
 * Sorry, no automatic DST handling yet. */
#define UTC_OFFSET_SEC 3600

extern NTP_T *ntp_init(void);
extern int ntp_ok(NTP_T *state);
extern int ntp_err(NTP_T *state);

/* For verbose debugging, switch the commenting of these two lines.
 * .. and see CMakeLists.txt */
static void debug_print(const char *str, ...) {}
//#define debug_print printf
