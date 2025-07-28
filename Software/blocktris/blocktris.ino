/**********************************************************************************
 * Copyright (c) 2025, Theo Borm
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define ACCESSPOINTSSID "bornhack"
#define ACCESSPOINTPASSWORD ""

/*******************************************************************************************************************
* Arduino sucks.
*
* Really, The Arduino IDE is a multi-level clusterfuck for everything but the simplest projects.
*
* This could (or maybe should) be a multi-page rant about what is wrong with ARDUINO, and there
* might be some entertainment value in that, but I simply don't have the time for it, so I'll
* be short.
*
* There is NO project management in ARDUINO, and if you split source across multiple files
* you end up in dependency hell because the ARDUINO IDE breaks C/C++ programming standards.
* Splitting complex source into multiple units along functional bounfaries is a way to keep
* code understandable and manageable, and is something programmers deal with on a daily basis.
* Not in ARDUINO IDE, though. The ARDUINO IDE adds some "Magic" to the compilation process which
* (I guess) makes it slightly easier to get started, but which breaks normal dependency management.
*
* While it may very well be possible to work around the limitations and create a multi-source-
* file project, with sources split in a way that makes some sense from a software maintainance
* point of view, I simply don't have the time nor energy to jump through these hoops, So I
* restructured everything so that it at least works: one big file. As a convenience I've split
* the code into "functional sections" with markers on the boundaries.
*
*******************************************************************************************************************/


/*******************************************************************************************************************
* INCLUDES section for external header files
*******************************************************************************************************************/
#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
// for high score server
#include <WiFi.h>
#include <HTTPClient.h>

/*******************************************************************************************************************
* TYPEDEFINITIONS section here because ARDUINO IDE does weird stuff with them
*******************************************************************************************************************/
// a struct holding the 3 32 bit encoded colour values in the required order
typedef struct {
  uint32_t G;
  uint32_t R;
  uint32_t B;
} paletteColour;

// a shape has a type, orientation and a position. the natural color of a shape is t * 2 (+1 for bright)
typedef struct {
  int8_t t;
  int8_t r;
  int8_t x;
  int8_t y;
} shape;

// when rotating pieces, they can sometimes be "kicked" into a different position
typedef struct {
  int8_t x;
  int8_t y;
} kick;

/*******************************************************************************************************************
* BOARDSUPPORT section
*******************************************************************************************************************/
// some basic stuff for working (only) with boards. Depends on nothing internally, but other sections depend on this.
#define BOARDWIDTH 10
#define BOARDHEIGHT 20
#define SPAWNHEIGHT 5

// the "board" on which we play. "spawn" area is above the play area, and is  invisible
uint8_t board[BOARDHEIGHT+SPAWNHEIGHT][BOARDWIDTH]; // spawn area = rows above board

// returns true or false if a row is completely full or not
int8_t isBoardRowFull(uint8_t y) {
  for (int8_t x=0; x<BOARDWIDTH; x++) {
    if (board[y][x] <= 1) return(0); //no
  }
  return(1); // yes
}

// returns the index of the first full row from the bottom, or -1 if there is none
int8_t findFirstFullRow(void) {
  for (int8_t i=0; i<BOARDHEIGHT+SPAWNHEIGHT; i++) {
    if (isBoardRowFull(i)) return(i);
  }
  return(-1); // no full row
}

// returns the end (+1) of a block of full rows from the bottom given a starting row.
int8_t findLastFullRowInBlock(int8_t sr) { 
  for (int8_t i=sr+1; i<BOARDHEIGHT+SPAWNHEIGHT; i++) {
    if (isBoardRowFull(i)==0) return(i); // not full, so previous line was last
  }
  return(BOARDHEIGHT+SPAWNHEIGHT);
}

/*******************************************************************************************************************
* PALETTESUPPORT section
*******************************************************************************************************************/
// some basic stuff for working (only) with palettes translating indexed colour values into encoded values to be
// sent to the WS2812 string. Depends on nothing internally, but other sections depend on this.

