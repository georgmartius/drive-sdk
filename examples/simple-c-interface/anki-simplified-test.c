
#include "anki-simplified.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  if(argc<3){
    fprintf(stderr, "usage: %s adapter car-MAC\n",argv[0]);
    exit(0);
  }
  const char* adapter = argv[1];
  const char* car_id  = argv[2];
  anki_s_init(adapter, car_id);
  sleep(1);
  anki_s_set_speed(1000,20000);
  sleep(1);
  anki_s_set_speed(200,20000);
  sleep(1);
  anki_s_set_speed(1000,20000);
  sleep(1);
  anki_s_set_speed(0,20000);
  sleep(1);
  anki_s_close();

}
