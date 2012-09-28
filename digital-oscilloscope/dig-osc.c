// Black and white NTSC Digital Oscilloscope
// D.0 is sync
// D.1 is video
// Mega644 version by Shane Pryor 
// mod by brl4@cornell.edu:
//    for mega1284
//    for resolution 160 horizontal x 200 vertical
//    plays a musical scale from port B.3
//    UART SPI-mode video output from Morgan D. Jones

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdlib.h> 
#include <math.h> 
#include <util/delay.h>  
#include <avr/sleep.h>


// optional, if preferred///
#define begin {
#define end   }
////////////////////////////			

//cycles = 63.625 * 16 Note NTSC is 63.55 
//but this line duration makes each frame exactly 1/60 sec
//which is nice for keeping a realtime clock 
// video timing
#define LINE_TIME 1018 // 20 MHz 1271
#define SLEEP_TIME 999 // 20 MHz 1250
#define bytes_per_line 20
#define screen_width (bytes_per_line*8)
#define screen_height 200
#define screen_array_size screen_width*screen_height/8 

#define ScreenTop 30
#define ScreenBot (ScreenTop+screen_height)

//sync
char syncON, syncOFF;

volatile uint8_t  adc_index;
uint8_t adc_buffer[160];
uint8_t adc_erase[160];
uint8_t adc_complete;

//current line number in the current frame
volatile int LineCount;

//160h x 160v - screen buffer and pointer
char screen[screen_array_size];
int* screenindex;

//One bit masks
char pos[8] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};

//================================ 
//3x5 font numbers, then letters
//packed two per definition for fast 
//copy to the screen at x-position divisible by 4
prog_char smallbitmap[39][5] = { 
	//0
    0b11101110,
	0b10101010,
	0b10101010,
	0b10101010,
	0b11101110,
	//1
	0b01000100,
	0b11001100,
	0b01000100,
	0b01000100,
	0b11101110,
	//2
	0b11101110,
	0b00100010,
	0b11101110,
	0b10001000,
	0b11101110,
	//3
	0b11101110,
	0b00100010,
	0b11101110,
	0b00100010,
	0b11101110,
	//4
	0b10101010,
	0b10101010,
	0b11101110,
	0b00100010,
	0b00100010,
	//5
	0b11101110,
	0b10001000,
	0b11101110,
	0b00100010,
	0b11101110,
	//6
	0b11001100,
	0b10001000,
	0b11101110,
	0b10101010,
	0b11101110,
	//7
	0b11101110,
	0b00100010,
	0b01000100,
	0b10001000,
	0b10001000,
	//8
	0b11101110,
	0b10101010,
	0b11101110,
	0b10101010,
	0b11101110,
	//9
	0b11101110,
	0b10101010,
	0b11101110,
	0b00100010,
	0b01100110,
	//:
	0b00000000,
	0b01000100,
	0b00000000,
	0b01000100,
	0b00000000,
	//=
	0b00000000,
	0b11101110,
	0b00000000,
	0b11101110,
	0b00000000,
	//blank
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//A
	0b11101110,
	0b10101010,
	0b11101110,
	0b10101010,
	0b10101010,
	//B
	0b11001100,
	0b10101010,
	0b11101110,
	0b10101010,
	0b11001100,
	//C
	0b11101110,
	0b10001000,
	0b10001000,
	0b10001000,
	0b11101110,
	//D
	0b11001100,
	0b10101010,
	0b10101010,
	0b10101010,
	0b11001100,
	//E
	0b11101110,
	0b10001000,
	0b11101110,
	0b10001000,
	0b11101110,
	//F
	0b11101110,
	0b10001000,
	0b11101110,
	0b10001000,
	0b10001000,
	//G
	0b11101110,
	0b10001000,
	0b10001000,
	0b10101010,
	0b11101110,
	//H
	0b10101010,
	0b10101010,
	0b11101110,
	0b10101010,
	0b10101010,
	//I
	0b11101110,
	0b01000100,
	0b01000100,
	0b01000100,
	0b11101110,
	//J
	0b00100010,
	0b00100010,
	0b00100010,
	0b10101010,
	0b11101110,
	//K
	0b10001000,
	0b10101010,
	0b11001100,
	0b11001100,
	0b10101010,
	//L
	0b10001000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b11101110,
	//M
	0b10101010,
	0b11101110,
	0b11101110,
	0b10101010,
	0b10101010,
	//N
	0b00000000,
	0b11001100,
	0b10101010,
	0b10101010,
	0b10101010,
	//O
	0b01000100,
	0b10101010,
	0b10101010,
	0b10101010,
	0b01000100,
	//P
	0b11101110,
	0b10101010,
	0b11101110,
	0b10001000,
	0b10001000,
	//Q
	0b01000100,
	0b10101010,
	0b10101010,
	0b11101110,
	0b01100110,
	//R
	0b11101110,
	0b10101010,
	0b11001100,
	0b11101110,
	0b10101010,
	//S
	0b11101110,
	0b10001000,
	0b11101110,
	0b00100010,
	0b11101110,
	//T
	0b11101110,
	0b01000100,
	0b01000100,
	0b01000100,
	0b01000100, 
	//U
	0b10101010,
	0b10101010,
	0b10101010,
	0b10101010,
	0b11101110, 
	//V
	0b10101010,
	0b10101010,
	0b10101010,
	0b10101010,
	0b01000100,
	//W
	0b10101010,
	0b10101010,
	0b11101110,
	0b11101110,
	0b10101010,
	//X
	0b00000000,
	0b10101010,
	0b01000100,
	0b01000100,
	0b10101010,
	//Y
	0b10101010,
	0b10101010,
	0b01000100,
	0b01000100,
	0b01000100,
	//Z
	0b11101110,
	0b00100010,
	0b01000100,
	0b10001000,
	0b11101110
};

