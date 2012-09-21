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
#include <math.h> 		// for sine

//timeout values for each task
#define t3 30  // delta t
#define t1 200
 
//I like these 
#define begin {
#define end   }   

#define maxkeys 16
#define maxparam 5

// the decoded button number
unsigned char  butnum;
unsigned char  ready;

//key pad scan table
unsigned char keytbl[16]=
{
	0x77, 0xb7, 0xd7, 0x7b,
	0xbb, 0xdb, 0x7d, 0xbd,
	0xdd, 0xe7, 0xeb, 0xed,
	0xee, 0xde, 0xbe, 0x7e
};						  
 
unsigned char lcd_buffer[16];

unsigned char maybe;
unsigned char state_en;
unsigned char keystr[maxparam];

#define InvalidNum 16
#define term 10


//State machine state names
#define Released 1 
#define MaybePushed 2
#define Pushed 3
#define MaybeReleased 4
#define MaybeTerm 5
#define Termed 6
#define TermReleased 7
#define Done 8

// strings
const int8_t cri[] PROGMEM = "ChrpRpI:\0";
const int8_t nos[] PROGMEM = "NumSyll:\0";
const int8_t sd[] PROGMEM  = "SylDurn:\0";
const int8_t sri[] PROGMEM = "SylRepI:\0";
const int8_t bf[] PROGMEM  = "BurFreq:\0";

const int8_t oor[] PROGMEM = "Out of Range!\0";
const int8_t inv[] PROGMEM = "Valid Num 0-9!\0";
const int8_t play[] PROGMEM = " playing...\0";

          
volatile unsigned char time3;	//timeout counters 
unsigned char PushState;	//state machine  

//**********************************************************
// DDS 
//**********************************************************

#define countMS 63  //ticks/mSec

// ramp constant
#define RAMPUPEND 250 // = 4*62.5 or 4mSec * 62.5 samples/mSec NOTE:max=255

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
int chirpRepeatInterval;
int numberOfSyllables;
int syllableDuration;
int syllableRepeatInterval;
int burstFrequency;   // frequency of the cricket call

int rampdownstart;
int rampdownend;

// State Variables
char playing;

//the three task subroutines
void scan_keypad(void);	
unsigned char get_key(void);
unsigned char check_state(void);
char get_param(int size);	
char get_call(void);
void initDDS(void);

void initialize(void); //all the usual mcu stuff 

//**********************************************************


ISR (TIMER0_OVF_vect)
begin 

  if( playing ){

  	if(chirpRepeatTimer == 0)
    begin

      // reset chirp timer and syllable count
      cli();  // force atomic transaction by disabling interrupts
      chirpRepeatTimer = chirpRepeatInterval;  // takes two cycles to set 16bit int
      syllableRepeatTimer = syllableRepeatInterval;
	    syllableDurationTimer = syllableDuration + 8;
	    syllableCount    = numberOfSyllables;
  	  sei();  // renable interrupts
      
    end

    if (syllableRepeatTimer == 0 && syllableCount > 0) 
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

    if(syllableDurationTimer > 0){

    	accumulator = accumulator + increment ;
    	highbyte = (char)(accumulator >> 24) ;
    	
    	OCR0A = 128 + ((sineTable[highbyte] * rampTable[rampCount])>>7) ;
    	
    	sample++;
    	if (sample <= RAMPUPEND) rampCount++ ;
    	if (sample > RAMPUPEND && sample <= rampdownstart ) rampCount = 255 ;
    	if (sample > rampdownstart && sample <= rampdownend ) rampCount-- ;
    	if (sample > rampdownend) rampCount = 0; 

  	}else{
	  
      OCR0A = 128;

    }

  }else{

    OCR0A = 128;

  }

  // 62 counts is about 1 mSec
  if( --count == 0 )
  begin
    count = countMS;
	if( playing ){
    --chirpRepeatTimer;
    --syllableRepeatTimer;
    --syllableDurationTimer;
	}
	if (state_en && time3>0) --time3;
  end  

end 

//**********************************************************          
//Task subroutines

