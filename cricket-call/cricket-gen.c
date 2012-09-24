// Debounce demo 
// Mega644 version
// used as an example of a state machine.      

#define F_CPU 16000000             
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h> // needed for lcd_lib
#include "lcd_lib.h"
#include <math.h>     // for sine

#define countMS 63  //ticks/mSec

// ramp constants
#define RAMPUPEND 250 // = 4*62.5 or 4mSec * 62.5 samples/mSec NOTE:max=255
//#define RAMPDOWNSTART 500 // = duration*62.5
//#define RAMPDOWNEND 875 // = (duration+4)*62.5 NOTE: RAMPDOWNEND-RAMPDOWNSTART<255 
 
//I like these 
#define begin {
#define end   }   

// delta for keypad state machine
#define t3 30   

// Keypad constants
#define maxkeys 16
#define maxparam 5 
#define InvalidNum 16 // this value does not exist
#define term 10

//Keypad state machine state names
#define Released 1 
#define MaybePushed 2
#define Pushed 3
#define MaybeReleased 4

// Keypad timer
volatile unsigned char time3;

// keypad timer enable
unsigned char state_en;

// Keypad state machine variable
unsigned char PushState;  

// the decoded button number
unsigned char butnum;
unsigned char maybe;

// keypad scan table
unsigned char keytbl[16]=
{
  0x77, 0xb7, 0xd7, 0x7b, // 1 2 3 A
  0xbb, 0xdb, 0x7d, 0xbd, // 4 5 6 B
  0xdd, 0xe7, 0xeb, 0xed, // 7 8 9 C
  0xee, 0xde, 0xbe, 0x7e  // 0 F E D
};              

// key buffer
unsigned char keystr[maxparam];

// LCD buffer
unsigned char lcd_buffer[16];

// LCD strings
const int8_t cri[] PROGMEM = "ChrpRpI:\0";
const int8_t nos[] PROGMEM = "NumSyll:\0";
const int8_t sd[] PROGMEM  = "SylDurn:\0";
const int8_t sri[] PROGMEM = "SylRepI:\0";
const int8_t bf[] PROGMEM  = "BurFreq:\0";

const int8_t oor[] PROGMEM = "Out of Range!\0";
const int8_t inv[] PROGMEM = "Valid Num 0-9!\0";
const int8_t play[] PROGMEM = " playing...\0";

// The DDS variables 
volatile unsigned long accumulator;
volatile unsigned char highbyte;
volatile unsigned long increment;

// tables for DDS     
signed char sineTable[256];
char rampTable[256];

// Time variables
// the volitile is needed because the time is only set in the ISR
// time counts mSec, sample counts DDS samples (62.5 KHz)
volatile unsigned int sample, rampCount;
volatile char count;

// DDS Timers / Counters
volatile uint16_t chirpRepeatTimer;      // measures time between chirps
volatile uint8_t  syllableDurationTimer; // measures time for one syllable
volatile uint8_t  syllableRepeatTimer;   // measures time between syllables
volatile uint8_t  syllableCount;         // counts the number of syllables

// DDS Parameters
int chirpRepeatInterval;
int numberOfSyllables;
int syllableDuration;
int syllableRepeatInterval;
int burstFrequency;   // frequency of the cricket call

// DDS Ramp Variables
int rampdownstart;
int rampdownend;

// Play/Stop flag
char playing;

// Function prototypes
void scan_keypad(void); 
unsigned char get_key(void);
unsigned char check_state(void);
char get_param(int size); 
char get_call(void);
void initDDS(void);
void initialize(void); //all the usual mcu stuff 

