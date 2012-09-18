                 
// Keyscanner
// Mega644 version 
// assumes a standard 4x4 keypad connected to a port (example is PORTD)

#include <avr/io.h>
#include <util/delay.h>
      
#define maxkeys 16
#define PORTDIR DDRD
#define PORTDATA PORTD
#define PORTIN PIND

#define begin {
#define end }

// The raw keyscan
unsigned char key ;   
// The decoded button number
unsigned char butnum ;

//key pad scan table
unsigned char keytbl[16]={0xee, 0xed, 0xeb, 0xe7, 
						  0xde, 0xdd, 0xdb, 0xd7, 
						  0xbe, 0xbd, 0xbb, 0xb7, 
						  0x7e, 0x7d, 0x7b, 0x77};

int main(void)
begin

  //endless loop to read keyboard
  while(1)
  begin 
	//get lower nibble
  	PORTDIR = 0x0f;
  	PORTDATA = 0xf0; 
  	_delay_us(5);
  	key = PORTIN;
  	  
  	//get upper nibble
  	PORTDIR = 0xf0;
  	PORTDATA = 0x0f; 
  	_delay_us(5);
  	key = key | PORTIN;
  	  
  	//find matching keycode in keytbl
  	if (key != 0xff)
  	begin   
  	  for (butnum=0; butnum<maxkeys; butnum++)
  	  begin   
  	  	if (keytbl[butnum]==key)  break;   
  	  end

  	  if (butnum==maxkeys) butnum=0;
  	  else butnum++;	   //adjust by one to make range 1-16
  	end  
  	else butnum=0;
  	
  	end // end while
  end   //end main