void mangle(paletteColour * p){
  p->B=0xf0f0f0f0;
}
// a standard palette with 18 colours
paletteColour standardPalette[18]= {
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

// Encode an 8 bit value into a 32 bit value
uint32_t SpiLedScreenEncodeValue(uint8_t v) {
  uint32_t r=0b10001000100010001000100010001000; //encoding of all zero bits
  if (v&  1) r |= 0b10001110100010001000100010001000;
  if (v&  2) r |= 0b11101000100010001000100010001000;
  if (v&  4) r |= 0b10001000100011101000100010001000;
  if (v&  8) r |= 0b10001000111010001000100010001000;
  if (v& 16) r |= 0b10001000100010001000111010001000;
  if (v& 32) r |= 0b10001000100010001110100010001000;
  if (v& 64) r |= 0b10001000100010001000100010001110;
  if (v&128) r |= 0b10001000100010001000100011101000;
  return(r);
 }

 // fill a single palette entry with the appropriate requested value
  void SpiLedScreenCreateColour(paletteColour * p, uint8_t R, uint8_t G, uint8_t B) {
  /*
    p->R=SpiLedScreenEncodeValue(R);
    p->G=SpiLedScreenEncodeValue(G);
    p->B=SpiLedScreenEncodeValue(B);
    */
 }


/*******************************************************************************************************************
* SPILEDSCREEN section
*******************************************************************************************************************/
// basic SPI+DMA hardware LED screen driver. Depends on PALETTESUPPORT internally, but other sections depend on this.
/*
 * This code uses SPI to send data to the ws2812 LED panel(s).
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
 * It would be possible to reduce the memory consumption by a quarter by using patterns
 * of three bits per "0" or "1", but that would require a lot more processing and
 * memory transfer cycles.
 *
 * Of course, having to encode 24 bit RGB values into 3 32 bits words is a bit of a
 * hassle. To make this a bit simpler from a programming point of view, a pallette
 * with "standard" colour values is provided, together with functions to put these
 * values into the display buffer. If necessary, the user can also opt to generate
 * the necessary encoded values and directly put these into the display buffer.
 *
 * The RGB values must be encoded in 3 consecutive 32 bit words in the right (GRB)
 * order. Each bit of each RGB value byte must be translated in a 4 bit pattern. The
 * storage order of these patterns in the 32 bit words depends on the endianness of
 * the ESP32, and is (for each bit 0..7, with 7 the most significant in the byte):
 * 11110000333322225555444477776666
 */

/*
 * if multiple display units are used they must be strung together first 
 * from left to right, then from top to bottom. UNITWIDTH and UNITHEIGHT
 * is the width and height of a single unit. UNITSWIDE and UNITSHIGH
 * are the number of units in the array
 * The top-left pixel is the start of the complete string and the bottom-
 * right pixel the end.
 */

//default #define for the pin to which the screen is connected
#define LEDSCREENPIN 10
#
 //default #defines for geometry
#define UNITWIDTH 10
#define UNITHEIGHT 20
#define UNITSWIDE 1
#define UNITSHIGH 1

//computed from the above
#define UNITBLOCKSIZE UNITWIDTH * UNITSWIDE * UNITHEIGHT
#define TOTALSIZE UNITBLOCKSIZE * UNITSHIGH
#define LEDSTRINGSIZE TOTALSIZE
#define LEDSTRINGBITS LEDSTRINGSIZE * 24 * 4

// encoded RGB screen memory
DMA_ATTR paletteColour ledDMAbuffer[LEDSTRINGSIZE];

// control structures for the spi interface attached to the led screen
static spi_device_handle_t ledScreenSpiDevice;
static spi_device_interface_config_t ledScreenSpiInterfaceCfg;
static spi_bus_config_t ledScreenSpiBusCfg;
static spi_transaction_t ledScreenSpiTransaction;

// initialize the hardware to drive a screen
void initializeSpiLedScreen(void) {
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

// update the actual screen by sending the data to it
void updateSpiLedScreen(void) {
  esp_err_t err;
  err = spi_device_queue_trans(ledScreenSpiDevice, &ledScreenSpiTransaction,1);
}


/*******************************************************************************************************************
* DRAWSUPPORT section
*******************************************************************************************************************/
// basic routines for drawing stuff on both the board and the screen. internally depends on PALETTESUPPORT and
// BOARDSUPPORT and SPILEDSCREEN. Other sections depend on this

// unless the target is evident, routines have  a "target" which specifies what needs to happen
#define TARGETNONE 0
#define TARGETBOARD 1
#define TARGETSCREEN 2
#define TARGETBOTH 3

// set a single encoded pixel in the WS2812 output array
// the target of this function is implicit
void setEncodedScreenPixel(int8_t x, int8_t y, paletteColour v) {
  // in the display row 0 is the top row, in the game its is the bottom row
  int8_t displayy=BOARDHEIGHT-y-1;
  int8_t displayblock=displayy/UNITHEIGHT; // integer div, rounded down
  int16_t displayindex=displayy % UNITHEIGHT + x * UNITHEIGHT + displayblock * UNITBLOCKSIZE;
  ledDMAbuffer[displayindex]=v;
}

// set a single pixel in the WS2812 output array
// the target of this function is implicit
void setScreenPixel(int8_t x, int8_t y, int8_t v) {
  if (y<BOARDHEIGHT) setEncodedScreenPixel(x,y,standardPalette[v]);
}

// updates the WS2812 data array from the corresponding board array position
// the target of this function is implicit
void renderPixel(int8_t x, int8_t y) {
    if (y<BOARDHEIGHT) setScreenPixel(x, y, board[y][x]);
}

// updates a row of WS2812 data array according to the corresponding row in the board array
// the target of this function is implicit
void renderBoardRow(int8_t r) {
  if ((r<BOARDHEIGHT)&&(r>=0)) { // do not try to render any of the invisible rows
    for (int8_t x=0; x<BOARDWIDTH; x++) {
      setScreenPixel(x,r,board[r][x]);
    }
  }
}

// updates a block of WS2812 data array according to the corresponding block in the board array
// the target of this function is implicit
void renderBoardBlock(int8_t startrow, int8_t endrow) {
  if (startrow<endrow) {
    for (int8_t y=startrow; y<endrow; y++) {
      renderBoardRow(y);
    }
  }
}

// completely updates the WS2812 data array according to the board array
// the target of this function is implicit
void renderBoard(void) {
  for (int8_t y=0;y<BOARDHEIGHT; y++) {
    renderBoardRow(y);
  }
}

// set a single pixel in the board / WS2812 output array
// the target of this function is explicit
void setPixel(int8_t x, int8_t y, int8_t c, uint8_t target) {
  if (target & 1) board[y][x]=c;
  if (target & 2) setScreenPixel(x, y, c);
}

// set a single row in the board / WS2812 output array to a specific colour index
// the target of this function is explicit.
void setPixelRow (int8_t y, int8_t v, uint8_t target) {
  for (int8_t x=0; x<BOARDWIDTH; x++) {
    setPixel(x, y, v, target);
  }
}

// sets a block of rows in the board/WS2812 output array to a specific colour index
// the target of this function is explicit
void setPixelBlock(int8_t startrow, int8_t endrow, int8_t v, uint8_t target){
  for (int8_t i=startrow; i<endrow; i++) setPixelRow(i,v,target);
}

// clears all pixels in the board / WS2812 output array
// the target of this function is explicit
void clearAllPixels(uint8_t target) {
  if (target & 1) {
    for (uint8_t y=0;y<BOARDHEIGHT+SPAWNHEIGHT; y++) {
      for (uint8_t x=0; x<BOARDWIDTH; x++) {
        board[y][x]=0;
      }
    }
  }
  if (target & 2) {
    for (int i=0;i<LEDSTRINGSIZE; i++) {
      ledDMAbuffer[i]=standardPalette[0];
    }
  }
}

// copies a row of pixels. If target includes screen, the copy is made by rendering the board row
// the target of this function is explicit
void copyPixelRow(int8_t fromrow, int8_t torow, uint8_t target) {
  if (target & 1) {
    for (uint8_t x=0; x<BOARDWIDTH; x++) {
      board[torow][x]=board[fromrow][x];
    }
  }
  if (target & 2) {
    for (uint8_t x=0; x<BOARDWIDTH; x++) {
      setScreenPixel(x,torow,board[fromrow][x]);
    }
  }
}

// moves al rows >=endrow toward startrow, backfilling the vacated area with the specified colour v
// the target of this function is explicit
void removePixelRowBlock(int8_t startrow, int8_t endrow, int8_t v, uint8_t target) {
  int8_t nrrows=BOARDHEIGHT+SPAWNHEIGHT-endrow;
  for (uint8_t i=0; i<nrrows; i++) {
    copyPixelRow(endrow+i,startrow+i,target);
  }

  for (uint8_t i=startrow+nrrows; i<BOARDHEIGHT+SPAWNHEIGHT; i++) {
    setPixelRow (i, v, target);
  }
}


/*******************************************************************************************************************
* BLOCKSSUPPORT section
*******************************************************************************************************************/
// routines for working with "falling" blocks. Depends on everything above

// every shape can be in 4 different orentations
// 0 - the spawn orientation
// 1 - rotated 90 degrees counter-clockwise (also known as "L")
// 2 - rotated bij 180 degrees (also known as "2")
// 3 - rotated 90 degrees clockwise (also known as "R")

/*
 *Shapes drawn in 4x4 boxes in different orientations 0 - 1 - 2 - 3 
 * The "centre" of the box is always at the same position:
 * ....
 * .c..
 * ....
 * ....
 * note that in case of I the "centre" can be OUTSIDE the piece itself
 *
 *  .... .#.. .... ..#.     .... .... .... ....
 *  #### .#.. .... ..#.     .##. .##. .##. .##.
 *  .... .#.. #### ..#.     .##. .##. .##. .##.
 *  .... .#.. .... ..#.     .... .... .... ....
 *
 *  .#.. .#.. .... .#..
 *  ###, ##.. ###. .##.
 *  .... .#.. .#.. .#..
 *  .... .... .... ....
 *
 *  ..#. ##,, .... .#..     #... .#.. .... .##.
 *  ###. .#.. ###. .#..     ###. .#.. ###. .#..
 *  .... .#.. #... .##.     .... ##.. ..#. .#..
 *  .... .... .... ....     .... .... .... ....
 *
 *  .##. #... .... .#..     ##.. .#.. .... ..#.
 *  ##.. ##.. .##. .##.     .##. ##.. ##.. .##.
 *  .... .#.. ##.. ..#.     .... #... .##. .#..
 *  .... .... .... ....     .... .... .... ....
 */

// every shape has a number, and the correct colour is this number * 2 (+1 if extra bright)
#define SHAPE_I 1
#define SHAPE_O 2
#define SHAPE_T 3
#define SHAPE_L 4
#define SHAPE_J 5
#define SHAPE_S 6
#define SHAPE_Z 7

// kick data for the I shape
kick Ikickdata[8][5] = {
  { {0,0},{-2,0},{1,0},{-2,-1},{1,2} },//(0->R)
  { {0,0},{-1,0},{2,0},{-1,2},{2,-1} },//(0->L)
  { {0,0},{1,0},{-2,0},{1,-2},{-2,1} },//(L->0)
  { {0,0},{-2,0},{1,0},{-2,-1},{1,2} },//(L->2)
  { {0,0},{2,0},{-1,0},{2,1},{-1,-2} },//(2->L)
  { {0,0},{1,0},{-2,0},{1,-2},{-2,1} },//(2->R)
  { {0,0},{-1,0},{2,0},{-1,2},{2,-1} },//(R->2)
  { {0,0},{2,0},{-1,0},{2,1},{-1,-2} } //(R->0)
};
// kick data for other shapes
kick Okickdata[8][5] = {
  { {0,0},{-1,0},{-1,1},{0,-2},{-1,-2} },//(0->R)
  { {0,0},{1,0},{1,1},{0,-2},{1,-2} },//(0->L)
  { {0,0},{-1,0},{-1,-1},{0,2},{-1,2} },//(L->0)
  { {0,0},{-1,0},{-1,-1},{0,2},{-1,2} },//(L->2)
  { {0,0},{1,0},{1,1},{0,-2},{1,-2} },//(2->L)
  { {0,0},{-1,0},{-1,1},{0,-2},{-1,-2} },//(2->R)
  { {0,0},{1,0},{1,-1},{0,2},{1,2} },//(R->2)
  { {0,0},{1,0},{1,-1},{0,2},{1,2} } //(R->0)
};

// draws a shape at the specified location and orientation in a colour (c)
// can be used with colour 0 or 1 (background) to remove a shape
// t specifies the target for drawing: board and/or screen
void drawShape(shape * s , int8_t c, int8_t t) {
  switch (s->t) {
    case 1: // I
      switch (s->r) {
        case 0: setPixel(s->x-1,s->y,c,t); setPixel(s->x,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x+2,s->y,c,t); break;
        case 1: setPixel(s->x,s->y+1,c,t); setPixel(s->x,s->y,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x,s->y-2,c,t); break ;
        case 2: setPixel(s->x-1,s->y-1,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x+1,s->y-1,c,t); setPixel(s->x+2,s->y-1,c,t); break;
        case 3: setPixel(s->x+1,s->y+1,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x+1,s->y-1,c,t); setPixel(s->x+1,s->y-2,c,t); break;
      }
    break;
    case 2: // O
      setPixel(s->x,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x+1,s->y-1,c,t);
    break;
    case 3: // T
      switch (s->r) {
        case 0: setPixel(s->x,s->y,c,t); setPixel(s->x-1,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x,s->y+1,c,t); break;
        case 1: setPixel(s->x,s->y,c,t); setPixel(s->x,s->y+1,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x-1,s->y,c,t); break;
        case 2: setPixel(s->x,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x-1,s->y,c,t); setPixel(s->x,s->y-1,c,t); break;
        case 3: setPixel(s->x,s->y,c,t); setPixel(s->x,s->y+1,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x+1,s->y,c,t); break;
      }
    break;
    case 4: // L
      switch (s->r) {
        case 0: setPixel(s->x,s->y,c,t); setPixel(s->x-1,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x+1,s->y+1,c,t); break;
        case 1: setPixel(s->x,s->y,c,t); setPixel(s->x,s->y+1,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x-1,s->y+1,c,t); break;
        case 2: setPixel(s->x,s->y,c,t); setPixel(s->x-1,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x-1,s->y-1,c,t); break;
        case 3: setPixel(s->x,s->y,c,t); setPixel(s->x,s->y+1,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x+1,s->y-1,c,t); break;
      }
    break;
    case 5: // J
      switch (s->r) {
        case 0: setPixel(s->x,s->y,c,t); setPixel(s->x-1,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x-1,s->y+1,c,t); break;
        case 1: setPixel(s->x,s->y,c,t); setPixel(s->x,s->y+1,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x-1,s->y-1,c,t); break;
        case 2: setPixel(s->x,s->y,c,t); setPixel(s->x-1,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x+1,s->y-1,c,t); break;
        case 3: setPixel(s->x,s->y,c,t); setPixel(s->x,s->y+1,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x+1,s->y+1,c,t); break;
      }
    break;
    case 6: // S
      switch (s->r) {
        case 0: setPixel(s->x,s->y,c,t); setPixel(s->x-1,s->y,c,t); setPixel(s->x,s->y+1,c,t); setPixel(s->x+1,s->y+1,c,t); break;
        case 1: setPixel(s->x,s->y,c,t); setPixel(s->x-1,s->y,c,t); setPixel(s->x-1,s->y+1,c,t); setPixel(s->x,s->y-1,c,t); break;
        case 2: setPixel(s->x,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x-1,s->y-1,c,t); break;
        case 3: setPixel(s->x,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x+1,s->y-1,c,t); setPixel(s->x,s->y+1,c,t); break;
      }
    break;
    case 7: // Z
      switch (s->r) {
        case 0: setPixel(s->x,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x,s->y+1,c,t); setPixel(s->x-1,s->y+1,c,t); break;
        case 1: setPixel(s->x,s->y,c,t); setPixel(s->x-1,s->y,c,t); setPixel(s->x,s->y+1,c,t); setPixel(s->x-1,s->y-1,c,t); break;
        case 2: setPixel(s->x,s->y,c,t); setPixel(s->x-1,s->y,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x+1,s->y-1,c,t); break;
        case 3: setPixel(s->x,s->y,c,t); setPixel(s->x+1,s->y,c,t); setPixel(s->x,s->y-1,c,t); setPixel(s->x+1,s->y+1,c,t); break;
      }
    break;
  }
}

// returns 1 if the piece will fit at the designated position, 0 otherwise.
int8_t checkFit(shape * s) {
  // first check walls, ceiling and floor
  switch (s->t) {
    case 1: // I
      switch (s->r) {
        case 0: if ((s->x<1)||(s->x>BOARDWIDTH-3)||(s->y<0)||(s->y>BOARDHEIGHT+SPAWNHEIGHT-1)) return(0); break;
        case 1: if ((s->x<0)||(s->x>BOARDWIDTH-1)||(s->y<2)||(s->y>BOARDHEIGHT+SPAWNHEIGHT-2)) return(0); break;
        case 2: if ((s->x<1)||(s->x>BOARDWIDTH-3)||(s->y<1)||(s->y>BOARDHEIGHT+SPAWNHEIGHT-0)) return(0); break;
        case 3: if ((s->x<-1)||(s->x>BOARDWIDTH-2)||(s->y<2)||(s->y>BOARDHEIGHT+SPAWNHEIGHT-2)) return(0); break;
      }
    break;
    case 2: // O - independent of orientation
        if ((s->x<0)||(s->x>BOARDWIDTH-2)||(s->y<1)||(s->y>BOARDHEIGHT+SPAWNHEIGHT-1)) return(0);
    break;
    default: // all other shapes have the same bounding box
      switch (s->r) {
        case 0: if ((s->x<1)||(s->x>BOARDWIDTH-2)||(s->y<0)||(s->y>BOARDHEIGHT+SPAWNHEIGHT-2)) return(0); break;
        case 1: if ((s->x<1)||(s->x>BOARDWIDTH-1)||(s->y<1)||(s->y>BOARDHEIGHT+SPAWNHEIGHT-2)) return(0); break;
        case 2: if ((s->x<1)||(s->x>BOARDWIDTH-2)||(s->y<1)||(s->y>BOARDHEIGHT+SPAWNHEIGHT-1)) return(0); break;
        case 3: if ((s->x<0)||(s->x>BOARDWIDTH-2)||(s->y<1)||(s->y>BOARDHEIGHT+SPAWNHEIGHT-2)) return(0); break;
      }
    break;
  }
  // now check if there is anything else occupying the required cells
  switch (s->t) {
    case 1: // I
      switch (s->r) {
        case 0: for (int8_t i=-1; i<3; i++) { if (board[s->y][s->x+i]>1) return(0); } break;
        case 1: for (int8_t i=-2; i<2; i++) { if (board[s->y+i][s->x]>1) return(0); } break;
        case 2: for (int8_t i=-1; i<3; i++) { if (board[s->y-1][s->x+i]>1) return(0); } break;
        case 3: for (int8_t i=-2; i<2; i++) { if (board[s->y+i][s->x+1]>1) return(0); } break;
      }
    break;
    case 2: // O //independent of orientation
      if ((board[s->y][s->x]>1)||(board[s->y][s->x+1]>1)||(board[s->y-1][s->x]>1)||(board[s->y-1][s->x+1]>1)) return(0);
    break;
    case 3: // T
      if (board[s->y][s->x]>1) return(0);
      if ((s->r!=0)&&(board[s->y-1][s->x]>1)) return(0);
      if ((s->r!=1)&&(board[s->y][s->x+1]>1)) return(0);
      if ((s->r!=2)&&(board[s->y+1][s->x]>1)) return(0);
      if ((s->r!=3)&&(board[s->y][s->x-1]>1)) return(0);
    break;
    case 4: //L
      switch (s->r) {
        case 0: if ( board[s->y][s->x]>1 || board[s->y][s->x-1]>1 || board[s->y][s->x+1]>1 || board[s->y+1][s->x+1]>1 ) return(0); break;
        case 1: if ( board[s->y][s->x]>1 || board[s->y-1][s->x]>1 || board[s->y+1][s->x]>1 || board[s->y+1][s->x-1]>1 ) return(0); break;
        case 2: if ( board[s->y][s->x]>1 || board[s->y][s->x-1]>1 || board[s->y][s->x+1]>1 || board[s->y-1][s->x-1]>1 ) return(0); break;
        case 3: if ( board[s->y][s->x]>1 || board[s->y-1][s->x]>1 || board[s->y+1][s->x]>1 || board[s->y-1][s->x+1]>1 ) return(0); break;
      }
    break;
    case 5: //J
      switch (s->r) {
        case 0: if ( board[s->y][s->x]>1 || board[s->y][s->x-1]>1 || board[s->y][s->x+1]>1 || board[s->y+1][s->x-1]>1 ) return(0); break;
        case 1: if ( board[s->y][s->x]>1 || board[s->y-1][s->x]>1 || board[s->y+1][s->x]>1 || board[s->y-1][s->x-1]>1 ) return(0); break;
        case 2: if ( board[s->y][s->x]>1 || board[s->y][s->x-1]>1 || board[s->y][s->x+1]>1 || board[s->y-1][s->x+1]>1 ) return(0); break;
        case 3: if ( board[s->y][s->x]>1 || board[s->y-1][s->x]>1 || board[s->y+1][s->x]>1 || board[s->y+1][s->x+1]>1 ) return(0); break;
      }
    break;
    case 6: //S
      switch (s->r) {
        case 0: if ( board[s->y][s->x]>1 || board[s->y][s->x-1]>1 || board[s->y+1][s->x]>1 || board[s->y+1][s->x+1]>1 ) return(0); break;
        case 1: if ( board[s->y][s->x]>1 || board[s->y][s->x-1]>1 || board[s->y-1][s->x]>1 || board[s->y+1][s->x-1]>1 ) return(0); break;
        case 2: if ( board[s->y][s->x]>1 || board[s->y][s->x+1]>1 || board[s->y-1][s->x]>1 || board[s->y-1][s->x-1]>1 ) return(0); break;
        case 3: if ( board[s->y][s->x]>1 || board[s->y][s->x+1]>1 || board[s->y+1][s->x]>1 || board[s->y-1][s->x+1]>1 ) return(0); break;
      }
    break;
    case 7: //Z
      switch (s->r) {
        case 0: if ( board[s->y][s->x]>1 || board[s->y][s->x+1]>1 || board[s->y+1][s->x]>1 || board[s->y+1][s->x-1]>1 ) return(0); break;
        case 1: if ( board[s->y][s->x]>1 || board[s->y][s->x-1]>1 || board[s->y+1][s->x]>1 || board[s->y-1][s->x-1]>1 ) return(0); break;
        case 2: if ( board[s->y][s->x]>1 || board[s->y][s->x-1]>1 || board[s->y-1][s->x]>1 || board[s->y-1][s->x+1]>1 ) return(0); break;
        case 3: if ( board[s->y][s->x]>1 || board[s->y][s->x+1]>1 || board[s->y-1][s->x]>1 || board[s->y+1][s->x+1]>1 ) return(0); break;
      }
    break;
  }
  return(1); // last option: it fits!
}

// try to rotate a shape, kick if necessary
// rr = relative rotation, eiter 0 = CCW, 1 = CW
// returns 0 in case of failure
int8_t rotateShape(shape * s, bool CW) {
  shape t;
  int rt = 2 * s->r; // rotation type
  if (!CW) rt++;
  /* rotation type now is one of the following:
   * 0: coming from 0, going to 3 (0->R)
   * 1: coming from 0, going to 1 (0->L)
   * 2: coming from 1, going to 0 (L->0)
   * 3: coming from 1, going to 2 (L->2)
   * 4: coming from 2, going to 1 (2->L)
   * 5: coming from 2, going to 3 (2->R)
   * 6: coming from 3, going to 2 (R->2)
   * 7: coming from 3, going to 0 (R->0)
   */
  // first remove the current shape from the board
  drawShape(s,0,TARGETBOARD);
  // go through all the kick levels until one that succeeds is found OR until all fail
  for (int kl=0; kl<5; kl++)
  {
    t=*s;
    if (CW) t.r --; else t.r ++;
    t.r &= 3; // make sure value is 0...3
    if (t.t==1) { // I rules
      t.x += Ikickdata[rt][kl].x;
      t.y += Ikickdata[rt][kl].y;
    } else { // O rules
      t.x += Okickdata[rt][kl].x;
      t.y += Okickdata[rt][kl].y;
    }
    if (checkFit(&t)) {
      // ok, it fits here, now remove the old shape from the screen as well
      drawShape(s,0,TARGETSCREEN);
      *s=t;
      // draw the new shape in the new position and orientation on screen and board
      drawShape(s,2 * s->t + 1,TARGETBOTH);
      return(1);
    }
  }
  // doesn't fit - draw back the shape in its old position and orientation
  drawShape(s,2 * s->t +1,TARGETBOARD);
  return(0);
}

// at the last drop we need to have some extra time for block gymnasticst, so we add an extra timer that is
// reset at every block drop, but is only actually used when the block is at the bottom
uint32_t lockDelayTime;
#define LOCKDELAY 2 * gameSpeed

// drop a shape one line.
// returns 0 in case of failure
int8_t dropShape(shape * s) {
  shape t;
  t=*s;
  t.y--; // drop it one line
  // first remove the current shape from the board
  drawShape(s,0,TARGETBOARD); 
  if (checkFit(&t)) { // check if shape fits in the new position
    // ok, it fits here, now remove the old shape from the screen as well
    drawShape(s,0,TARGETSCREEN);
      *s=t;
    // draw the shape in the new position in bright colour on screen and board
    drawShape(s,2 * s->t + 1,TARGETBOTH);
    lockDelayTime = millis();
    return(1); // succes
  }
  // draw the shape back in its old position, but in the normal (dark) colour
  drawShape(s,2 * s->t,TARGETBOTH);
  return(0); // failure
}

// try to move a shape left or right. -1=left, +1=right
// returns 0 in case of failure
int8_t moveShape(shape * s, int8_t lr) {
  shape t;
  t=*s;
  t.x+=lr;
   // first remove the current shape from the board
  drawShape(s,0,TARGETBOARD); 
  if (checkFit(&t)) { // check if shape fits in the new position
    // ok, it fits here, now remove the old shape from the screen as well
    drawShape(s,0,TARGETSCREEN);
      *s=t;
    // draw the shape in the new position in bright colour on screen and board
    drawShape(s,2 * s->t + 1,TARGETBOTH);
    return(1); // succes
  }
  // draw the shape back in its old position, but in the bright colour
  drawShape(s,2 * s->t + 1,TARGETBOTH);
  return(0); // failure
}


/*******************************************************************************************************************
* BUTTONSUPPORT section
*******************************************************************************************************************/
#define HARDWAREBUTTONNUMBER 4
// B1=top right=pin 6
// B2=bottom right=pin 7
// B3=top left=pin 4
// B4=bottom left=pin 3
uint8_t buttonPin[HARDWAREBUTTONNUMBER]={6,7,4,3};

// this turns 4 hardware buttons into 16 software buttons by only reacting to combinations of buttons
#define SOFTWAREBUTTONNUMBER 16

volatile uint8_t buttonDebounce[SOFTWAREBUTTONNUMBER];
volatile uint32_t buttonTimer[SOFTWAREBUTTONNUMBER];
volatile uint32_t buttonPresses[SOFTWAREBUTTONNUMBER];

void setupHardwareButtons() {
  for (uint8_t i=0; i<HARDWAREBUTTONNUMBER; i++) {
    pinMode(buttonPin[i],INPUT_PULLUP);
  }
}
void setupSoftwareButtons() {
  for (uint8_t i=0; i<SOFTWAREBUTTONNUMBER; i++) {
    buttonDebounce[i]=0;
    buttonTimer[i]=0;
    buttonPresses[i]=0;
  }
}

uint8_t readHardwareButtons() {
  uint8_t rv=0;
  for (uint8_t i=0; i<HARDWAREBUTTONNUMBER; i++) {
    rv <<= 1;
    if (digitalRead(buttonPin[i])==0) { rv=rv|1; }
  }
  return(rv);
}
 
void readSoftwareButtons() {
  uint8_t hb=readHardwareButtons();
  for (uint8_t i=0; i<SOFTWAREBUTTONNUMBER; i++) {
    buttonDebounce[i]=buttonDebounce[i]<<1;
    if (i==hb) {
      buttonDebounce[i]|=1;
      if (buttonDebounce[i]==0xff) { // a software button is clearly pressed
        if (buttonTimer[i]==0)  { // it is a new software button press
          buttonPresses[i]++; // count the press
        } else if (buttonTimer[i]>=250) { // autorepeat the software button after .25 seconds
          buttonPresses[i]++; // count the press
          buttonTimer[i]-=100; // autorepeat interval .1 seconds
        }
        buttonTimer[i]++; // run the timer
      }
    } else {
      buttonTimer[i]=0; // reset the timer/ do not run
    }
  }
}

void clearSoftwareButtons() {
  setupSoftwareButtons();
}


/*******************************************************************************************************************
* TIMERSUPPORT section
*******************************************************************************************************************/
// a 1ms timer is used to handle keypresses and making sure display data is transferred completely
// Data for one display of 200 LEDs * 24 decoded bits * 4 encoded bits per bit at 3.2MHz takes 6ms to transfer, and
// 12ms / 18ms / 24ms for two / three / four displays respectively So if we set a 25ms timer, we're good to go for
// 1..4 displays.
hw_timer_t * times = NULL; // a timer structure for data acquisition

void IRAM_ATTR handleTimer(void) {
  readSoftwareButtons();
}

int initializeTimer(void) {
  times = timerBegin(1000000);
  if (times == NULL) {
    return(false);
  } else {
    timerAttachInterrupt(times, &handleTimer);
    timerAlarm(times, 1000, true, 0);
    return(true);
  }
}


/*******************************************************************************************************************
* HISCORESERVER section
*******************************************************************************************************************/
const char* ssid = ACCESSPOINTSSID; 
const char* password = ACCESSPOINTPASSWORD;
#define WIFITIMEOUT 4000

// this tries to connect to wifi, and returns 0 if it doesn't work within 4 seconds
uint8_t connectWifi() {
  Serial.println("wifi connecion startup");
  WiFi.mode(WIFI_STA);    //Set Wi-Fi Mode as station
  WiFi.begin(ssid, password);
  uint32_t starttime=millis();
  uint32_t now=starttime;
  
  while ( (WiFi.status() != WL_CONNECTED) && ((now-starttime)< WIFITIMEOUT ) ) {
    now=millis();
  }
  Serial.println("wifi connection attempt finished");
  Serial.println(WiFi.status());
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("wifi connection failed");
    return(0);
  } else {
    Serial.println("wifi connected");
    return(1);
  }
}

void disconnectWifi() {

}

// keep this under  ~90 characters or increase the size of the string holding the url
#define HIGHSCOREBASEURL "http://hackwinkel.nl/tetrishighscore.php"

// contacts the server, sends the current score and retreives the current highscore
// of course this is vulnerable to manipulation...
int32_t exchangeScores(int32_t myscore) {
  Serial.println("score exchange started");
  int32_t highscore=0;
  if (connectWifi()) {
    Serial.println("Wifi connected");
    HTTPClient http;
    // string to build the request
    char url[100];
    sprintf(url,"%s?score=%d",HIGHSCOREBASEURL,myscore);
    Serial.println(url);
    http.begin(url);
    int32_t httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      String result = http.getString();
      highscore=result.toInt();
      http.end();
      disconnectWifi();
      Serial.println("score exchange succes");
      return(highscore);
    } else {
      http.end();
      disconnectWifi();
      Serial.println("score exchange failed");
      return(-1); //request failed - ignore and return negative high score
    }
  }
  Serial.println("score exchange failed");
  return(-1); // server could not be contacted
}
 