//**********************************************************
// Timer 0 Overflow ISR
// 
// Description: If playing is enabled, generated DDS.
// Also serves as a millisecond timer.
//**********************************************************
ISR (TIMER0_OVF_vect)
begin 

  if( playing )
  begin

    if( chirpRepeatTimer == 0 )
    begin
      // reset chirp timer and syllable count
      cli();  // force atomic transaction by disabling interrupts
      chirpRepeatTimer = chirpRepeatInterval;  // takes two cycles to set 16bit int
      syllableRepeatTimer = syllableRepeatInterval;
      syllableDurationTimer = syllableDuration + 8;
      syllableCount    = numberOfSyllables;
      sei();  // renable interrupts
    end

    if( syllableRepeatTimer == 0 && syllableCount > 0 ) 
    begin
      // reset syllable cycle 
      syllableRepeatTimer = syllableRepeatInterval;
      syllableCount--;
      syllableDurationTimer = syllableDuration + 8;

      // init ramp variables
      sample    = 0;
      rampCount = 0;

      // init the DDS phase increment
      // for a 32-bit DDS accumulator, running at 16e6/256 Hz:
      // increment = 2^32*256*Fout/16e6 = 68719 * Fout
      // Fout=1000 Hz, increment= 68719000 
      increment = 68719 * burstFrequency; 

      // phase lock the sine generator DDS
      accumulator = 0 ; 
    end

    if( syllableDurationTimer > 0 )
    begin
      accumulator = accumulator + increment ;
      highbyte = (char)(accumulator >> 24) ;
      
      OCR0A = 128 + ((sineTable[highbyte] * rampTable[rampCount])>>7) ;
      
      sample++;
      if (sample <= RAMPUPEND) rampCount++ ;
      if (sample > RAMPUPEND && sample <= rampdownstart ) rampCount = 255 ;
      if (sample > rampdownstart && sample <= rampdownend ) rampCount-- ;
      if (sample > rampdownend) rampCount = 0; 
    end
    else
    begin
      OCR0A = 128;
    end

  end
  else
  begin
    OCR0A = 128;
  end

  // 63 counts is about 1 mSec
  if( --count == 0 )
  begin
    count = countMS;
    if( playing )
    begin
      --chirpRepeatTimer;
      --syllableRepeatTimer;
      --syllableDurationTimer;
    end

    if ( state_en && time3 > 0 ) --time3;
  end  

end 

//**********************************************************
// get_param
// 
// Description: 
// Fills a keypad buffer by specifying a size. Can also
// break prematurely if terminator key is detected or 
// error if key press is not 0-9. Return success or error.
//**********************************************************
char get_param(int size)
begin
  char success = 0; // Initial state is success
  unsigned char temp; 
  unsigned char index = 0; // Key buffer index
  
  // reset key buffer
  memset(keystr, 0, maxparam);
  
  while (index < size)
  begin;
    // get a key press
    temp = get_key();
    
    // Input check
    if (temp == term)
    begin
      break; // terminate input retrieval
    end
    else if(temp > 9) // Invalid Range
    begin
      CopyStringtoLCD(inv, 0, 0);
      success = -1; // output error
      break;
    end
    else
    begin
      sprintf(&keystr[index], "%i", temp); // put value into buffer
      index++;
    end
  end

  // Return error or success
  return success;
end

