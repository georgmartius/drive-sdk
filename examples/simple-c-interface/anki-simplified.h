/*
 *  simplified version of the anki drive sdk
 *
 *  Copyright (C) 2015 Georg Martius
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef ANKI_SIMPLIFIED_H
#define ANKI_SIMPLIFIED_H

typedef struct localization {
  int segm    ;
  int subsegm ;
  int is_clockwise;
  int update_time ;
} localization_t;

typedef void* AnkiHandler;

/** initializes the interface to the car via bluetooth and returns a handler (0 on failure).
    adapter is typicalle "hci0" and the car_id can be determined by the vehicle_scan utility
 */
AnkiHandler anki_s_init(const char *adapter, const char *car_id, int verbose);
void anki_s_close(AnkiHandler handler);
int anki_s_set_speed(AnkiHandler handler, int speed, int accel);
int anki_s_uturn(AnkiHandler handler);
int anki_s_change_lane(AnkiHandler handler, int relative_offset, int h_speed, int h_accel);

localization_t anki_s_get_localization(AnkiHandler handler);

#endif