/*******************************************************************************************************************
* ACTIONSUPPORT section
*******************************************************************************************************************/
// translate software button presses into actions
#define ACTIONLEFT 3
#define ACTIONRIGHT 12
#define ACTIONDROP 5
#define ACTIONSTART 10
#define ACTIONCW 9
#define ACTIONCCW 6
#define ACTIONQUIT 15


uint8_t gameState=0;
/*
 * 0 = waiting for start button press, displaying highscore
 * 1 = running
 * 2 = wipe to score
 * 3 = display score
 */
shape cur;

uint32_t gameSpeed;
uint32_t gameSpeedPreviousTime;
uint32_t gameScore=0;
uint32_t gameHiScore=0;
uint32_t lastScreenUpdate;

void printBoard(){
  for (int y = BOARDHEIGHT + SPAWNHEIGHT-1; y >= 0; y--){
    for (int x = 0; x < BOARDWIDTH; x++){
      Serial.print(board[y][x]);
    }
    Serial.println("");
  }
}

void dumpDMABuffer(){
  for (int i = 0; i < LEDSTRINGSIZE; i++){
    if ((ledDMAbuffer[i].B == 0b10001000100010001000100010001000)
      && (ledDMAbuffer[i].R == 0b10001000100010001000100010001000)
      && (ledDMAbuffer[i].G == 0b10001000100010001000100010001000)) Serial.print("0");
    else Serial.print("1");
  }
}

