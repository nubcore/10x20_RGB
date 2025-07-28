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

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"



/*
 * This code uses SPI to send data to the ws2812 LED panel.
 * The WS2812 expects each bit to last ~ 1.2 us. A "1" is transmitted as an 800ns
 * high level on a pin, followed by a 400ns low level. A "0" is transmitted as a 400ns
 * high level on a pin, followed by an 800ns low level. The data rate is 800 kbit/s
 * If we operate the SPI at 3.2 MHz, we can "emulate" this on-off pattern with 4 bits
 * per bit that must be sent to the panel. A "0" bit is emulated using the pattern
 * 1000, a "1" bit is emulated using the pattern 1110. This means that a "0" bit is
 * sent as a 312.5 ns high, followed by a 937.5 ns low signal, and that a "1" bit is
 * sent as a 837.5 ns high, followed by a 312.5 ns low signal. This is OFF-SPEC, but
 * it works. Main benefit is that one byte of data to be transmitted fits into a 32
 * bit pattern, using "only" 4 times more memory than the "real" data, as opposed to
 * 32 times as much data for the RMT perhipheral. This also makes manipulating
 * display data much more efficient. Instead of shifting colour bits and then turning
 * colour bits into 32 bits in memory, we can write a (translated) colour byte to
 * memory as a single 32 bit word.
 * It would be possible to reduce the memory consumption to 3x by using patterns
 * of three bits per "0" or "1", but that would require a lot more processing and
 * memory transfer cycles.
 */

 /*
  * for every one of the 19) indexed colors we need to define a one word pattern
  * per colour component
  * the least significant byte is transmitted firs and the most significant byte last
  * as every byte stands for 2 bits in the RGB values, this must be compensated in
  * the patterns below.
  * The order of the encoding of each RGB value bit in these 32 transmission bits is:
  * 11110000333322225555444477776666
*/
typedef struct paletteColour {
  uint32_t G;
  uint32_t R;
  uint32_t B;
};

paletteColour palette[18]= {
//      11110000333322225555444477776666
  {//black
    0b10001000100010001000100010001000,
    0b10001000100010001000100010001000,
    0b10001000100010001000100010001000
  },{//dark grey
    0b10001110100010001000100010001000,
    0b10001110100010001000100010001000,
    0b10001110100010001000100010001000
  },{// cyan
    0b10001000100010001110100010001000,
    0b10001000100010001000100010001000,
    0b10001000100010001110100010001000
  },{//light cyan
    0b10001000100010001000100010001110,
    0b10001000100010001000100010001000,
    0b10001000100010001000100010001110
  },{//yellow
    0b10001000100010001110100010001000,
    0b10001000100010001000111010001000,
    0b10001000100010001000100010001000,
  },{//light yellow
    0b10001000100010001000100010001110,
    0b10001000100010001110100010001000,
    0b10001000100010001000100010001000
  },{//magenta
    0b10001000100010001000100010001000,
    0b10001000100010001110100010001000,
    0b10001000100010001110100010001000
  },{//light magenta
    0b10001000100010001000100010001000,
    0b10001000100010001000100010001110,
    0b10001000100010001000100010001110
  },{//orange
    0b10001000100011101000100010001000,
    0b10001000100010001110100010001000,
    0b10001000100010001000100010001000
  },{//light orange
    0b10001000100010001110100010001000,
    0b10001000100010001000100010001110,
    0b10001000100010001000100010001000
  },{//blue
    0b10001000100010001000100010001000,
    0b10001000100010001000100010001000,
    0b10001000100010001110100010001000
  },{//light blue
    0b10001000100010001000100010001000,
    0b10001000100010001000100010001000,
    0b10001000100010001000100010001110
  },{//green
    0b10001000100010001110100010001000,
    0b10001000100010001000100010001000,
    0b10001000100010001000100010001000
  },{//light green
    0b10001000100010001000100010001110,
    0b10001000100010001000100010001000,
    0b10001000100010001000100010001000
  },{//red
    0b10001000100010001000100010001000,
    0b10001000100010001110100010001000,
    0b10001000100010001000100010001000
  },{//light red
    0b10001000100010001000100010001000,
    0b10001000100010001000100010001110,
    0b10001000100010001000100010001000
  },{//light grey
    0b10001000111010001000100010001000,
    0b10001000111010001000100010001000,
    0b10001000111010001000100010001000
  },{// white
    0b10001000100010001000100010001110,
    0b10001000100010001000100010001110,
    0b10001000100010001000100010001110
  }
};


#define LEDSCREENPIN 10
#define LEDSTRINGSIZE 200
#define LEDSTRINGBITS LEDSTRINGSIZE * 24 * 4

DMA_ATTR paletteColour ledDMAbuffer[LEDSTRINGSIZE];

// control structures for the spi interface attached to the led screen
static spi_device_handle_t ledScreenSpiDevice;
static spi_device_interface_config_t ledScreenSpiInterfaceCfg;
static spi_bus_config_t ledScreenSpiBusCfg;
static spi_transaction_t ledScreenSpiTransaction;





void initializeLedScreenSpi(void) {
	ledScreenSpiBusCfg = {
		.mosi_io_num = LEDSCREENPIN,
		.miso_io_num = -1,
		.sclk_io_num = -1,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = LEDSTRINGBITS
	};
	ledScreenSpiInterfaceCfg = {
		.command_bits = 0,
		.address_bits = 0,
		.mode = 0,
		.clock_speed_hz = 3200000,
		.spics_io_num = -1,
		.queue_size = 1 
	};
	ledScreenSpiTransaction = {
		.flags=0,
		.cmd=0,
		.addr=0,
		.length=LEDSTRINGBITS,// in bits
		.rxlength=0,
		.user=(void*)0,
		.tx_buffer=(void *)ledDMAbuffer,
		.rx_buffer=(void*)0
	};

	esp_err_t err;
	err = spi_bus_initialize(SPI2_HOST, &ledScreenSpiBusCfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(err);
	err = spi_bus_add_device(SPI2_HOST, &ledScreenSpiInterfaceCfg, &ledScreenSpiDevice);
	ESP_ERROR_CHECK(err);
}


uint32_t now,prev;


void setup(void)
{	
	esp_err_t err;
	Serial.begin(9600);

	initializeLedScreenSpi();

	for (int i=0; i<200; i++) {
		int v=i % 18;
		ledDMAbuffer[i]=palette[v];
	}

	err = spi_device_queue_trans(ledScreenSpiDevice, &ledScreenSpiTransaction,1);
	//ESP_ERROR_CHECK(err);
	now=millis();
	prev=now;
	for (int i=0; i<200; i++) {
	int v=random(18);
		ledDMAbuffer[i]=palette[v];
	}
}

void loop(void) {
	esp_err_t err;
	for (int j=0; j<10; j++) {
		int v=random(18);
		int i=random(200);
		ledDMAbuffer[i]=palette[v];
	}
	while (now-prev<100) {
		now=millis();
	}
	err = spi_device_queue_trans(ledScreenSpiDevice, &ledScreenSpiTransaction,1);
	prev+=100;
}