//===============================================
// Full ascii 5x7 char set
// Designed by: David Perez de la Cruz,and Ed Lau	  
// see: http://instruct1.cit.cornell.edu/courses/ee476/FinalProjects/s2005/dp93/index.html

prog_char ascii[128][7] = {
	//0
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//1
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//2
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//3
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//4
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//5
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//6
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//7
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//8
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//9
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//10
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//11
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//12
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//13
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//14
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//15
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//16
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//17
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//18
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//19
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//20
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//21
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//22
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//23
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//24
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//25
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//26
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//27
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//28
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//29
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//30
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//31
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//32 Space
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//33 Exclamation !
	0b01100000,
	0b01100000,
	0b01100000,
	0b01100000,
	0b00000000,
	0b00000000,
	0b01100000,
	//34 Quotes "
	0b01010000,
	0b01010000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//35 Number #
	0b00000000,
	0b01010000,
	0b11111000,
	0b01010000,
	0b11111000,
	0b01010000,
	0b00000000,
	//36 Dollars $
	0b01110000,
	0b10100000,
	0b10100000,
	0b01110000,
	0b00101000,
	0b00101000,
	0b01110000,
	//37 Percent %
	0b01000000,
	0b10101000,
	0b01010000,
	0b00100000,
	0b01010000,
	0b10101000,
	0b00010000,
	//38 Ampersand &
	0b00100000,
	0b01010000,
	0b10100000,
	0b01000000,
	0b10101000,
	0b10010000,
	0b01101000,
	//39 Single Quote '
	0b01000000,
	0b01000000,
	0b01000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//40 Left Parenthesis (
	0b00010000,
	0b00100000,
	0b01000000,	
	0b01000000,
	0b01000000,
	0b00100000,
	0b00010000,
	//41 Right Parenthesis )
	0b01000000,
	0b00100000,
	0b00010000,
	0b00010000,
	0b00010000,
	0b00100000,
	0b01000000,
	//42 Star *
	0b00010000,
	0b00111000,
	0b00010000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//43 Plus +
	0b00000000,
	0b00100000,
	0b00100000,
	0b11111000,
	0b00100000,
	0b00100000,
	0b00000000,
	//44 Comma ,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00010000,
	0b00010000,
	//45 Minus -
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b11111000,
	0b00000000,
	0b00000000,
	//46 Period .
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00010000,
	// 47 Backslash /
	0b00000000,
	0b00001000,
	0b00010000,
	0b00100000,
	0b01000000,
	0b10000000,
	0b00000000,
	// 48 Zero
	0b01110000,
	0b10001000,
	0b10011000,
	0b10101000,
	0b11001000,
	0b10001000,
	0b01110000,
	//49 One
	0b00100000,
	0b01100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b01110000,  
	//50 two
	0b01110000,
	0b10001000,
	0b00001000,
	0b00010000,
	0b00100000,
	0b01000000,
	0b11111000,
	 //51 Three
	0b11111000,
	0b00010000,
	0b00100000,
	0b00010000,
	0b00001000,
	0b10001000,
	0b01110000,
	//52 Four
	0b00010000,
	0b00110000,
	0b01010000,
	0b10010000,
	0b11111000,
	0b00010000,
	0b00010000,
	//53 Five
	0b11111000,
	0b10000000,
	0b11110000,
	0b00001000,
	0b00001000,
	0b10001000,
	0b01110000,
	//54 Six
	0b01000000,
	0b10000000,
	0b10000000,
	0b11110000,
	0b10001000,
	0b10001000,
	0b01110000,
	//55 Seven
	0b11111000,
	0b00001000,
	0b00010000,
	0b00100000,
	0b01000000,
	0b10000000,
	0b10000000,
	//56 Eight
	0b01110000,
	0b10001000,
	0b10001000,
	0b01110000,
	0b10001000,
	0b10001000,
	0b01110000,
	//57 Nine
	0b01110000,
	0b10001000,
	0b10001000,
	0b01111000,
	0b00001000,
	0b00001000,
	0b00010000,
	//58 :
	0b00000000,
	0b00000000,
	0b00100000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00100000,
	//59 ;
	0b00000000,
	0b00000000,
	0b00100000,
	0b00000000,
	0b00000000,
	0b00100000,
	0b00100000,
	//60 <
	0b00000000,
	0b00011000,
	0b01100000,
	0b10000000,
	0b01100000,
	0b00011000,
	0b00000000,
	//61 =
	0b00000000,
	0b00000000,
	0b01111000,
	0b00000000,
	0b01111000,
	0b00000000,
	0b00000000,
	//62 >
	0b00000000,
	0b11000000,
	0b00110000,
	0b00001000,
	0b00110000,
	0b11000000,
	0b00000000,
	//63 ?
	0b00110000,
	0b01001000,
	0b00010000,
	0b00100000,
	0b00100000,
	0b00000000,
	0b00100000,
	//64 @
	0b01110000,
	0b10001000,
	0b10111000,
	0b10101000,
	0b10010000,
	0b10001000,
	0b01110000,
	//65 A
	0b01110000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b11111000,
	0b10001000,
	0b10001000,
	//B
	0b11110000,
	0b10001000,
	0b10001000,
	0b11110000,
	0b10001000,
	0b10001000,
	0b11110000,
	//C
	0b01110000,
	0b10001000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b10001000,
	0b01110000,
	//D
	0b11110000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b11110000,
	//E
	0b11111000,
	0b10000000,
	0b10000000,
	0b11111000,
	0b10000000,
	0b10000000,
	0b11111000,
	//F
	0b11111000,
	0b10000000,
	0b10000000,
	0b11111000,
	0b10000000,
	0b10000000,
	0b10000000,
	//G
	0b01110000,
	0b10001000,
	0b10000000,
	0b10011000,
	0b10001000,
	0b10001000,
	0b01110000,
	//H
	0b10001000,
	0b10001000,
	0b10001000,
	0b11111000,
	0b10001000,
	0b10001000,
	0b10001000,
	//I
	0b01110000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b01110000,
	//J
	0b00111000,
	0b00010000,
	0b00010000,
	0b00010000,
	0b00010000,
	0b10010000,
	0b01100000,
	//K
	0b10001000,
	0b10010000,
	0b10100000,
	0b11000000,
	0b10100000,
	0b10010000,
	0b10001000,
	//L
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b11111000,
	//M
	0b10001000,
	0b11011000,
	0b10101000,
	0b10101000,
	0b10001000,
	0b10001000,
	0b10001000,
	//N
	0b10001000,
	0b10001000,
	0b11001000,
	0b10101000,
	0b10011000,
	0b10001000,
	0b10001000,
	//O
	0b01110000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b01110000,
	//P
	0b11110000,
	0b10001000,
	0b10001000,
	0b11110000,
	0b10000000,
	0b10000000,
	0b10000000,
	//Q
	0b01110000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b10101000,
	0b10010000,
	0b01101000,
	//R
	0b11110000,
	0b10001000,
	0b10001000,
	0b11110000,
	0b10100000,
	0b10010000,
	0b10001000,
	//S
	0b01111000,
	0b10000000,
	0b10000000,
	0b01110000,
	0b00001000,
	0b00001000,
	0b11110000,
	//T
	0b11111000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	//U
	0b10001000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b01110000,
	//V
	0b10001000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b10001000,
	0b01010000,
	0b00100000,
	//W
	0b10001000,
	0b10001000,
	0b10001000,
	0b10101000,
	0b10101000,
	0b10101000,
	0b01010000,
	//X
	0b10001000,
	0b10001000,
	0b01010000,
	0b00100000,
	0b01010000,
	0b10001000,
	0b10001000,
	//Y
	0b10001000,
	0b10001000,
	0b10001000,
	0b01010000,
	0b00100000,
	0b00100000,
	0b00100000,
	//Z
	0b11111000,
	0b00001000,
	0b00010000,
	0b00100000,
	0b01000000,
	0b10000000,
	0b11111000,
	//91 [
	0b11100000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b10000000,
	0b11100000,
	//92 (backslash)
	0b00000000,
	0b10000000,
	0b01000000,
	0b00100000,
	0b00010000,
	0b00001000,
	0b00000000,
	//93 ]
	0b00111000,
	0b00001000,
	0b00001000,
	0b00001000,
	0b00001000,
	0b00001000,
	0b00111000,
	//94 ^
	0b00100000,
	0b01010000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//95 _
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b11111000,
	//96 `
	0b10000000,
	0b01000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	//97 a
	0b00000000,
	0b01100000,
	0b00010000,
	0b01110000,
	0b10010000,
	0b01100000,
	0b00000000,
	//98 b
	0b10000000,
	0b10000000,
	0b11100000,
	0b10010000,
	0b10010000,
	0b11100000,
	0b00000000,
	//99 c
	0b00000000,
	0b00000000,
	0b01110000,
	0b10000000,
	0b10000000,
	0b01110000,
	0b00000000,
	// 100 d
	0b00010000,
	0b00010000,
	0b01110000,
	0b10010000,
	0b10010000,
	0b01110000,
	0b00000000,
	//101 e
	0b00000000,
	0b01100000,
	0b10010000,
	0b11110000,
	0b10000000,
	0b01110000,
	0b00000000,
	//102 f
	0b00110000,
	0b01000000,
	0b11100000,
	0b01000000,
	0b01000000,
	0b01000000,
	0b00000000,
	//103 g
	0b00000000,
	0b01100000,
	0b10010000,
	0b01110000,
	0b00010000,
	0b00010000,
	0b01100000,
	//104 h
	0b10000000,
	0b10000000,
	0b11100000,
	0b10010000,
	0b10010000,
	0b10010000,
	0b00000000,
	//105 i
	0b00000000,
	0b00100000,
	0b00000000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00000000,
	//106 j
	0b00000000,
	0b00010000,
	0b00000000,
	0b00010000,
	0b00010000,
	0b00010000,
	0b01100000,
	//107 k
	0b10000000,
	0b10010000,
	0b10100000,
	0b11000000,
	0b10100000,
	0b10010000,
	0b00000000,
	//108 l
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00000000,
	//109 m
	0b00000000,
	0b00000000,
	0b01010000,
	0b10101000,
	0b10101000,
	0b10101000,
	0b00000000,
	//110 n
	0b00000000,
	0b00000000,
	0b01100000,
	0b10010000,
	0b10010000,
	0b10010000,
	0b00000000,
	//111 o
	0b00000000,
	0b00000000,
	0b01100000,
	0b10010000,
	0b10010000,
	0b01100000,
	0b00000000,
	//112 p
	0b00000000,
	0b00000000,
	0b01100000,
	0b10010000,
	0b11110000,
	0b10000000,
	0b10000000,
	//113 q
	0b00000000,
	0b00000000,
	0b01100000,
	0b10010000,
	0b11110000,
	0b00010000,
	0b00010000,
	//114 r
	0b00000000,
	0b00000000,
	0b10111000,
	0b01000000,
	0b01000000,
	0b01000000,
	0b00000000,
	//115 s
	0b00000000,
	0b00000000,
	0b01110000,
	0b01000000,
	0b00010000,
	0b01110000,
	0b00000000,
	//116 t
	0b01000000,
	0b01000000,
	0b11100000,
	0b01000000,
	0b01000000,
	0b01000000,
	0b00000000,
	// 117u
	0b00000000,
	0b00000000,
	0b10010000,
	0b10010000,
	0b10010000,
	0b01100000,
	0b00000000,
	//118 v
	0b00000000,
	0b00000000,
	0b10001000,
	0b10001000,
	0b01010000,
	0b00100000,
	0b00000000,
	//119 w
	0b00000000,
	0b00000000,
	0b10101000,
	0b10101000,
	0b01010000,
	0b01010000,
	0b00000000,
	//120 x
	0b00000000,
	0b00000000,
	0b10010000,
	0b01100000,
	0b01100000,
	0b10010000,
	0b00000000,
	//121 y
	0b00000000,
	0b00000000,
	0b10010000,
	0b10010000,
	0b01100000,
	0b01000000,
	0b10000000,
	//122 z
	0b00000000,
	0b00000000,
	0b11110000,
	0b00100000,
	0b01000000,
	0b11110000,
	0b00000000,
	//123 {
	0b00100000,
	0b01000000,
	0b01000000,
	0b10000000,
	0b01000000,
	0b01000000,
	0b00100000,
	//124 |
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	0b00100000,
	//125 }
	0b00100000,
	0b00010000,
	0b00010000,
	0b00001000,
	0b00010000,	
	0b00010000,
	0b00100000,
	//126 ~
	0b00000000,
	0b00000000,
	0b01000000,
	0b10101000,
	0b00010000,
	0b00000000,
	0b00000000,
	//127 DEL
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000
};

