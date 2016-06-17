#!/usr/bin/python
from time import sleep
from ctypes import cdll, Structure, c_int, c_void_p
import sys

class location_data(Structure):
    _fields_=[("segm", c_int),
              ("subsegm", c_int),
              ("is_clockwise", c_int),
              ("update_time", c_int),
              ("num_uncounted_transitions", c_int),
              ("is_delocalized", c_int),
              ("finished_change_lane", c_int),
              ("got_ping_ack", c_int),]
class Car:
    status = 'disconnected'
    speed = 0
    def __init__(self, MAC, interface='hci0'):
        self.interface = interface
        self.MAC = MAC
        self.anki_lib = cdll.LoadLibrary('../../build/dist/lib/libankidrivesimplified.so')
        self.anki_init = self.anki_lib.anki_s_init
        self.anki_init.restype = c_void_p
        self.anki_close = self.anki_lib.anki_s_close
        self.anki_close.restype = None
        self.anki_ping = self.anki_lib.anki_s_ping
        self.anki_ping.restype = c_int
        self.anki_set_speed = self.anki_lib.anki_s_set_speed
        self.anki_set_speed.restype = c_int
        self.anki_uturn = self.anki_lib.anki_s_uturn
        self.anki_uturn.restype = c_int
        self.anki_change_lane = self.anki_lib.anki_s_change_lane
        self.anki_change_lane.restype = c_int
        self.anki_cancel_lane_change = self.anki_lib.anki_s_cancel_lane_change
        self.anki_cancel_lane_change.restype = c_int
        self.anki_get_localization = self.anki_lib.anki_s_get_localization
        self.anki_get_localization.restype = location_data
        self.anki_is_connected = self.anki_lib.anki_s_is_connected
        self.anki_is_connected.restype = c_int

    def __del__(self):
        if self.status == 'connected':
            self.anki_close(self.handle)
            self.status = 'disconnected'

    def connect(self, retries=1):
        self.status = 'connecting'
        for attempts in range(retries):
            self.handle = self.anki_init(self.interface, self.MAC, 1)
            if self.handle:
                self.status = 'connected'
                return 0
            else:
                sleep(1)

        self.status = 'disconnected'
        return 1

    def disconnect(self):
        if self.status == 'connected':
            self.stop()
            sleep(1)
            self.anki_close(self.handle)
            self.status = 'disconnected'
            sleep(1)
        return

    def set_speed(self, val, accel=5000):
        status = self.anki_set_speed(self.handle, val, accel)
        if status == 0:
            self.speed = val
        return status

    def stop(self):
        return self.set_speed(0, 500) # slow fade out looks cooler than instant stop

    def get_localization(self):
        return  self.anki_get_localization(self.handle)

    def change_lane_finished(self):
        return  self.anki_get_localization(self.handle).finished_change_lane;

    def got_ping_ack(self):
        return  self.anki_get_localization(self.handle).got_ping_ack;

    def uturn(self):
        return self.anki_uturn(self.handle)

    def ping(self):
        return self.anki_ping(self.handle)

    def change_lane(self, offset, speed=None, accel=None):
        if speed is None:
            speed = self.speed
        if accel is None:
            accel = 1000
        return  self.anki_change_lane(self.handle, offset, speed, accel)

    def cancel_change_lane(self):
        return  self.anki_cancel_lane_change(self.handle)


if __name__ == "__main__":

    RHO    = "E6:D8:52:F1:D9:43"
    BOSON  = "D9:81:41:5C:D4:31"
    KATAL  = "D8:64:85:29:01:C0"
    KOURAI = "EB:0D:D8:05:CA:1A"
    NUKE   = "C5:34:5D:26:BE:53"
    HADION = "D4:48:49:03:98:95"

    car = Car(RHO)
    if not car:
        print "Couldn't create Car object"
        raise SystemExit

    err = car.connect()
    if err:
        print "Couldn't connect, code", err
        raise SystemExit

    status = car.set_speed(500, 5000)
    if status:
        print "Couldn't set speed, code",  status
        raise SystemExit

    # go to the right and the left continuously and wait until finised
    for x in range(40):
        offset= -80 if (x // 6) % 2 ==0 else 80
        #car.cancel_change_lane()
        status = car.change_lane(offset, 300, 1000)
        sys.stdout.write("waiting")
        while not car.change_lane_finished():
            sleep(0.05)
            sys.stdout.write(".")
        sys.stdout.write("\n")
    car.ping()
    while not car.got_ping_ack():
        sleep(0.05)
        sys.stdout.write("x")
    sys.stdout.write("\n")

    print "Stop car"
    status = car.set_speed(0, 10000)
    sleep(1)
    status = car.set_speed(1000, 10000)
    sleep(1)
    # speed up and slow down
    for x in range(40):
        status = car.set_speed((x % 6)*100+700, 5000)
        sleep(0.1)
    print "Stop car"
    status = car.set_speed(0, 10000)
    sleep(1)
    status = car.set_speed(500, 10000)
#    car.cancel_change_lane()
    if status:
        print "Couldn't set speed, code",  status
        raise SystemExit

    for i in range(5):
        loc = car.get_localization()
        print "segm %02x subsegm %02x clockwise %d update %d" % (loc.segm, loc.subsegm, loc.is_clockwise, loc.update_time)
        sleep(0.5)
    car.uturn()
    for i in range(5):
        loc = car.get_localization()
        print "segm %02x subsegm %02x clockwise %d update %d" % (loc.segm, loc.subsegm, loc.is_clockwise, loc.update_time)
        sleep(0.5)


    res = car.stop()
    sleep(1)
    del car
