import dbus
import time
import sys
import os
from random import randint

bus = dbus.SessionBus()

videoControlBus          = bus.get_object('org.sflphone.SFLphone', '/org/sflphone/SFLphone/VideoControls')
videoControl             = dbus.Interface(videoControlBus, dbus_interface='org.sflphone.SFLphone.VideoControls')

while True:
 time.sleep(2)
 videoControl.startCamera()
 time.sleep(2)
 videoControl.stopCamera()
