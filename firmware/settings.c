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
#include <stdio.h>

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
}


int settings_read()
{
    int i;
    struct mt_settings *sp;
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
