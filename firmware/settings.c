/*
 * settings.c
 *
 * Copyright © 2026 Thomas White <taw@physics.org>
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


#include <hardware/flash.h>
#include <hardware/sync.h>
#include <hardware/rtc.h>
#include <stdio.h>
#include <time.h>

#include "settings.h"


struct mt_settings settings;
const int signature = 0x4e54574d;   /* "MTWN" */
const size_t last_sector = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
const int n_pages = FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE;

static void settings_default(struct mt_settings *s)
{
    s->signature = signature;
    s->version = 0;
    s->morning_hour = 7;
    s->morning_min = 15;
    s->late_hour = 8;
    s->late_min = 0;
    s->clear_hour = 12;
    s->morning_pin = 19;  /* Use 21 for cheap and cheerful hardware */
    s->late_pin = 22;
    s->utc_offset = 1;
}


void settings_show()
{
    printf("Settings version %i\n", settings.version);
    printf(" Wake after %02i:%02i\n", settings.morning_hour, settings.morning_min);
    printf(" Rise before %02i:%02i\n", settings.late_hour, settings.late_min);
    printf(" Clear LEDs at %02i:00\n", settings.clear_hour);
    printf(" Local time is UTC + %i hours\n", settings.utc_offset);
    printf(" LED assignments wake=%i, rise=%i\n", settings.morning_pin, settings.late_pin);
}


int settings_read()
{
    int i;
    struct mt_settings *sp = NULL;
    int max_version = 0;

    printf("Sector size = %li\n", FLASH_SECTOR_SIZE);
    printf("Page size = %li\n", FLASH_PAGE_SIZE);
    printf("Pages per sector = %i\n", n_pages);
    printf("Last sector is at %p\n", last_sector+XIP_BASE);

    for ( i=0; i<n_pages; i++ ) {

        struct mt_settings *spm = (struct mt_settings *)(XIP_BASE+last_sector+i*FLASH_PAGE_SIZE);

        printf("Checking page %i (at %p) ... ", i, spm);

        if ( (spm->signature == signature ) && (spm->version > max_version) ) {
            printf("version %i\n", spm->version);
            sp = spm;
        } else {
            if ( spm->signature != signature ) {
                printf(" (no signature)\n");
            } else {
                printf(" (version too low: %i < %i)\n", spm->version, max_version);
            }
        }

    }
    if ( sp != NULL ) {
        printf("Found settings version %i at %p\n", sp->version, sp);
        settings = *sp;
    } else {
        printf("No settings found, using defaults\n");
        settings_default(&settings);
    }

    return 0;
}


int settings_write()
{
    int i;
    uint32_t v;

    if ( settings.version != 0 ) {

        int i;

        for ( i=0; i<n_pages; i++ ) {

            struct mt_settings *spm = (struct mt_settings *)(XIP_BASE+last_sector+i*FLASH_PAGE_SIZE);

            if ( spm->signature != signature ) {
                settings.version++;
                printf("Saving settings version %i at %p (page %i)\n",
                        settings.version, spm, i);
                flash_range_program(last_sector+i*FLASH_PAGE_SIZE, (char *)&settings, FLASH_PAGE_SIZE);
                return 0;
            }

        }

    }

    /* No pages found, or this is the first save on this chip */
    printf("No unused pages found - clearing sector\n");
    v = save_and_disable_interrupts();
    flash_range_erase(PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE,
                      FLASH_SECTOR_SIZE);
    settings.version++;
    flash_range_program(last_sector, (char *)&settings, FLASH_PAGE_SIZE);
    restore_interrupts(v);
    return 0;
}


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
int32_t dst(datetime_t t)
{
    /* April to September inclusive. */
    if ( (t.month >= 4) && (t.month <= 9) ) return 1;

    if ( (t.month == 3) && (t.day >= last_sunday_in_march(t.year)) ) return 1;
    if ( (t.month == 10) && (t.day < last_sunday_in_october(t.year)) ) return 1;

    return 0;
}
