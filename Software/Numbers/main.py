from neopixel import NeoPixel
from time import sleep_ms
from machine import Pin
import random

i = 0

r = 22
g = 2
b = 2

ms = 50

p = Pin(10, Pin.OUT)
np = NeoPixel(p, 200)
np.fill((0,0,0))
np.write()

def num_1():
 c = 4 * 20
 for i in range(20):
  np[i+c] = (r,g,b)
 c = 5 * 20
 for i in range(20):
  np[i+c] = (r,g,b)

 for i in range(10):
  c = i * 20
  for x in range(18,20):
   np[x+c] = (r,g,b)

 np[23] = (r,g,b)
 np[24] = (r,g,b)

 np[42] = (r,g,b)
 np[43] = (r,g,b)

 np[61] = (r,g,b)
 np[62] = (r,g,b)

num_1()
np.write()


#second_clock()

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

def second_clock():
 while 1:
  cykel_pixel()
  sleep_ms(ms)
