
#ifndef ANKI_SIMPLIFIED_H
#define ANKI_SIMPLIFIED_H

typedef struct localization {
  int segm    ;
  int subsegm ;
  int is_clockwise;
  int update_time ;
} localization_t;

int anki_s_init(const char *adapter, const char *car_id);
void anki_s_close();
int anki_s_set_speed(int speed, int accel);
int anki_s_u_turn();
int anki_s_change_lane(int relative_offset, int h_speed, int h_accel);

localization_t anki_s_get_localization();

#endif
