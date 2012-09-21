// DDS output thru PWM on timer0 OC0A (pin B.3)

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h> 		// for sine

//I like these definitions
#define begin {
#define end   } 

#define countMS 63  //ticks/mSec

//#define chirpRepeatInterval 20
//#define syllableRepeatInterval 10
//#define syllableDuration 10
//#define numberOfSyllables 1
//#define burstFrequency 4500

// ramp constants
#define RAMPUPEND 250 // = 4*62.5 or 4mSec * 62.5 samples/mSec NOTE:max=255
//#define RAMPDOWNSTART 625 // = 10*62.5
//#define RAMPDOWNEND 875 // = 14*62.5 NOTE: RAMPDOWNEND-RAMPDOWNSTART<255 

// The DDS variables 
volatile unsigned long accumulator ;
volatile unsigned char highbyte ;
volatile unsigned long increment;

// tables for DDS			
signed char sineTable[256] ;
char rampTable[256] ;

// Time variables
// the volitile is needed because the time is only set in the ISR
// time counts mSec, sample counts DDS samples (62.5 KHz)
volatile unsigned int sample, rampCount;
volatile char count;

// Timers / Counters
volatile uint16_t chirpRepeatTimer;      // measures time between chirps
volatile uint8_t  syllableDurationTimer; // measures time for one syllable
volatile uint8_t  syllableRepeatTimer;   // measures time between syllables
volatile uint8_t  syllableCount;         // counts the number of syllables

// Parameters
uint16_t chirpRepeatInterval;
uint8_t  numberOfSyllables;
uint8_t  syllableDuration;
uint8_t  syllableRepeatInterval;
uint16_t burstFrequency;   // frequency of the cricket call
uint16_t rampDownStart;  // = syllableDuration * 62.5 
uint16_t rampDownEnd;    // = (syllableDuration + 4) * 62.5

// State Variables
char playing;

ISR (TIMER0_OVF_vect)
begin 

  if( playing ){

    if(chirpRepeatTimer == 0)
    begin
      // reset all timers and counters
      cli();  // force atomic transaction by disabling interrupts
      chirpRepeatTimer      = chirpRepeatInterval;  // takes two cycles to set 16bit int
      syllableRepeatTimer   = syllableRepeatInterval;
	    syllableDurationTimer = syllableDuration + 8;
  	  syllableCount         = numberOfSyllables;
	    sei();  // renable interrupts
        
    end

    if (syllableRepeatTimer == 0 && syllableCount > 0) 
    begin

      // reset syllable timers
      syllableRepeatTimer   = syllableRepeatInterval;
      syllableDurationTimer = syllableDuration + 8;

      // update syllable counter
      syllableCount--;
        
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

    if(syllableDurationTimer > 0){

    	accumulator = accumulator + increment ;
    	highbyte = (char)(accumulator >> 24) ;
    	
    	OCR0A = 128 + ((sineTable[highbyte] * rampTable[rampCount])>>7) ;
    	
    	sample++;
    	if (sample <= RAMPUPEND) rampCount++ ;
    	if (sample > RAMPUPEND && sample <= RAMPDOWNSTART ) rampCount = 255 ;
    	if (sample > RAMPDOWNSTART && sample <= RAMPDOWNEND ) rampCount-- ;
    	if (sample > RAMPDOWNEND) rampCount = 0; 

  	}else{
	  
      OCR0A = 128;

    }

  	// 62 counts is about 1 mSec
  	if( --count == 0 )
  	begin
  	  count = countMS;
      --chirpRepeatTimer;
      --syllableRepeatTimer;
      --syllableDurationTimer;
  	end  

  }else{

    OCR0A = 128;

  }

end 
 
void initDDS()
begin

  uint16_t i; // temporary index variable

  // make B.3 a pwm output
  DDRB = (1<<PINB3);

  // init the sine and ramp tables
  for (i=0; i<256; i++)
  begin
    sineTable[i] = (char)(127.0 * sin(6.283*((float)i)/256.0));
    rampTable[i] = i>>1;
  end  

  // init the timers & counter
  count                 = countMS;
  chirpRepeatTimer      = chirpRepeatInterval;
  syllableRepeatTimer   = 0; // init to 0 to allow buffer interval
  syllableCount         = 0;
	syllableDurationTimer = 0;
  TCCR0B                = 1; // timer 0 runs at full rate
  TIMSK0                = (1<<TOIE0); // turn on timer 0 overflow ISR

  // turn on PWM
  // turn on fast PWM and OC0A output
  // at full clock rate, toggle OC0A (pin B3) 
  // 16 microsec per PWM cycle sample time
  TCCR0A = (1<<COM0A0) | (1<<COM0A1) | (1<<WGM00) | (1<<WGM01) ; 
  OCR0A = 128 ; // set PWM to half full scale i.e. 50% duty cycle

  // turn on all ISRs
  sei() ;
   
end

int main(void)
begin 

  initDDS();
  playing = 1;
  
  if( playing )
  begin 
    
    while(1) 
    begin  
    end // end while(1)

  end // if(playing)

end  //end main      