// put the MCU to sleep JUST before the CompA ISR goes off
ISR(TIMER1_COMPB_vect, ISR_NAKED)
{
	sei();
	sleep_cpu();
	reti();
}


//==================================
//This is the sync generator and raster generator. It MUST be entered from 
//sleep mode to get accurate timing of the sync pulses

ISR (TIMER1_COMPA_vect) {
	int x, screenStart ;
	//start the Horizontal sync pulse    
	PORTD = syncON;

	//update the current scanline number
	LineCount++;   
  
	//begin inverted (Vertical) synch after line 247
	if (LineCount==248) { 
    	//syncON = 0b00000001;
    	syncON = 0b01000000;
    	syncOFF = 0;
  	}
  
	//back to regular sync after line 250
	if (LineCount==251)	{
		syncON = 0;
		//syncOFF = 0b00000001;
		syncOFF = 0b01000000;
	}  
  
  	//start new frame after line 262
	if (LineCount==263)
		LineCount = 1;
      
	//adjust to make 5 us pulses
	_delay_us(3);

	//end sync pulse
	PORTD = syncOFF;   

	if (LineCount < ScreenBot && LineCount >= ScreenTop) {
		//compute offset into screen array
		//screenindex = screen + ((LineCount - ScreenTop) << 4) + ((LineCount - ScreenTop) << 3);

		//while( ADCSRA & (1<<ADSC));
		adc_buffer[adc_index++] = ADCH;
		ADCSRA |= (1<<ADSC);
		if (adc_index == 160){
			adc_complete = 1;
			adc_index = 0;
		}

		///}
		//compute offset into screen array
		//screenStart = ((LineCount - ScreenTop) << 4) + ((LineCount - ScreenTop) << 3) ;
		screenStart = (LineCount - ScreenTop) * bytes_per_line;
		//center image on screen
		_delay_us(7);
		//blast the data to the screen
		// We can load UDR twice because it is double-bufffered
		UDR0 = screen[screenStart] ;
		UCSR0B = _BV(TXEN0);
		UDR0 = screen[screenStart+1] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+2] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+3] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+4] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+5] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+6] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+7] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+8] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+9] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+10] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+11] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+12] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+13] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+14] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+15] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+16] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+17] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+18] ;
		while (!(UCSR0A & _BV(UDRE0))) ;
		UDR0 = screen[screenStart+19] ;

		//while( ADCSRA & (1<<ADSC));
		adc_buffer[adc_index++] = ADCH;
		ADCSRA |= (1<<ADSC);
		if (adc_index == 160){
			adc_complete = 1;
			adc_index = 0;
		}

		UCSR0B = 0 ;

	}         
}