/**********************/
char get_param(int size)
begin
  char success = 0;
  unsigned char temp;
  unsigned char index = 0;
	
	// reset key array and butnumber
	memset(keystr, 0, maxparam);
	
	while (index < size)
	begin;
		temp = get_key();
		
		// Input check
		if (temp == term)
		{
			break; // terminate input retrieval
		}
		else if(temp > 9) // Invalid Range
		{
			CopyStringtoLCD(inv, 0, 0);
			success = -1; // output error
			break;
		}
		else if (index != (maxparam -1))
		{
			sprintf(&keystr[index], "%i", temp);
			index++;
		}
		
	end

	return success;
end

char get_call(void)
begin
    char i;
	char success = 0;
	int  temp;

	for(i=0; i<5; i++)
	begin
		switch (i)
		begin
			case 0:
				CopyStringtoLCD(cri, 0, 0);

				// check for valid entry
				if (get_param(maxparam) == 0) 
				begin
					temp = atoi(keystr); // convert input into integer

					// check valid range
					if (temp >= 10 && temp <= 1500)
					begin
						chirpRepeatInterval = temp;
					end
					else
					begin
						CopyStringtoLCD(oor, 0, 0);
						success = -1;
					end
					
				end
				else
				begin
					success = -1;
				end

				break;
				
			case 1:
				CopyStringtoLCD(nos, 0, 0);

				sprintf(lcd_buffer,"    \0");
	    		LCDstring(lcd_buffer, strlen(lcd_buffer));
				LCDGotoXY(8,0);

				// check for valid entry
				if (get_param(maxparam) == 0) 
				begin
					temp = atoi(keystr); // convert input into integer
					
					// check valid range
					if (temp >= 1 && temp <= 100)
					begin
						numberOfSyllables = temp;
					end
					else
					begin
						CopyStringtoLCD(oor, 0, 0);
						success = -1;
					end

				end
				else
				begin
					success = -1;
				end

				break;
				
			case 2:

				CopyStringtoLCD(sd, 0, 0);

				sprintf(lcd_buffer,"    \0");
	    		LCDstring(lcd_buffer, strlen(lcd_buffer));
				LCDGotoXY(8,0);
				
				if (get_param(maxparam) == 0) 
				begin
					temp = atoi(keystr); // convert input into integer
					
					// check valid range
					if (temp >= 5 && temp <= 100)
					begin
						syllableDuration = temp;
					end
					else
					begin
						CopyStringtoLCD(oor, 0, 0);
						success = -1;
					end

				end
				else
				begin
					success = -1;
				end
				break;
				
			case 3:

				CopyStringtoLCD(sri, 0, 0);

				sprintf(lcd_buffer,"    \0");
	    		LCDstring(lcd_buffer, strlen(lcd_buffer));
				LCDGotoXY(8,0);
				
				if (get_param(maxparam) == 0) 
				begin
					temp = atoi(keystr); // convert input into integer
					
					// check valid range
					if (temp >= 10 && temp <= 100)
					begin
						syllableRepeatInterval = temp;
					end
					else
					begin
						CopyStringtoLCD(oor, 0, 0);
						success = -1;
					end

				end
				else
				begin
					success = -1;
				end
				break;
			
			case 4:

				CopyStringtoLCD(bf, 0, 0);

				sprintf(lcd_buffer,"    \0");
	    		LCDstring(lcd_buffer, strlen(lcd_buffer));
				LCDGotoXY(8,0);
				
				if (get_param(maxparam) == 0) 
				begin
					temp = atoi(keystr); // convert input into integer
					
					// check valid range
					if (temp >= 500 && temp <= 6000)
					begin
						burstFrequency = temp;
					end
					else
					begin
						CopyStringtoLCD(oor, 0, 0);
						success = -1;
					end

				end
				else
				begin
					success = -1;
				end
				break;
		end

		// Check if an error happened break from parameter input
		if (success != 0)
		begin
			break;
		end
		
	end
	
	return success;
end

