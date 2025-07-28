Copyright (c) 2025, Theo Borm

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#define LEDPIN 10
#define SCREENWIDTH 10
#define SCREENHEIGHT 20
#define STRINGSIZE SCREENWIDTH * SCREENHEIGHT
#define STRINGBITS STRINGSIZE * 24


/*
 * In this example, he LED screen is controlled using the RMT perhipheral.
 * the RMT perhipheral can generate the necessary waveforms to drive WS2812 at the cost
 * of rather high memory usage: every bit in the stream must be represented by an RMT
 * data structure of 32 bits. Each bit is transmitted by the RMT in two phases.
 * The 32 bit data structure is split in two 16 bit values, one for each phase.
 * Each 16 bit data word contains 1 bit for the pin level of that phase and 15 bits for
 * the duration of that phase.
 * The WS2812 12 expects each bit to last ~ 1.2 us. A "1" is transmitted as an 800ns
 * high level on a pin, followed by a 400ns low level. A "0" is transmitted as a 400ns
 * high level on a pin, followed by an 800ns low level. If the RMT is operated at a
 * 10MHz clock speed (100ns), then this means that for a "1", high and low phases of
 * 8 and 4 cycles respectively should be used, and for a "0" these phases should be
 * 4 and 8 cycles respectively.
 */
#define DEBUG 1

// an array to hold the data for the LED screen in RMT format.
// every rmt_data_t is 32 bits, and we need 1 per bit we need to send
rmt_data_t led_RMT_data[STRINGBITS];


// sets up data for a single LED
void set_single_led_RMT_data(int index , uint32_t grb) {
  int a=index * 24;
  for (int c=0; c<3; c++){ // go through the bytes in the GRB value
    int v=(grb >> (c*8)) & 0xff; // extract the byte value
    for (int b=0; b<8; b++) { // go through the bits of the byte
        if (v & (1 << (7-b))) { // if the bit is "1", then...
          led_RMT_data[a + c * 8 + b].level0 = 1;
          led_RMT_data[a + c * 8 + b].duration0 = 8;
          led_RMT_data[a + c * 8 + b].level1 = 0;
          led_RMT_data[a + c * 8 + b].duration1 = 4;
        } else { // if the bit is "0", then...
          led_RMT_data[a + c * 8 + b].level0 = 1;
          led_RMT_data[a + c * 8 + b].duration0 = 4;
          led_RMT_data[a + c * 8 + b].level1 = 0;
          led_RMT_data[a + c * 8 + b].duration1 = 8;
        }
    }
  }
}

// set all LEDs to the same value
void set_all_leds_RMT_data(uint32_t grb) {
  for (int i=0; i<STRINGSIZE; i++) set_single_led_RMT_data(i,grb);
}

void setup() {
  Serial.begin(115200);
  while (!rmtInit(LEDPIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) { 
    if (DEBUG) Serial.println("RMT controller initializtion failed\n");
  }
  set_all_leds_RMT_data(0x000000);
}

void walklight(uint32_t c) {
  for (int l=0; l< STRINGSIZE; l++) {
    set_single_led_RMT_data(l , c);
    rmtWrite(LEDPIN, led_RMT_data, STRINGBITS, RMT_WAIT_FOR_EVER);
    set_single_led_RMT_data(l , 0x000000);
    delay(25);
  }
}

void loop() {
  walklight(0x0000ff);
  walklight(0x00ff00);
  walklight(0x00ffff);
  walklight(0xff0000);
  walklight(0xff00ff);
  walklight(0xffff00);
  walklight(0xffffff);
}