//**********************************************************
// get_call
// 
// Description: 
// Gets 5 parameters, print LCD statements, check for errors
// and also check parameter ranges. Return success or error.
//**********************************************************
char get_call(void)
begin
  char i;
  char success = 0; // Initial state is success
  int  temp;
  
  for(i=0; i<5; i++)
  begin

    switch (i)
    begin
      case 0:
        // Request Chirp Repeat Interval
        CopyStringtoLCD(cri, 0, 0);

        // check for valid entry
        if( get_param(maxparam) == 0 ) 
        begin
          temp = atoi(keystr); // convert input into integer

          // check valid range
          if( temp >= 10 && temp <= 1500 )
          begin
            chirpRepeatInterval = temp;
          end
          else
          begin
            CopyStringtoLCD(oor, 0, 0);
            success = -1; // output error
          end
        end
        else // error has already been found
        begin
          success = -1; // output error
        end

        break;
        
      case 1:
        // Request Number of Syllables
        CopyStringtoLCD(nos, 0, 0);

        // Clear LCD
        sprintf(lcd_buffer,"    \0");
        LCDstring(lcd_buffer, strlen(lcd_buffer));
        LCDGotoXY(8,0);

        // check for valid entry
        if( get_param(maxparam) == 0 ) 
        begin
          temp = atoi(keystr); // convert input into integer
          
          // check valid range
          if( temp >= 1 && temp <= 100 )
          begin
            numberOfSyllables = temp;
          end
          else
          begin
            CopyStringtoLCD(oor, 0, 0);
            success = -1; // output error
          end
        end
        else
        begin // error has already been found
          success = -1; // output error
        end

        break;
        
      case 2:
        // Request Syllable Duration
        CopyStringtoLCD(sd, 0, 0);

        // Clear LCD
        sprintf(lcd_buffer,"    \0");
        LCDstring(lcd_buffer, strlen(lcd_buffer));
        LCDGotoXY(8,0);
        
        // check for valid entry
        if( get_param(maxparam) == 0 ) 
        begin
          temp = atoi(keystr); // convert input into integer
          
          // check valid range
          if( temp >= 5 && temp <= 100 )
          begin
            syllableDuration = temp;
          end
          else
          begin
            CopyStringtoLCD(oor, 0, 0);
            success = -1; // output error
          end
        end
        else
        begin // error has already been found
          success = -1; // output error
        end
        break;
        
      case 3:
        // Request Syllable Repeat Interval
        CopyStringtoLCD(sri, 0, 0);

        // Clear LCD
        sprintf(lcd_buffer,"    \0");
        LCDstring(lcd_buffer, strlen(lcd_buffer));
        LCDGotoXY(8,0);
        
        // check for valid entry
        if( get_param(maxparam) == 0 ) 
        begin
          temp = atoi(keystr); // convert input into integer
          
          // check valid range
          if( temp >= 10 && temp <= 100 )
          begin
            syllableRepeatInterval = temp;
          end
          else
          begin
            CopyStringtoLCD(oor, 0, 0);
            success = -1; // output error
          end
        end
        else
        begin // error has already been found
          success = -1; // output error
        end
        break;
      
      case 4:
        // Request Burst Frequency
        CopyStringtoLCD(bf, 0, 0);

        // Clear LCD
        sprintf(lcd_buffer,"    \0");
        LCDstring(lcd_buffer, strlen(lcd_buffer));
        LCDGotoXY(8,0);
        
        // check valid range
        if( get_param(maxparam) == 0 ) 
        begin
          temp = atoi(keystr); // convert input into integer
          
          // check valid range
          if( temp >= 500 && temp <= 6000 )
          begin
            burstFrequency = temp;
          end
          else
          begin
            CopyStringtoLCD(oor, 0, 0);
            success = -1; // output error
          end
        end
        else
        begin // error has already been found
          success = -1; // output error
        end
        break;
    end

    // Check if an error happened break from parameter input
    if (success != 0)
    begin
      break;
    end

  end
  
  // Return error or success
  return success;
end

//**********************************************************
// get_key
// 
// Description: 
// Polls for key presses by scanning the keypad and debouncing
// the key press. Returns the key press value and prints it
// in LCD.
//**********************************************************
unsigned char get_key(void)
begin
  unsigned char output;
  unsigned char temp;
  
  // reset butnumber and maybe to invalid values
  butnum = InvalidNum;
  maybe = InvalidNum;

  temp = InvalidNum;

  // Intialize debounce state machine
  PushState = Released;

  // Poll for valid key value
  while( temp == InvalidNum )
  begin
    scan_keypad();
    // enable state machine timer
    state_en = 1;
    // check state machine
    if( time3 == 0 )
    begin
    
      temp = check_state();
      if( temp != InvalidNum ) // Key has been detected
      begin
        output = temp; // set output as key value
      end
      
    end
  end
  
  // disable state machine timer
  state_en = 0;
  time3 = t3;
  
  // Print out key press in LCD
  if( output != term )
  begin
    sprintf(lcd_buffer,"%i\0",output);
    LCDstring(lcd_buffer, strlen(lcd_buffer));
  end
  
  // Return key value
  return output;
end

//**********************************************************
// scan_keypad
// 
// Description: 
// Scans the keypad by checking port values then compares 
// port to key table and sets global variable with key found.
//**********************************************************
void scan_keypad(void)
begin
  unsigned char key;

  // get lower nibble
  DDRD  = 0x0f;
  PORTD = 0xf0;
  _delay_us(5);
  key = PIND;

  // get upper nibble
  DDRD  = 0xf0;
  PORTD = 0x0f;
  _delay_us(5);
  key |= PIND;

  // find matching keycode in key table
  if( key != 0xff )
  begin
    for( butnum = 0; butnum < maxkeys; butnum++ )
    begin
      if( keytbl[butnum] == key ) break;
    end

    if( butnum == maxkeys )
    begin 
      butnum = InvalidNum;
    end
    else
    begin
      butnum++; // adjust by one to make range 1-16
      // According to keypad layout butnum 16 is labeled 0
      if(butnum == 16) butnum = 0; // Set to 0
    end
  end
  else butnum = InvalidNum;
