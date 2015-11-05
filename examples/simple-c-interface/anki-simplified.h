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

/**
   Self-localization information
*/
typedef struct localization {
  int segm    ;    ///< Track piece (between 0-255)
  int subsegm ;    ///< Lane and/or subpiece information? (between 0-255)
  int is_clockwise; ///< direction of driving (0: ccw, 1: cw)
  int update_time ; ///< Last update time (is increased every time we get an update from the car)
  int num_uncounted_transitions ; ///< number of segment transitions where we did not get a localization yet
  int is_delocalized ; ///< if 1 the car lost the track
} localization_t;

typedef void* AnkiHandle;

/** initializes the interface to the car via bluetooth adapter.
    @param adapter name of bluetooth device, typically "hci0"
    @param car_id MAC-address of car, can be determined with the vehicle_scan utility
    @return handle  or 0 (NULL) on failure.
 */
AnkiHandle anki_s_init(const char *adapter, const char *car_id, int verbose);
/// closes the connection
void anki_s_close(AnkiHandle handle);
/** set speed of the car
    @param speed value in mm/s between 0 (stop) and 5000? (negative values are unpredictable)
    @param accel accelation value in mm/s^2 (default 25000)
    @return 0 for okay, 1 for failure
 */
int anki_s_set_speed(AnkiHandle handle, int speed, int accel);
/** set let the car do a u-turn (direction of turning seems to be is autonomously decided)
    @return 0 for okay, 1 for failure
 */
int anki_s_uturn(AnkiHandle handle);
/** set let the car change a lane
    @param relative_offset value in mm relative to current lane (polarity ?)
    @param h_speed value in mm/s. (default 100)
    @param h_accel value in mm/s^ (default 500)
    @return 0 for okay, 1 for failure
 */
int anki_s_change_lane(AnkiHandle handle, int relative_offset, int h_speed, int h_accel);

/** cancels last change command
    @return 0 for okay, 1 for failure
 */
int anki_s_cancel_lane_change(AnkiHandle handle);

/** returns 1 if connected and 0 if not*/
int anki_s_is_connected(AnkiHandle handle);

/** returns the last self-localization of the car
    @return locatization struct
 */
localization_t anki_s_get_localization(AnkiHandle handle);

#endif