//==================================
//plot one point 
//at x,y with color 1=white 0=black 2=invert 
void video_pt(char x, char y, char c) {
	//each line has 18 bytes
	//calculate i based upon this and x,y
	// the byte with the pixel in it
	//int i = (x >> 3) + ((int)y<<4) + ((int)y<<1);
	int i = (x >> 3) + (int)y * bytes_per_line ;

	if (c==1)
	  screen[i] = screen[i] | pos[x & 7];
    else if (c==0)
	  screen[i] = screen[i] & ~pos[x & 7];
    else
	  screen[i] = screen[i] ^ pos[x & 7];
}

//==================================
//plot a line 
//at x1,y1 to x2,y2 with color 1=white 0=black 2=invert 
//NOTE: this function requires signed chars   
//Code is from David Rodgers,
//"Procedural Elements of Computer Graphics",1985
void video_line(char x1, char y1, char x2, char y2, char c) {
	int e;
	signed int dx,dy,j, temp;
	signed char s1,s2, xchange;
    signed int x,y;
        
	x = x1;
	y = y1;
	
	//take absolute value
	if (x2 < x1) {
		dx = x1 - x2;
		s1 = -1;
	}

	else if (x2 == x1) {
		dx = 0;
		s1 = 0;
	}

	else {
		dx = x2 - x1;
		s1 = 1;
	}

	if (y2 < y1) {
		dy = y1 - y2;
		s2 = -1;
	}

	else if (y2 == y1) {
		dy = 0;
		s2 = 0;
	}

	else {
		dy = y2 - y1;
		s2 = 1;
	}

	xchange = 0;   

	if (dy>dx) {
		temp = dx;
		dx = dy;
		dy = temp;
		xchange = 1;
	} 

	e = ((int)dy<<1) - dx;  
	 
	for (j=0; j<=dx; j++) {
		video_pt(x,y,c);
		 
		if (e>=0) {
			if (xchange==1) x = x + s1;
			else y = y + s2;
			e = e - ((int)dx<<1);
		}

		if (xchange==1) y = y + s2;
		else x = x + s1;

		e = e + ((int)dy<<1);
	}
}

