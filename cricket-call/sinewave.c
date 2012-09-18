// DDS output thru PWM on timer0 OC0A (pin B.3)
// Mega644 version
// Produces a 15 mSec sine wave burst every 50 mSec.
// with liner tapered ends

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h> 		// for sine

//I like these definitions
#define begin {
#define end   } 

#define countMS 62  //ticks/mSec

#define syllable_repeat_interval 30
#define syllable_duration 18
#define syllable_count 4
// ramp constants
#define RAMPUPEND 250 // = 4*62.5 or 4mSec * 62.5 samples/mSec NOTE:max=255
#define RAMPDOWNSTART 625 // = 10*62.5
#define RAMPDOWNEND 875 // = 14*62.5 NOTE: RAMPDOWNEND-RAMPDOWNSTART<255 

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
volatile unsigned int time, sample, rampCount, chirp_time, syl_time, syl_dur;
volatile char count, scount;

// index for sine table build
unsigned int i;

ISR (TIMER0_OVF_vect)
begin 
	//the actual DDR 
	accumulator = accumulator + increment ;
	highbyte = (char)(accumulator >> 24) ;
	
	// output the wavefrom sample
	OCR0A = 128 + ((sineTable[highbyte] * rampTable[rampCount])>>7) ;
	
	sample++ ;
	if (sample <= RAMPUPEND) rampCount++ ;
	if (sample > RAMPUPEND && sample <= RAMPDOWNSTART ) rampCount = 255 ;
	if (sample > RAMPDOWNSTART && sample <= RAMPDOWNEND ) rampCount-- ;
	if (sample > RAMPDOWNEND) rampCount = 0; 
	
	// generate time base for MAIN
	// 62 counts is about 1 mSec
	count--;
	if (0 == count )
	begin
    // all time updates should go here
		count=countMS;
		time++;    //in mSec
    chirp_time++;
    syl_time++;
    syl_dur++;
	end  

  // if playing set ocr0a to dds, else set to 128
end 
 
int main(void)
begin 
   
  // make B.3 an output
  DDRB = (1<<PINB3) ;
   
  // init the sine table
  for (i=0; i<256; i++)
  begin
  	sineTable[i] = (char)(127.0 * sin(6.283*((float)i)/256.0)) ;
		// the following table needs 
		// rampTable[0]=0 and rampTable[255]=127
		rampTable[i] = i>>1 ;
  end  

  // init the time counter
  time=0;
  chirp_time = 0;
  syl_timer  = 0;
  syl_dur    = 0;
  scount=0;

  // timer 0 runs at full rate
  TCCR0B = 1 ;  
  //turn on timer 0 overflow ISR
  TIMSK0 = (1<<TOIE0) ;
  // turn on PWM
  // turn on fast PWM and OC0A output
  // at full clock rate, toggle OC0A (pin B3) 
  // 16 microsec per PWM cycle sample time
  TCCR0A = (1<<COM0A0) | (1<<COM0A1) | (1<<WGM00) | (1<<WGM01) ; 
  OCR0A = 128 ; // set PWM to half full scale i.e. 50% duty cycle

  // turn on all ISRs
  sei() ;
   
  while(1) 
  begin  
   
    if(chirp_time==chirp_repeat_interval)
    begin
      ctime=0;
      if (syl_time==syllable_repeat_interval) 
      begin
  	    // start a new syllable cycle 
        time=0;
  		 
  		  // init ramp variables
  		  sample = 0 ;
  		  rampCount = 0;

        // init the DDS phase increment
     		// for a 32-bit DDS accumulator, running at 16e6/256 Hz:
     		// increment = 2^32*256*Fout/16e6 = 68719 * Fout
    		// Fout=1000 Hz, increment= 68719000 
     		increment = 68719000L; 

  		  // phase lock the sine generator DDS
        accumulator = 0 ;

  		  TCCR0A = (1<<COM0A0) | (1<<COM0A1) | (1<<WGM00) | (1<<WGM01) ; 	  
    
      end //if (time==50)

      // after syllable duration (ms) turn off PWM -- don't do this!
      if (syl_dur==syllable_duration || scount==syllable_count) OCR0A = 128 ;
    end
  end // end while(1)
end  //end main
      