// returns 0 on failure
uint8_t spawnNew() {
  cur.t=1+random(7);
  cur.x=4;
  cur.y=22;
  cur.r=0;
  if (checkFit(&cur)) {
    drawShape(&cur , 2* cur.t + 1, TARGETBOTH);
    return(1);
  }
  return(0);
}

void newGame() {
  clearSoftwareButtons();
  clearAllPixels(TARGETBOTH);
  delay(25);
  updateSpiLedScreen();
  lastScreenUpdate=millis();
  delay(25);
  gameSpeed = 1000; // drop 1 line per 1000 ms
  gameSpeedPreviousTime=millis();
  gameScore = 0;
  uint8_t dummy=spawnNew();
  gameState=1;
}

// score the current board. return 0 if no score
void scoreBoard() {
  Serial.println("enter score board");
  int8_t f;
  while ((f=findFirstFullRow())>=0) {
    int8_t l=findLastFullRowInBlock(f);

    uint32_t currentScore = (l-f)*(l-f);
    gameScore = gameScore + currentScore;
    Serial.println(f);
    Serial.println(l);
    setPixelBlock(f,l, 16, TARGETSCREEN); // blink white
    updateSpiLedScreen();
    lastScreenUpdate=millis();
    delay(250);
    renderBoardBlock(f, l); // blink normal
    updateSpiLedScreen();
    lastScreenUpdate=millis();
    delay(250);
    setPixelBlock(f,l, 0, TARGETSCREEN);// blink black
    updateSpiLedScreen();
    lastScreenUpdate=millis();
    delay(250);
    removePixelRowBlock(f, l, 0, TARGETBOTH); // actually delete the block
    updateSpiLedScreen();
    lastScreenUpdate=millis();
    delay(250);

    if (gameSpeed > 500) { gameSpeed = gameSpeed - 5 * currentScore; }
    else if (gameSpeed > 250)  { gameSpeed = gameSpeed - 3 * currentScore; }
    else if (gameSpeed > 125)  { gameSpeed = gameSpeed - 2 * currentScore; }
    else if (gameSpeed > 62)  { gameSpeed = gameSpeed -  currentScore; }
    else { gameSpeed = gameSpeed -  ((currentScore+1) >> 1); }
    if (gameSpeed < 32) { gameSpeed=32; }
  }
  Serial.println("exit score board");
}

