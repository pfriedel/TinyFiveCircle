// Because I tend to forget - the number here is the Port B pin in use.  Pin 5 is PB0, Pin 6 is PB1, etc.
#define LINE_A 0 // PB0 / Pin 5 on ATtiny85 / Pin 14 on an ATmega328 (D8)
#define LINE_B 1 // PB1 / Pin 6 on ATtiny85 / Pin 15 on an ATmega328 (D9)
#define LINE_C 2 // PB2 / Pin 7 on ATtiny85 / Pin 17 on an ATmega328 (D11)
#define LINE_D 3 // PB3 / Pin 2 on ATtiny85 / Pin 18 on an ATmega328 (D12)
#define LINE_E 4 // PB4 / Pin 3 on ATtiny85

#include <avr/power.h>
#include <EEPROM.h>

// How many modes do we want to go through?
#define MAX_MODE 9
// How long should I draw each color on each LED?
#define DRAW_TIME 5

// Location of the brown-out disable switch in the MCU Control Register (MCUCR)
#define BODS 7 
// Location of the Brown-out disable switch enable bit in the MCU Control Register (MCUCR)
#define BODSE 2 

// The base unit for the time comparison. 1000=1s, 10000=10s, 60000=1m, etc.
//#define time_multiplier 1000 
#define time_multiplier 60000
// How many time_multiplier intervals should run before going to sleep?
#define run_time 240

#define MAX_HUE 360 // normalized

// How bit-crushed do you want the bit depth to be?  1 << TIMESCALE is how
// quickly it goes through the LED softpwm scale.

// 1 means 128 shades per color.  3 is 32 colors while 6 is 4 shades per color.
// it sort of scales the time drawing routine, but not well.

// It also affects how bright it it (less time spent drawing nothing) as well as
// the POV flickering rate. Higher numbers are both brighter and less flicker-y.

// the build uses 1 for the PTH versions and 3 for the SMD displays.

#define TIMESCALE 3

byte __attribute__ ((section (".noinit"))) last_mode;

uint8_t led_grid[15] = {
  000 , 000 , 000 , 000 , 000 , // R
  000 , 000 , 000 , 000 , 000 , // G
  000 , 000 , 000 , 000 , 000  // B
};

uint8_t led_grid_next[15] = {
  000 , 000 , 000 , 000 , 000 , // R
  000 , 000 , 000 , 000 , 000 , // G
  000 , 000 , 000 , 000 , 000  // B
};


void setup() {
  Serial.begin(115200);
}

void loop() {
  HueWalk(run_time,millis(),5,1); // 1:1 space to LED, fast progression
}

void HueWalk(uint16_t time, uint32_t start_time, uint8_t width, uint8_t speed) {
  while(1) {
    
    if(millis() >= (start_time + (time * time_multiplier))) { break; }

    for(int16_t colorshift=MAX_HUE; colorshift>0; colorshift = colorshift - speed) {

      if(millis() >= (start_time + (time * time_multiplier))) { break; }
      for(uint8_t led = 0; led<5; led++) {
	//	if(led == 0) { Serial.println(colorshift); }

	uint16_t hue = ((led) * MAX_HUE/(width)+colorshift)%MAX_HUE;
	setLedColorHSV(led, hue, 255, 255);
      }
      draw_for_time(DRAW_TIME); // this gets called 5 times, once per LED.
    }
  }
}


const byte dim_curve[] = {
  0,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,   4,   4,
  4,   4,   4,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   6,   6,   6,
  6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   7,   8,   8,   8,   8,
  8,   8,   9,   9,   9,   9,   9,   9,   10,  10,  10,  10,  10,  11,  11,  11,
  11,  11,  12,  12,  12,  12,  12,  13,  13,  13,  13,  14,  14,  14,  14,  15,
  15,  15,  16,  16,  16,  16,  17,  17,  17,  18,  18,  18,  19,  19,  19,  20,
  20,  20,  21,  21,  22,  22,  22,  23,  23,  24,  24,  25,  25,  25,  26,  26,
  27,  27,  28,  28,  29,  29,  30,  30,  31,  32,  32,  33,  33,  34,  35,  35,
  36,  36,  37,  38,  38,  39,  40,  40,  41,  42,  43,  43,  44,  45,  46,  47,
  48,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
  63,  64,  65,  66,  68,  69,  70,  71,  73,  74,  75,  76,  78,  79,  81,  82,
  83,  85,  86,  88,  90,  91,  93,  94,  96,  98,  99,  101, 103, 105, 107, 109,
  110, 112, 114, 116, 118, 121, 123, 125, 127, 129, 132, 134, 136, 139, 141, 144,
  146, 149, 151, 154, 157, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 190,
  193, 196, 200, 203, 207, 211, 214, 218, 222, 226, 230, 234, 238, 242, 248, 255,
};