//==================================
// put a big character on the screen
// c is index into bitmap
void video_putchar(char x, char y, char c) { 
    char i;
	char y_pos;
	uint8_t j;

	for (i=0;i<7;i++) {
        y_pos = y + i;

		j = pgm_read_byte(((uint32_t)(ascii)) + c*7 + i);

        video_pt(x,   y_pos, (j & 0x80)==0x80);  
        video_pt(x+1, y_pos, (j & 0x40)==0x40); 
        video_pt(x+2, y_pos, (j & 0x20)==0x20);
        video_pt(x+3, y_pos, (j & 0x10)==0x10);
        video_pt(x+4, y_pos, (j & 0x08)==0x08);
    }
}

//==================================
// put a string of big characters on the screen
void video_puts(char x, char y, char *str) {
	char i;
	for (i=0; str[i]!=0; i++) { 
		video_putchar(x,y,str[i]);
		x = x+6;	
	}
}
      
//==================================
// put a small character on the screen
// x-coord must be on divisible by 4 
// c is index into bitmap
void video_smallchar(char x, char y, char c) { 
	char mask;
	//int i=((int)x>>3) + ((int)y<<4) + ((int)y<<1);
	int i=((int)x>>3) + (int)y * bytes_per_line ;

	if (x == (x & 0xf8)) mask = 0x0f;     //f8
	else mask = 0xf0;
	
	uint8_t j = pgm_read_byte(((uint32_t)(smallbitmap)) + c*5);
	screen[i]    =    (screen[i] & mask) | (j & ~mask);

	j = pgm_read_byte(((uint32_t)(smallbitmap)) + c*5 + 1);
   	screen[i+bytes_per_line] = (screen[i+bytes_per_line] & mask) | (j & ~mask);

	j = pgm_read_byte(((uint32_t)(smallbitmap)) + c*5 + 2);
    screen[i+bytes_per_line*2] = (screen[i+bytes_per_line*2] & mask) | (j & ~mask);
    
	j = pgm_read_byte(((uint32_t)(smallbitmap)) + c*5 + 3);
	screen[i+bytes_per_line*3] = (screen[i+bytes_per_line*3] & mask) | (j & ~mask);
   	
	j = pgm_read_byte(((uint32_t)(smallbitmap)) + c*5 + 4);
	screen[i+bytes_per_line*4] = (screen[i+bytes_per_line*4] & mask) | (j & ~mask); 
}

