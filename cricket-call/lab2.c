// Debounce demo 
// Mega644 version
// used as an example of a state machine.      
             
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h> // needed for lcd_lib
#include "lcd_lib.h"

//timeout values for each task
#define t3 30  // delta t
#define t1 200
 
//I like these 
#define begin {
#define end   }   

#define maxkeys 16
#define maxparam 4

// the decoded button number
unsigned char  butnum;
unsigned char  ready;

//key pad scan table
unsigned char keytbl[16]={0xee, 0xed, 0xeb, 0xe7,
                          0xde, 0xdd, 0xdb, 0xd7, 
                          0xbe, 0xbd, 0xbb, 0xb7, 
                          0x7e, 0x7d, 0x7b, 0x77};

unsigned char lcd_buffer[16];

unsigned char count;
unsigned char state_en;
unsigned char keystr[maxparam];
unsigned char param_array[5][maxparam];
#define term 0x77


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
const int8_t cri[] PROGMEM = "CRI:\0";

//the three task subroutines
void scan_keypad(void);	
unsigned char get_key(void);
unsigned char check_state(void);	
void get_call(void);
void update_lcd(void);

void initialize(void); //all the usual mcu stuff 
          
volatile unsigned char time3, time1;	//timeout counters 
unsigned char PushState;	//state machine  
         
//**********************************************************
//timer 0 comare match ISR
ISR (TIMER0_COMPA_vect) 
begin
  //Decrement the three times if they are not already zero
  if (state_en && time3>0) --time3;
  if (time1 > 0)
  begin
  	--time1;
  end
  else if(time1 == 0)
  begin
  	update_lcd();
  end

end  

//**********************************************************          
//Task subroutines
//

void update_lcd(void)
{
 	LCDstring(lcd_buffer, strlen(lcd_buffer));
}

/**********************/
void get_param(void)
begin
	
    // reset key array and butnumber
	memset(&keystr, 0, maxparam);
	count = 0;
	while (count < maxparam)
	begin;
		keystr[count++] = get_key();
	end
	

end

void get_call(void)
begin
  
	for(unsigned char i=0; i<5; i++)
	begin
		LCDclr();
		switch (i)
		begin
			case 0:
			//	CopyStringtoLCD(cri, 0, 0);
				get_param();
				memset( &param_array[i][maxparam], 0, maxparam);
				memcpy( &param_array[i][maxparam], &keystr, maxparam);
				break;
				
			case 1:
				CopyStringtoLCD(cri, 0, 0);
				get_param();
				memset( &param_array[i][maxparam], 0, maxparam);
				memcpy( &param_array[i][maxparam], &keystr, maxparam);
				break;
				
			case 2:
			//	CopyStringtoLCD(cri, 0, 0);
				get_param();
				memset( &param_array[i][maxparam], 0, maxparam);
				memcpy( &param_array[i][maxparam], &keystr, maxparam);
				break;
				
			case 3:
			//	CopyStringtoLCD(cri, 0, 0);
				get_param();
				memset( &param_array[i][maxparam], 0, maxparam);
				memcpy( &param_array[i][maxparam], &keystr, maxparam);
				break;
			
			case 4:
			//	CopyStringtoLCD(cri, 0, 0);
				get_param();
				memset( &param_array[i][maxparam], 0, maxparam);
				memcpy( &param_array[i][maxparam], &keystr, maxparam);
				break;
		end
		
	end
end

unsigned char get_key(void)
begin
	unsigned char output;
	unsigned char temp;
	
    // reset butnumber
	butnum = 0;
	
	while (~ready)
	begin
		scan_keypad();
		// enable state machine timer
		state_en = 1;

		//if(time3 == 0)
		//begin
		
			temp = check_state();
			if(temp != 0 && temp != term)
			begin
				output = temp;
			end
			
		//end
	end
	
  // reset state machine
	state_en = 0;
	time3 = t3;
	
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

    if ( butnum == maxkeys ) butnum = 0;
    else butnum++;  // adjust by one to make range 1-16
  end
  else butnum = 0;
end


//Task 3  
unsigned char check_state(void)
begin
	unsigned char output = 0;
	unsigned char test[2];
  time3=t3;     //reset the task timer
  unsigned char maybe = butnum;
	
  switch (PushState)
  begin
    case Released: 
      if (butnum == 0) PushState=Released;
      else  PushState=MaybePushed;
      break;
    case MaybePushed:
      if (butnum == maybe) {
        PushState = MaybeTerm;
        output = butnum;
		sprintf(lcd_buffer,"%i\0",output);
		//LCDstring(test, strlen(test));
      }
      else PushState = Released;
      break;
    case Pushed:  
      if (butnum == maybe) PushState = Pushed; 
      else PushState = MaybeReleased;    
      break;
    case MaybeReleased:
      if (butnum == maybe) PushState = Pushed; 
      else PushState = Released;
      break;
    case MaybeTerm:
      if (butnum == term) PushState = Termed;
      else PushState = Pushed;
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
	  sprintf(keystr,"\0");
      ready = 1;
      break;
  end
	
	return output;
end
  
//********************************************************** 
//Set it all up
void initialize(void)
begin
//set up the ports
           
  //set up timer 0 for 1 mSec timebase 
  OCR0A = 249;  		//set the compare re to 250 time ticks
  TIMSK0= (1<<OCIE0A);	//turn on timer 0 cmp match ISR 
  //set prescalar to divide by 64 
  TCCR0B= 3; //0b00000011;	
  // turn on clear-on-match
  TCCR0A= (1<<WGM01) ;

  count = 0;
  ready = 0;

  //init the task timers
  time3=t3;
  time1=t1;  
  
  // initialize LCD
  LCDinit();
  LCDcursorOFF();
  LCDclr();
  LCDGotoXY(0,0);
  //init the state machine
	state_en  = 0;
  PushState = Released;
      
  //crank up the ISRs
  sei() ;
  
end 

//**********************************************************       
//Entry point and task scheduler loop
int main(void)
begin  
	unsigned char start = 1;
	unsigned char stop  = 2;
	unsigned char value;
	
	unsigned char startup = 1;
	
  initialize();
  
  //main task scheduler loop 
	
	//get_call();
  while(1)
  begin

		get_call();
		/*value = get_key();
		if (value == stop)
		begin
			get_call();
		end
		else if (value == start)
		begin
			//play_sound();
		end*/
		
  end

end  
  

   