void doGameState0() {
  if(random(500)==0) { // slow down twinkling
    uint8_t x=random(BOARDWIDTH);
    uint8_t y=random(BOARDHEIGHT);
    setPixel(x, y, board[y][x] ^ 1, TARGETBOTH);
    if (buttonPresses[ACTIONSTART]) {
      delay(10);
      newGame();
    }
  }
}

void doGameState1() {
  uint32_t now=millis();
  if ((now-gameSpeedPreviousTime)>=gameSpeed) {
    if (dropShape(&cur)==0) { // cant drop any further - score the board
      if ((now - lockDelayTime)>=LOCKDELAY) {
        scoreBoard();
        if (spawnNew()==0) {// failed to spawn new block - end of game
          gameState=2;
        }
      }
    }
    gameSpeedPreviousTime+=gameSpeed;
  }
  // now check if we're still in the same gamestate, because it just might be possible
  // that the line was completed  and lockdelay passed
  if (gameState==1) {
    // no block falling this time - handle keypresses
    if (buttonPresses[ACTIONLEFT]) {
      buttonPresses[ACTIONLEFT]--;
      // ignore if it can be done or not
      moveShape(&cur, -1);

    }
    if (buttonPresses[ACTIONRIGHT]) {
      buttonPresses[ACTIONRIGHT]--;
      // ignore if it can be done or not
      moveShape(&cur, 1);
    }
    if (buttonPresses[ACTIONCW]) {
      buttonPresses[ACTIONCW]--;
      rotateShape(&cur, true);
    }
    if (buttonPresses[ACTIONCCW]) {
      buttonPresses[ACTIONCCW]--;
      rotateShape(&cur, false);
    }
    if (buttonPresses[ACTIONDROP]) {
      buttonPresses[ACTIONDROP]--;
      if (dropShape(&cur)==0) { // cant drop any further - score the board
        scoreBoard();
        if (spawnNew()==0) {// failed to spawn new block - end of game
          gameState=2;
        }
      }
    }
    if (buttonPresses[ACTIONQUIT]) {
      buttonPresses[ACTIONQUIT]=0;
      gameState=1;
    }
  }
}

