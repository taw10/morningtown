/*
 * terminal.c
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

#include <pico/stdlib.h>
#include <hardware/rtc.h>
#include <stdio.h>
#include <string.h>

#include "ds3231.h"

struct terminal
{
    char c[256];
    int nchar;
};

struct terminal terminal_data;


static void set_clock(const char *str)
{
    int dow, d, mon, y, h, m, s;
    if ( sscanf(str, "%i %i %i %i %i %i %i", &dow, &d, &mon, &y, &h, &m, &s) == 7 ) {

        datetime_t t;

        printf("OK %i-%i-%i (%i), %i:%i:%i\n", d, mon, y, dow, h, m, s);

        t.year = y;
        t.month = mon;
        t.day = d;
        t.dotw = dow;
        t.hour = h;
        t.min = m;
        t.sec = s;
        rtc_set_datetime(&t);

    } else {
        printf("Syntax: set <weekday> <day> <mon> <yyyy> <hour> <min> <sec>\n");
    }
}


static void print_datetime()
{
    datetime_t t = {0};
    rtc_get_datetime(&t);
    printf("Pico RTC date/time: %i-%i-%i (%i)  %i:%i:%i\n",
            t.day, t.month, t.year, t.dotw, t.hour, t.min, t.sec);
}


static void run_command(struct terminal *trm)
{
    trm->c[trm->nchar] = '\0';
    printf("\n   --> %s\n", trm->c);

    if ( strcmp(trm->c, "ds") == 0 ) {
        ds3231_status();
    }

    if ( strcmp(trm->c, "tt") == 0 ) {
        print_datetime();
    }

    if ( strncmp(trm->c, "set ", 4) == 0 ) {
        set_clock(trm->c+4);
    }

    if ( strcmp(trm->c, "setds") == 0 ) {
        set_ds3231_from_picortc();
    }

    if ( strcmp(trm->c, "help") == 0 ) {
        printf("Commands:\n");
        printf("  help   : Show this help message\n");
        printf("  tt     : Show Pico RTC date/time\n");
        printf("  ds     : Show DS3231 date/time and status\n");
        printf("  set    : Set date/time\n");
    }
}


static void chars_avail(void *vp)
{
    struct terminal *trm = vp;
    int i;

    i = getchar_timeout_us(0);
    if ( i == PICO_ERROR_TIMEOUT ) return;

    if ( (i == 13) || (i == 10) ) {
        printf("\n");
        run_command(trm);
        printf("\nmorningtown> ");
        trm->nchar = 0;
        return;
    }

    if ( i == 21 ) {
        trm->nchar = 0;
        printf("\r                                                       \rmorningtown> ");
        return;
    }

    if ( i == 8 ) {
        printf("%c %c", i, i);
        trm->nchar--;
        return;
    }

    if ( trm->nchar == 256 ) return;
    trm->c[trm->nchar++] = i;
    printf("%c", i);
}


void terminal_init()
{
    struct terminal *trm = &terminal_data;
    trm->nchar = 0;
    printf("\n\nmorningtown> ");
    stdio_set_chars_available_callback(chars_avail, trm);
}
