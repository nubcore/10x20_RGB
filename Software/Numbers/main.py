from neopixel import NeoPixel
from time import sleep_ms
from machine import Pin
import random

i = 0

r = 22
g = 22
b = 22

ms = 50

p = Pin(10, Pin.OUT)
np = NeoPixel(p, 200)
np.fill((0,0,0))
np.write()

def rand_pixel():
 return (random.randint(0,r), random.randint(0,g),  random.randint(0,b))

def cykel_pixel():
 global i
 np.fill((0,0,0))
 np[i] = rand_pixel()
 np.write()
 i = i + 1
 if i > 199:
  i = 0

def test(ms=2):
 for x in range(200):
  np.fill((0,0,0))
  np[x] = (r,g,b)
  np.write()
  sleep_ms(10)


while 1:
 cykel_pixel()
 sleep_ms(ms)