void doGameState2() {
  for (uint8_t i=0; i<BOARDHEIGHT; i++) {
    setPixelRow (i, 16, TARGETSCREEN);
    updateSpiLedScreen();
    lastScreenUpdate=millis();
    delay(100);
  }
  for (uint8_t i=0; i<BOARDHEIGHT; i++) {
    renderBoardRow(i);
    updateSpiLedScreen();
    delay(100);
  }
  for (uint8_t i=0; i<BOARDHEIGHT; i++) {
    setPixelRow (i, 0, TARGETSCREEN);
    updateSpiLedScreen();
    lastScreenUpdate=millis();
    delay(100);
  }

  clearAllPixels(TARGETBOTH);
 
  int32_t tempHiScore=exchangeScores(gameScore);
  if (tempHiScore>0) { gameHiScore=tempHiScore; }
  else  if (gameScore>gameHiScore) { gameHiScore=gameScore; }

  for (uint32_t i=0; i<20; i++) {
    if (gameScore & (1 << i)) {
      setPixel(1, i, 12, TARGETBOTH);
      setPixel(2, i, 12, TARGETBOTH);
      setPixel(3, i, 12, TARGETBOTH);
    }
    if (gameHiScore & (1 << i)) {
      setPixel(6, i, 14, TARGETBOTH);
      setPixel(7, i, 14, TARGETBOTH);
      setPixel(8, i, 14, TARGETBOTH);
    }
    updateSpiLedScreen();
    lastScreenUpdate=millis();
    delay(100);
  }
  clearSoftwareButtons();
  gameState=0;
}


uint32_t idleCount=0;



void setup() {
  Serial.begin(9600);
  delay(100);
  clearAllPixels(TARGETBOTH);
  initializeSpiLedScreen();
  Serial.println("updatescreen");
  Serial.println(LEDSTRINGSIZE);
  updateSpiLedScreen();
  lastScreenUpdate=millis();
  delay(100);
  setupSoftwareButtons();
  setupHardwareButtons();
  initializeTimer();
}



void loop() {
  switch (gameState) {
    case 0: doGameState0(); break;
    case 1: doGameState1(); break;
    case 2: doGameState2(); break;
  }
  if ((millis()-lastScreenUpdate)>=25) {
    updateSpiLedScreen();
    lastScreenUpdate=millis();
  }
}















