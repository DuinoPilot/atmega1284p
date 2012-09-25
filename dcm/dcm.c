// ECE 4760 - Lab 1 Digital Capacitance Meter
// Jeff Mu, jm776
// Edgar Munoz, em447

// Run both timers in different modes
// timer 0 -- compare-match timebase
// timer 1 -- input capture from analog comparator

// main will schedule task1
// task 1 will calculate print the capacitance
// timer0 compare ISR will increment a timer and toggle LED when appropriate
// timer1 capture ISR will capture the time capacitor takes to charge to level

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h> // needed for lcd_lib
#include "lcd_lib.h"

// time between LCD updates
#define t1 200
// time between LED toggles
#define t2 500

// resistor value for RC circuit in kOhms
#define R2 10.0

#define begin {
#define end }
 
//**********************************************************
// LCD setup
void init_lcd(void) 
begin
  LCDinit();  //initialize the display
  LCDcursorOFF();
  LCDclr(); //clear the display
  LCDGotoXY(0,0);
end	

//**********************************************************
// the task subroutine
void task1(void); //execute every 200ms

void initialize(void); // all the usual mcu stuff 

// timeout counter for task1         
volatile unsigned char time1; 

// timer 1 capture variables for capacitance charge time, timeout counter for LED
volatile unsigned int T1capture, time2; 

int8_t lcd_buffer[17];	// LCD display buffer
volatile double capacitance;  // capacitance to display on the LCD  
const int8_t c_equals[] PROGMEM = "C = \0";
const int8_t no_capacitance[] PROGMEM = "No capacitor.\0";
				 
//**********************************************************
//timer 0 overflow ISR
ISR (TIMER0_COMPA_vect) 
begin      
  //Decrement the time if not already zero
  if (time1>0)	--time1;
  if (time2>0)  --time2;
  // toggle led in ISR due to delay that task1 causes
  if (time2 == 0) {
  	PORTD ^= 0x04;
	time2 = t2;
  }
end  

//**********************************************************
// timer 1 capture ISR
ISR (TIMER1_CAPT_vect)
begin
    // read timer1 input capture register
    T1capture = ICR1 ; 
end

//**********************************************************       
//Entry point and task scheduler loop
int main(void)
begin  
  // start the LCD 
  init_lcd();
  // initialize ports, timers, and ISR
  initialize();
  
  // main task scheduler loop 
  while(1)
  begin
    // task1 calculates and prints capacitance
    if (time1==0){time1=t1; task1();}
  end
end  
  
//**********************************************************          
//Timed task subroutine
//Task 1
void task1(void) 
begin 

  LCDclr(); 
  DDRB |= 0x04; // Set B.2 to output

  CopyStringtoLCD(c_equals, 0, 0); // write "C =" to LCD
  
  TCNT1 = 0;  // start counter
  DDRB = 0; // Set B.2 to input

  // capture interrupt trigger happens
  capacitance = ((double) T1capture / 16.0)/ R2; // capacitance in nF

  sprintf(lcd_buffer,"%0.2f nF",capacitance);	

  // check if value within 1% of 1 nF
  if ( capacitance <= .9 ) {
    CopyStringtoLCD(no_capacitance, 4, 0);
  } else {	
    // print capture interval 
    LCDstring(lcd_buffer, strlen(lcd_buffer));
  }

end  
 

//********************************************************** 
//Set it all up
void initialize(void)
begin

  // Set up ports for comparator
  DDRB = 0; // Set all Port B pins to input
  PORTB = 0; // Disable all pull up resistors
  
  // Set up ports for onboard LED
  DDRD = 0x04; 
  PORTD = 0;
 
  //set up timer 0 for 1 mSec timebase 
  TIMSK0= (1<<OCIE0A);	//turn on timer 0 cmp match ISR 
  OCR0A = 249;  		//set the compare re to 250 time ticks
  //set prescalar to divide by 64 
  TCCR0B= 3; //0b00001011;	
  // turn on clear-on-match
  TCCR0A= (1<<WGM01);
  
  //set up timer1 for full speed and
  //capture an edge on analog comparator pin B.3 
  // Set capture to positive edge, full counting rate
  TCCR1B = (1<<ICES1) + 1; 
  // Turn on timer1 interrupt-on-capture
  TIMSK1 = (1<<ICIE1);

  // Set analog comp to connect to timer capture input 
  ACSR = (1<<ACIC); //0b01000100  ;
  
  //init the task timer
  time1 = t1;
  time2 = t2;
    
  //crank up the ISRs
  sei();

end  
//==================================================

   