/*
Inputs:
  p : LED to set
  immediate : Whether the change should go to led_grid or led_grid_next
  hue : 0-360 - color
  sat : 0-255 - how saturated should it be? 0=white, 255=full color
  val : 0-255 - how bright should it be? 0=off, 255=full bright
*/
void setLedColorHSV(uint8_t p, int16_t hue, int16_t sat, int16_t val) {


  /*
    so dim_curve duplicates too many of the low numbers too long, causing the
    animations to look janky.  Maybe 6 2's and 14 3's can be adjusted in other ways.
    
    val = dim_curve[val];
    sat = 255-dim_curve[255-sat];
  */

  while (hue > 360) hue -= 361;
  while (hue < 0) hue += 361;

  int r, g, b, base;

  if (sat == 0) { // Acromatic color (gray). Hue doesn't mind.
    r = g = b = val;
  }
  else {
    base = ((255 - sat) * val)>>8;

    switch(hue/60) {
    case 0:
      r = val;
      g = (((val-base)*hue)/60)+base;
      b = base;
      break;

    case 1:
      r = (((val-base)*(60-(hue%60)))/60)+base;
      g = val;
      b = base;
      break;

    case 2:
      r = base;
      g = val;
      b = (((val-base)*(hue%60))/60)+base;
      break;

    case 3:
      r = base;
      g = (((val-base)*(60-(hue%60)))/60)+base;
      b = val;
      break;

    case 4:
      r = (((val-base)*(hue%60))/60)+base;
      g = base;
      b = val;
      break;

    case 5:
      r = val;
      g = base;
      b = (((val-base)*(60-(hue%60)))/60)+base;
      break;
    }
  }

  // output range is 0-255

  r = r>>TIMESCALE;
  g = g>>TIMESCALE;
  b = b>>TIMESCALE;

  if(p == 0) { 
    Serial.print(hue); 
    Serial.print("  r: "); Serial.print(r);
    Serial.print(" g: "); Serial.print(g);
    Serial.print(" b: "); Serial.println(b);
    
  }


  set_led_rgb(p,r,g,b);
}

/* Args:
   position - 0-4
   red value - 0-255
   green value - 0-255
   blue value - 0-255
*/
void set_led_rgb (uint8_t p, uint8_t r, uint8_t g, uint8_t b) {
  led_grid[p] = r;
  led_grid[p+5] = g;
  led_grid[p+10] = b;
}

// runs draw_frame a supplied number of times.
void draw_for_time(uint16_t time) {
  Serial.print("BEGINNING draw_for_time for ");
  Serial.print(time * (1<<(TIMESCALE-1)));
  Serial.println(" loops.");
  
  for(uint16_t f = 0; f<time * (1 << (TIMESCALE-1)); f++) { 
    draw_frame();   
  }
  Serial.println("ENDING draw_for_time");
  
}

// Anode | cathode
const uint8_t led_dir[15] = {
  ( 1<<LINE_B | 1<<LINE_A ), // 4 r
  ( 1<<LINE_A | 1<<LINE_E ), // 5 r
  ( 1<<LINE_E | 1<<LINE_D ), // 1 r
  ( 1<<LINE_D | 1<<LINE_C ), // 2 r
  ( 1<<LINE_C | 1<<LINE_B ), // 3 r

  ( 1<<LINE_B | 1<<LINE_C ), // 4 g
  ( 1<<LINE_A | 1<<LINE_B ), // 5 g
  ( 1<<LINE_E | 1<<LINE_A ), // 1 g
  ( 1<<LINE_D | 1<<LINE_E ), // 2 g
  ( 1<<LINE_C | 1<<LINE_D ), // 3 g

  ( 1<<LINE_B | 1<<LINE_D ), // 4 b
  ( 1<<LINE_A | 1<<LINE_C ), // 5 b
  ( 1<<LINE_E | 1<<LINE_B ), // 1 b
  ( 1<<LINE_D | 1<<LINE_A ), // 2 b
  ( 1<<LINE_C | 1<<LINE_E ), // 3 b
};

//PORTB output config for each LED (1 = High, 0 = Low)
const uint8_t led_out[15] = {
  ( 1<<LINE_B ), // 4 r
  ( 1<<LINE_A ), // 5 r
  ( 1<<LINE_E ), // 1 r
  ( 1<<LINE_D ), // 2 r
  ( 1<<LINE_C ), // 3 r
  
  ( 1<<LINE_B ), // 4 g
  ( 1<<LINE_A ), // 5 g
  ( 1<<LINE_E ), // 1 g
  ( 1<<LINE_D ), // 2 g
  ( 1<<LINE_C ), // 3 g
  
  ( 1<<LINE_B ), // 4 b
  ( 1<<LINE_A ), // 5 b
  ( 1<<LINE_E ), // 1 b
  ( 1<<LINE_D ), // 2 b
  ( 1<<LINE_C ), // 3 b
};

void light_led(uint8_t led_num) { //led_num must be from 0 to 19
  //DDRD is the ports in use on an ATmega328, DDRB on an ATtiny85
  DDRB = led_dir[led_num];
  PORTB = led_out[led_num];
}

void leds_off() {
  DDRB = 0;
  PORTB = 0;
}

void draw_frame(void){
  uint16_t b;
  uint8_t led;
  // giving the loop a bit of breathing room seems to prevent the last LED from flickering.  Probably optimizes into oblivion anyway.
  for ( led=0; led<=15; led++ ) { 

    for( b=0; b<led_grid[led]; b++) {
      light_led(led);
      if(led == 0) {
	Serial.print(1);
      }
    }
    // and turn the LEDs off for the amount of time in the led_grid array between LED brightness and 255.
    // BUG alert - at timescale 3, 
    // input "68" is 33 total cycles, 9 lit and 24 dark, 
    // input "63" is 32 total cycles, 8 lit and 24 dark.                                                                              
    for( b=led_grid[led]; b < ((1<<(8 - TIMESCALE))-1); b++ ) {
      leds_off();
      if(led == 0) {
	Serial.print(0);
      }
    }
    if(led == 0) {
      Serial.println();
    }
  }
}