unsigned char get_key(void)
begin
	unsigned char output;
	unsigned char temp;
	
	// reset butnumber
	butnum = 0;
	maybe = 0;
	temp = InvalidNum;
	ready = 0;
	PushState = Released;

	while (ready == 0)
	begin
	
		scan_keypad();
		// enable state machine timer
		state_en = 1;

		if(time3 == 0)
		begin
		
			temp = check_state();
			if(temp != InvalidNum) // Number has not been detected
			begin
				output = temp; 
			end
			
		end
	end
	
    // reset state machine
	state_en = 0;
	time3 = t3;
	
	if (output != term)
	{
	sprintf(lcd_buffer,"%i\0",output);
	LCDstring(lcd_buffer, strlen(lcd_buffer));
	}
	
	return output;
end

/*********************/
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

  // find matching keycode in keytbl
  if ( key != 0xff )
  begin
    for ( butnum = 0; butnum < maxkeys; butnum++ )
    begin
      if ( keytbl[butnum] == key ) break;
    end

    if ( butnum == maxkeys )
		{ 
			butnum = InvalidNum;
		}
    else
		{ 
			butnum++; // adjust by one to make range 1-16
			
			// According to keypad layout butnum 16 is labeled 0
			if(butnum == 16) butnum = 0; // Set to 0
		}
  end
  else butnum = InvalidNum;
end


//Task 3  
unsigned char check_state(void)
begin
  unsigned char output = InvalidNum;
  
  time3=t3;     //reset the task timer
	
  switch (PushState)
  begin
    case Released: 
      if (butnum == InvalidNum) PushState=Released;
      else
			{
	  	maybe = butnum;  
	  	PushState=MaybePushed;
      }
      break;
    case MaybePushed:
      if (butnum == maybe) PushState = MaybeTerm;
      else PushState = Released;
      break;
    case Pushed:  
      if (butnum == maybe) PushState = Pushed; 
      else PushState = MaybeReleased;    
      break;
    case MaybeReleased:
      if (butnum == maybe) PushState = Pushed;
      else
			{
				output = maybe;
				ready = 1;
				PushState = Released;
			}
      break;
    case MaybeTerm:
      //if (butnum == term) PushState = Termed;
      //else PushState = Pushed;
			PushState = Pushed;
      break;
    case Termed:
      if (butnum == maybe) PushState = Termed;
      else PushState = TermReleased;
      break;
    case TermReleased:
      if (butnum == maybe) PushState = Termed;
      else PushState = Done;
      break;
    case Done:
      break;
  end
	
	return output;
end
  
//********************************************************** 
//Set it all up
void initialize(void)
begin
//set up the ports

  ready = 0;
  playing = 0;
  //init the task timers
  time3=t3;
  
  // initialize LCD
  LCDinit();
  LCDcursorOFF();
  LCDclr();
  LCDGotoXY(0,0);
  //init the state machine
	state_en  = 0;
  PushState = Released;
      
  initDDS();

  //crank up the ISRs
  sei() ;
  
end 

void initDDS()
begin
 
  uint16_t i;
  // make B.3 a pwm output
  DDRB = (1<<PINB3);

  // init the sine and ramp tables
  for (i=0; i<256; i++)
  begin
    sineTable[i] = (char)(127.0 * sin(6.283*((float)i)/256.0));
    rampTable[i] = i>>1;
  end  

  // init the timers & counter
  count               = countMS;
  chirpRepeatTimer    = chirpRepeatInterval;
  syllableRepeatTimer = 0;
  syllableCount       = 0;
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
//Entry point and task scheduler loop
int main(void)
begin  
	unsigned char start = 1;
	unsigned char stop  = 2;
	signed char error = 0;
	unsigned char value;
	
	unsigned char startup = 1;
	
  initialize();
  
  //main task scheduler loop 
	
  while(1)
  begin
        
		if (startup == 1)
		{
			startup = 0;
			error = get_call();
			LCDclr();
		}
		
		value = get_key();
		if (value == stop)
		begin
			LCDclr();
			playing = 0;
			error = get_call();
		end
		else if (value == start)
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
  

   