//==================================
// put a string of small characters on the screen
// x-cood must be on divisible by 4 
void video_putsmalls(char x, char y, char *str) {
	char i;
	x = x & 0b11111100; //make it divisible by 4
	for (i = 0; str[i] != 0; i++) {
		if (str[i] >= 0x30 && str[i] <= 0x3a) 
			video_smallchar(x, y, str[i] - 0x30);

        else video_smallchar(x, y, str[i]-0x40+12);
		x += 4;	
	}
}

//==================================
//return the value of one point 
//at x,y with color 1=white 0=black 2=invert
char video_set(char x, char y) {
	//The following construction 
  	//detects exactly one bit at the x,y location
	// int i = (x>>3) + ((int)y<<4) + ((int)y<<3);
	int i = (x>>3) + (int)y * bytes_per_line ;

    return (screen[i] & 1<<(7-(x & 0x7)));   	
}

//=== fixed point mult ===============================
int multfix(int a, int b) {
  int result1 = a * b;
  int result2 = (a>>8) * (b>>8);

  return (result2 << 8) | (result1 >> 8);
} 


//=== fixed conversion macros ========================================= 
#define int2fix(a)   (((int)(a))<<8)            //Convert char to fix. a is a char
#define fix2int(a)   ((signed char)((a)>>8))    //Convert fix to int. a is an int
#define float2fix(a) ((int)((a)*256.0))         //Convert float to fix. a is a float
#define fix2float(a) (((float)(a))/256.0)       //Convert fix to float. a is an int   

