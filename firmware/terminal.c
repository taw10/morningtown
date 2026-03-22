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
#include "terminal.h"
#include "settings.h"

struct terminal
{
    char c[256];
    int nchar;
};

struct terminal terminal_data;


static void set_pins(const char *str)
{
    int morning, late;
    if ( sscanf(str, "%i %i", &morning, &late) == 2 ) {
        settings.morning_pin = morning;
        settings.late_pin = late;
    } else {
        printf("Syntax: leds <morning> <late>\n");
        printf("Default: leds 19 22\n");
    }
}


static void set_wake(const char *str)
{
    int hours, mins;
    if ( sscanf(str, "%i %i", &hours, &mins) == 2 ) {
        settings.morning_hour = hours;
        settings.morning_min = mins;
    } else {
        printf("Syntax: wake <hh> <mm>\n");
        printf("Default: wake 7 15\n");
    }
}


static void set_rise(const char *str)
{
    int hours, mins;
    if ( sscanf(str, "%i %i", &hours, &mins) == 2 ) {
        settings.late_hour = hours;
        settings.late_min = mins;
    } else {
        printf("Syntax: rise <hh> <mm>\n");
        printf("Default: wake 8 0\n");
    }
}


static void set_utc_offset(const char *str)
{
    int hours;
    if ( sscanf(str, "%i", &hours) == 1 ) {
        settings.utc_offset = hours;
    } else {
        printf("Syntax: tz <hours>\n");
        printf("Default (for CET/CEST): tz 1\n");
    }
}


static void set_clear_time(const char *str)
{
    int hours;
    if ( sscanf(str, "%i", &hours) == 1 ) {
        settings.clear_hour = hours;
    } else {
        printf("Syntax: clear <hours>\n");
        printf("Default: clear 12\n");
    }
}


static void set_clock(const char *str)
{
    int dow, d, mon, y, h, m, s;
    if ( sscanf(str, "%i %i %i %i %i %i %i", &dow, &d, &mon, &y, &h, &m, &s) == 7 ) {

        datetime_t t;
        bool r;

        printf("OK %i-%i-%i (%i), %i:%i:%i\n", d, mon, y, dow, h, m, s);

        t.year = y;
        t.month = mon;
        t.day = d;
        t.dotw = dow;
        t.hour = h;
        t.min = m;
        t.sec = s;
        if ( !rtc_set_datetime(&t) ) {
            printf("RTC set failed\n");
        } else {
            printf("OK!\n");
        }

    } else {
        printf("Syntax: set <weekday> <day> <mon> <yyyy> <hour> <min> <sec>\n");
        printf("Note: Time should be in UTC\n");
        printf("<weekday> = 0..6 for Sunday..Saturday\n");
    }
}


static const char *dotw_pico(int n)
{
    switch (n) {
        case 0 : return "Sunday";
        case 1 : return "Monday";
        case 2 : return "Tuesday";
        case 3 : return "Wednesday";
        case 4 : return "Thursday";
        case 5 : return "Friday";
        case 6 : return "Saturday";
        default : return "UNKNOWN";
    }
}


static void print_datetime()
{
    datetime_t t = {0};
    int r;
    r = rtc_get_datetime(&t);
    if ( r ) {
        printf("Pico RTC date/time (UTC): %i-%i-%i (%s)  %i:%i:%i\n",
               t.day, t.month, t.year, dotw_pico(t.dotw), t.hour, t.min, t.sec);
    } else {
        printf("Pico RTC not running.\n");
    }
}


static void run_command(struct terminal *trm)
{
    trm->c[trm->nchar] = '\0';

    if ( trm->nchar == 0 ) return;

    if ( strcmp(trm->c, "ds") == 0 ) {
        ds3231_status();

    } else if ( strcmp(trm->c, "tt") == 0 ) {
        print_datetime();

    } else if ( strncmp(trm->c, "set ", 4) == 0 ) {
        set_clock(trm->c+4);

    } else if ( strcmp(trm->c, "setds") == 0 ) {
        set_ds3231_from_picortc();

    } else if ( strcmp(trm->c, "osf") == 0 ) {
        ds3231_reset_osf();

    } else if ( strcmp(trm->c, "load") == 0 ) {
        settings_read();

    } else if ( strcmp(trm->c, "save") == 0 ) {
        settings_write();

    } else if ( strcmp(trm->c, "settings") == 0 ) {
        settings_show();

    } else if ( strncmp(trm->c, "leds ", 5) == 0 ) {
        set_pins(trm->c+5);

    } else if ( strncmp(trm->c, "wake ", 5) == 0 ) {
        set_wake(trm->c+5);

    } else if ( strncmp(trm->c, "rise ", 5) == 0 ) {
        set_rise(trm->c+5);

    } else if ( strncmp(trm->c, "tz ", 3) == 0 ) {
        set_utc_offset(trm->c+3);

    } else if ( strncmp(trm->c, "clear ", 6) == 0 ) {
        set_utc_offset(trm->c+6);

    } else if ( strcmp(trm->c, "help") == 0 ) {
        printf("Commands:\n");
        printf("  help     : Show this help message\n");
        printf("  tt       : Show Pico RTC date/time\n");
        printf("  set      : Set UTC date/time\n");
        printf("  ds       : Show DS3231 date/time and status\n");
        printf("  setds    : Set DS3231 from Pico RTC\n");
        printf("  osf      : Reset DS3231 stop flag\n");
        printf("  load     : Load settings\n");
        printf("  save     : Save settings\n");
        printf("  settings : Show settings\n");
        printf("  leds     : Set wake LED pin assignments\n");
        printf("  wake     : Set waking time (green)\n");
        printf("  rise     : Set rise/late time (red)\n");
        printf("  clear    : Set wake LED reset time\n");

    } else {
        printf("Command not recognised.  Try 'help'\n");
    }

}


void terminal_poll(Terminal *trm)
{
    int i;

    i = getchar_timeout_us(0);
    if ( i == PICO_ERROR_TIMEOUT ) return;
    if ( i == 0 ) return;

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


Terminal *terminal_init()
{
    struct terminal *trm = &terminal_data;
    trm->nchar = 0;
    printf("\n\nmorningtown> ");
    return trm;
}
