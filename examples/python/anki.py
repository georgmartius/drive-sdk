#!/usr/bin/python
from time import sleep
from ctypes import cdll, Structure, c_int, c_void_p

class location_data(Structure):
    _fields_=[("segm", c_int),
              ("subsegm", c_int),
              ("is_clockwise", c_int),
              ("update_time", c_int),
              ("num_uncounted_transitions", c_int),
              ("is_delocalized", c_int),]

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
        self.anki_set_speed = self.anki_lib.anki_s_set_speed
        self.anki_set_speed.restype = c_int
        self.anki_uturn = self.anki_lib.anki_s_uturn
        self.anki_uturn.restype = c_int
        self.anki_change_lane = self.anki_lib.anki_s_change_lane
        self.anki_change_lane.restype = c_int
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

    def uturn(self):
        return self.anki_uturn(self.handle)

    def change_lane(self, offset, speed=None, accel=None):
        if speed is None:
            speed = self.speed
        if accel is None:
            accel = 1000
        return  self.anki_change_lane(self.handle, offset, speed, accel)


if __name__ == "__main__":

    RHO = "E6:D8:52:F1:D9:43"
    BOSON = "D9:81:41:5C:D4:31"
    KATAL = "D8:64:85:29:01:C0"
    KOURAI = "EB:0D:D8:05:CA:1A"

    car = Car(RHO)
    if not car:
        print "Couldn't create Car object"
        raise SystemExit

    err = car.connect()
    if err:
        print "Couldn't connect, code", err
        raise SystemExit

    status = car.set_speed(1200, 5000)
    if status:
        print "Couldn't set speed, code",  status
        raise SystemExit

    sleep(2)
    status = car.change_lane(100, 1500, 1000)
    sleep(2)
    status = car.change_lane(-100, 100, 1000)
    sleep(5)

    if status:
        print "Couldn't set speed, code",  status
        raise SystemExit

    for i in range(2):
        loc = car.get_localization()
        print "%02x %02x %d %d" % (loc.segm, loc.subsegm, loc.is_clockwise, loc.update_time)
        sleep(0.1)

    res = car.stop()
    sleep(1)
    del car