end


//**********************************************************
// check_state
// 
// Description: 
// State machine to debounce a key, a key press is valid once
// it has been released.
//**********************************************************
unsigned char check_state(void)
begin
  unsigned char output = InvalidNum; // set to invalid value
  
  time3=t3; //reset the task timer
  
  switch( PushState )
  begin
    case Released: 
      if( butnum == InvalidNum ) PushState = Released;
      else
      begin
      maybe = butnum; // set maybe to possible key press
      PushState=MaybePushed;
      end
      break;
    case MaybePushed:
      if( butnum == maybe ) PushState = Pushed;
      else PushState = Released;
      break;
    case Pushed:  
      if( butnum == maybe ) PushState = Pushed; 
      else PushState = MaybeReleased;    
      break;
    case MaybeReleased:
      if( butnum == maybe ) PushState = Pushed;
      else
      begin
        output = maybe; // once released, output maybe
        PushState = Released;
      end
      break;
  end
  
  // output key press value
  return output;
end
  
//**********************************************************
// initialize
// 
// Description: 
// Sets up global variables, initializes LCD and DDS, turns
// on interrupts.
//**********************************************************
void initialize(void)
begin
  playing = 0;
  //init the task timers
  time3 = t3;
  
  // initialize LCD
  LCDinit();
  LCDcursorOFF();
  LCDclr();
  LCDGotoXY(0,0);
  //init the state machine
  state_en  = 0;
  PushState = Released;
  
  // initialize DDS
  initDDS();

  //crank up the ISRs
  sei() ;
end 

//**********************************************************
// initDDS
// 
// Description: 
// Sets up DDS global variables, initializes Timer 0, and
// calculates a sine table.
//**********************************************************
void initDDS()
begin
 
  uint16_t i;
  // make B.3 a pwm output
  DDRB = (1<<PINB3);

  // init the sine and ramp tables
  for ( i=0; i<256; i++ )
  begin
    sineTable[i] = (char)(127.0 * sin(6.283*((float)i)/256.0));
    rampTable[i] = i>>1;
  end  

  // init the timers & counter
  count                 = countMS;
  chirpRepeatTimer      = chirpRepeatInterval;
  syllableRepeatTimer   = 0;
  syllableCount         = 0;
  syllableDurationTimer = 0;
  TCCR0B = 1;          // timer 0 runs at full rate
  TIMSK0 = (1<<TOIE0); // turn on timer 0 overflow ISR

  // turn on PWM
  // turn on fast PWM and OC0A output
  // at full clock rate, toggle OC0A (pin B3) 
  // 16 microsec per PWM cycle sample time
  TCCR0A = (1<<COM0A0) | (1<<COM0A1) | (1<<WGM00) | (1<<WGM01) ; 
  OCR0A = 128 ; // set PWM to half full scale i.e. 50% duty cycle
   
end

//**********************************************************
// main
// 
// Description: 
// On startup, get first set of parameters to initiate a 
// cricket call. Poll for play to enable DDS interrupt routine
// and for stop to disable DDS and get another set of 
// parameters.
//**********************************************************
int main(void)
begin  
  unsigned char start = 1; // start value
  unsigned char stop  = 2; // stop value
  signed char error = 0; // error flag
  unsigned char value;
  
  unsigned char startup = 1; // startup flag
  
  initialize();
  
  //main task scheduler loop 
  while(1)
  begin
    
    // On startup get first parameters
    if( startup == 1 )
    begin
      startup = 0;
      error = get_call();
      LCDclr();
    end
    
    // Poll for stop/start
    value = get_key();
    
    // If stop, get new call disable DDS
    if( value == stop )
    begin
      LCDclr();
      playing = 0;
      error = get_call();
    end
    // If start, clear LCD and enable DDS
    else if( value == start )
    begin
      LCDclr();
      CopyStringtoLCD(play, 0, 0);

      chirpRepeatTimer    = chirpRepeatInterval;
      syllableRepeatTimer = 0;
      syllableCount       = 0;
      syllableDurationTimer = 0;
      rampdownstart = syllableDuration * 62.5;
      rampdownend = (syllableDuration + 4) * 62.5;
      playing = 1;
    end
  end

end  
  

   
