/*
 * settings.h
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

struct mt_settings
{
    uint32_t signature;
    uint32_t version;

    uint32_t morning_hour;
    uint32_t morning_min;
    uint32_t late_hour;
    uint32_t late_min;
    uint32_t clear_hour;
    int32_t utc_offset;
    uint32_t morning_pin;
    uint32_t late_pin;

    char pad[54];  /* Pad to at least FLASH_PAGE_SIZE */
};


extern struct mt_settings settings;
extern int settings_read(void);
extern int settings_write(void);
extern void settings_show(void);
extern int32_t dst(datetime_t t);