//=== animation stuff ==================================================
char cu1[]="Cornell  ECE 4760";
char cu2[]="F: ";

///////////////

//==================================         
// set up the ports and timers
int main() {
  uint8_t j;
  uint8_t temp;
  //init timer 1 to generate sync
  // TIMER 1: OC1* disconnected, CTC mode, fosc/1 (20MHz), OC1A and OC1B
  //		interrupts enabled
  TCCR1B = _BV(WGM12) | _BV(CS10);
  OCR1A  = LINE_TIME;	// time for one NTSC line
  OCR1B  = SLEEP_TIME;	// time to go to sleep
  TIMSK1 = _BV(OCIE1B) | _BV(OCIE1A);

  //init ports
  DDRD = 0x03;		//video out

  // USART in MSPIM mode, transmitter enabled, frequency fosc/4
  UCSR0B = _BV(TXEN0);
  UCSR0C = _BV(UMSEL01) | _BV(UMSEL00);
  UBRR0  = 1 ;

  // Setup ADC
  ADMUX  = (1<<ADLAR) | (1<<REFS0); // read high byte and set Vref to Vcc
  ADCSRA = (1 << ADEN) | (1<<ADSC) + 4; //enable ADC and set prescalar to 16

  adc_index = 0;
  adc_complete = 0;
  
  //initialize synch constants 
  LineCount = 1;

  syncON  = 0b00000000;
  syncOFF = 0b00000001;
  
  //Print "CORNELL" message
  video_puts(30,2,cu1);

  //Print "Copyright" message
  video_putsmalls(screen_width-70,screen_height-10,cu2);

  //side lines
  #define width screen_width-1
  #define height screen_height-1

  //video_line(0,0,0,height,1);
  video_line(width,0,width,height,1);
  video_line(0,10,width,10,1);
  video_line(0,0,width,0,1);
  video_line(0,height,width,height,1);
 

  ///////////////////////
  
  // Set up single video line timing
  sei();
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();


  // The following loop executes the if-block
  // when the screen refresh is done
  // then does all of the frame-end processing
  while(1) {

	if (LineCount == ScreenBot) { 

		if (adc_complete)
		{
			/*for (j = 0; j<160; j++){
				temp = adc_erase[j] >> 2;
				video_pt(j, -screen_height + temp, 2);
			}*/
			
			for (j = 0; j<160; j++){
				temp = adc_buffer[j] >> 2;
				video_pt(j, -screen_height + temp, 2);
			}
			
			memcpy(adc_erase, adc_buffer, 160);
			adc_complete = 0;
		}	
		
		/////////////////////////////////////////
		// add this line to execute code exactly once per frame
		// comment it out for max execution speed
		while (LineCount != ScreenTop) ;
		//_delay_ms(1);
     }  //line 231
  }  //for
}  //main
