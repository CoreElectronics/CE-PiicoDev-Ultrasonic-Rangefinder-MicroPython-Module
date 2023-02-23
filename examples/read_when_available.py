# Only read the Rangefinder when a new sample is available

from PiicoDev_Ultrasonic import PiicoDev_Ultrasonic
from PiicoDev_Unified import sleep_ms
 
ranger = PiicoDev_Ultrasonic() # Initialise the rangefinder
ranger.period_ms = 1000 # set a slow sample period

while True:
    if ranger.new_sample_available:
        print(ranger.distance_mm)
        
    sleep_ms(100)