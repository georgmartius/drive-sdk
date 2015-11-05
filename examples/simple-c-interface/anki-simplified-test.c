/*
 *  example program for simplified interface to anki cars via drive sdk
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
#include "anki-simplified.h"
#include <stdio.h>
#include <stdlib.h>

print_loc(AnkiHandle h){
  localization_t loc;
  loc=anki_s_get_localization(h);
  printf("Location: segm: %03x subsegm: %03x clock-wise: %i last-update: %i\n",
	 loc.segm, loc.subsegm, loc.is_clockwise, loc.update_time);
}

int main(int argc, char *argv[])
{
  if(argc<3){
    fprintf(stderr, "usage: %s adapter car-MAC [verbose]\n",argv[0]);
    exit(0);
  }
  const char* adapter = argv[1];
  const char* car_id  = argv[2];
  int i;
  AnkiHandle h = anki_s_init(adapter, car_id, argc>3);
  if(!h) return 1;
  printf("Setting initial speed 1000\n");
  if(anki_s_set_speed(h,1000,20000)!=0) return 1;
  for(i=0; i<10; i++){ usleep(200000); printf("changing lane -40\n"); anki_s_cancel_lane_change(h); anki_s_change_lane(h,-40,100,1000); print_loc(h); }
  printf("Setting slow speed 500\n");
  anki_s_set_speed(h,500,20000);
  for(i=0; i<5; i++){ usleep(100000); printf("changing lane +20\n");   anki_s_cancel_lane_change(h); anki_s_change_lane(h,20,200,4000);  print_loc(h); }
  //for(i=0; i<20; i++){ usleep(200000);  anki_s_cancel_lane_change(h); anki_s_change_lane(h,-40,100,1000); }
  for(i=0; i<10; i++){ usleep(1000000);  printf("testing locations\n"); print_loc(h);  }
  printf("Uturn\n");
  usleep(1000000);
  printf("Stopping (smoothly)\n"); anki_s_set_speed(h,0,500);
  for(i=0; i<14; i++){ usleep(1000000);  printf("speeding up to %d\n",i*100); anki_s_set_speed(h,i*100,10000);  print_loc(h); }
  for(i=0; i<14; i++){ usleep(1000000);  printf("lowing down to %d\n",1400-i*100); anki_s_set_speed(h,1400-i*100,10000);  print_loc(h); }
  printf("Uturn\n");
  anki_s_uturn(h);
  printf("Some more speed changes and lane changes\n");
  anki_s_set_speed(h,1200,20000);
  for(i=0; i<10; i++){ usleep(300000);  print_loc(h);  }
  anki_s_change_lane(h,-50,100,1000);
  for(i=0; i<10; i++){ usleep(300000);  print_loc(h);  }
  anki_s_set_speed(h,0,20000);
  sleep(1);
  anki_s_close(h);
  //sleep(5);
  //AnkiHandle h2 = anki_s_init(adapter, car_id, argc>3);
  //usleep(500000);
  //if(anki_s_set_speed(h2,1000,20000)!=0) return 1;
  //for(i=0; i<5; i++){ usleep(200000);  print_loc(h);  }
  //anki_s_set_speed(h2,0,20000);
  //usleep(200000);
  //anki_s_close(h2);
  return 0;
}
