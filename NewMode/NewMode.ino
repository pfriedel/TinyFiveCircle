// Because I tend to forget - the number here is the Port B pin in use.  Pin 5 is PB0, Pin 6 is PB1, etc.
#define LINE_A 0 // PB0 / Pin 5 on ATtiny85 / Pin 14 on an ATmega328 (D8)
#define LINE_B 1 // PB1 / Pin 6 on ATtiny85 / Pin 15 on an ATmega328 (D9)
#define LINE_C 2 // PB2 / Pin 7 on ATtiny85 / Pin 17 on an ATmega328 (D11)
#define LINE_D 3 // PB3 / Pin 2 on ATtiny85 / Pin 18 on an ATmega328 (D12)
#define LINE_E 4 // PB4 / Pin 3 on ATtiny85

#include <avr/power.h>
#include <EEPROM.h>

// How long should I draw each color on each LED?
#define DRAW_TIME 5

// The base unit for the time comparison. 1000=1s, 10000=10s, 60000=1m, etc.
//#define time_multiplier 1000 
#define time_multiplier 60000
// How many time_multiplier intervals should run before going to sleep?
#define run_time 240

#define MAX_HUE 360 // normalized

// How bit-crushed do you want the bit depth to be?  1 << DEPTH is how
// quickly it goes through the LED softpwm scale.

// 1 means 128 shades per color.  3 is 32 colors while 6 is 4 shades per color.
// it sort of scales the time drawing routine, but not well.

// It also affects how bright it it (less time spent drawing nothing) as well as
// the POV flickering rate. Higher numbers are both brighter and less flicker-y.

// the build uses 1 for the PTH versions and 3 for the SMD displays.

#define DEPTH 3

#define HUE_DEBUG 0
#define DRAW_DEBUG 0
#define MODE_DEBUG 0

uint8_t led_grid[15] = {
  000 , 000 , 000 , 000 , 000 , // R  0  1  2  3  4
  000 , 000 , 000 , 000 , 000 , // G  5  6  7  8  9
  000 , 000 , 000 , 000 , 000   // B 10 11 12 13 14
};

void setup() {
  if(( HUE_DEBUG == 1 ) || ( DRAW_DEBUG == 1 ) || ( MODE_DEBUG == 1 )) {
    Serial.begin(115200);
  }

  // Try and set the random seed more randomly.  Alternate solutions involve
  // using the eeprom and writing the last seed there

  uint16_t seed=0;
  uint8_t count=32;
  while (--count) {
    seed = (seed<<1) | (analogRead(A1)&1);
  }
  randomSeed(seed);
}

void loop() {
  //  RandHueWalk(run_time,millis()); // works great
  //  BiColorWalk(run_time, millis(), 0, 120); // red to green, works great
  //  BiColorWalk(run_time, millis(), 120, 240); // green to blue, works great
  //  BiColorWalk(run_time, millis(), 240, 359); // blue to red, works great - setting endhue to 0 makes it a very wiiiide walk.
  uint16_t starthue = random(360);
  uint16_t endhue = starthue + 120;
  BiColorWalk(run_time, millis(), starthue, endhue); // bounce between two randoms, seems to work.
}

// definitely a work in progress. Maybe change the led_dir to a velocity within
// that color channel, so red rotates 3 times faster than blue, and green rotates
// twice as fast as blue.  At every loop, change the velocity by +_ rand within
// reasonable range.

void MultiPrimary(uint16_t time, uint32_t start_time) {
  led_grid[0] = 32; 
  led_grid[1] = 64; 
  led_grid[2] = 127; 
  led_grid[3] = 128; 
  led_grid[4] = 0;
  led_grid[5] = 32; 
  led_grid[6] = 64; 
  led_grid[7] = 127; 
  led_grid[8] = 128; 
  led_grid[9] = 0;
  led_grid[10] = 32; 
  led_grid[11] = 64; 
  led_grid[12] = 127; 
  led_grid[13] = 128; 
  led_grid[14] = 0;
  
  int8_t led_dir[] = {-1,-1,-1,1,0,
		      0,-1,-1,-1,1,
		      1,0,-1,-1,1};
  
  
  while(1) { 
    for(uint8_t x = 0; x<15; x++) {
      led_grid[x] = led_grid[x] + led_dir[x];
      
      if((led_grid[x] == 254) && (led_dir[x] == 1)) { 
	led_dir[x] = -1; 
      }
      
      if((led_grid[x] == 0) && (led_dir[x] != 0)) { 
	led_dir[x] = 0; 
	if(x < 13) { led_dir[x+2] = 1; }
	if(x == 13) { led_dir[0] = 1; } 
	if(x == 14) { led_dir[1] = 1; }
      }
      
    }
    draw_for_time(DRAW_TIME);
  }
}

// rotate 5 random color distances around the ring. Works quite nicely.  Leans heavily on randomseed being random.
void RandHueWalk(uint16_t time, uint32_t start_time) {
  uint16_t ledhue[] = {random(360), random(360), random(360), random(360), random(360)};
  while(1) {
    
    if(millis() >= (start_time + (time * time_multiplier))) { break; }
    
    for(uint8_t led = 0; led<5; led++) {
      
      uint16_t hue = ledhue[led]--;
      
      if(hue == 0) { ledhue[led] = 359; }
      setLedColorHSV(led, hue, 255, 255);
    }
    draw_for_time(DRAW_TIME); // this gets called 5 times, once per LED.
  }
}

/* Additional ideas:
1) Consider hitting endhue, then desaturating to white and ending up at the starthue.
2) Maybe put all 5 LEDs on slightly different hues within the same start/endhue progression, then larson the colors back and forth?
*/

void BiColorWalk(uint16_t time, uint32_t start_time, uint16_t starthue, uint16_t endhue) {
  uint16_t curhue = starthue;
  int8_t curdir = 1;
  while(1) {
    if(millis() >= (start_time + (time * time_multiplier))) { break; }

    curhue += curdir;
    if(curhue == endhue) { curdir = -1; }
    if(curhue == starthue) { curdir = 1; }

    for(uint8_t led = 0; led < 5; led++) {
      setLedColorHSV(led, curhue, 255, 255);
    }
    draw_for_time(DRAW_TIME * 5);
  }
}
  
  

/*
Inputs:
  p : LED to set
  immediate : Whether the change should go to led_grid or led_grid_next
  hue : 0-359 - color: 0 = red, 120 = green, 240 = blue
  sat : 0-255 - how saturated should it be? 0=white, 255=full color
  val : 0-255 - how bright should it be? 0=off, 255=full bright
*/
void setLedColorHSV(uint8_t p, int16_t hue, int16_t sat, int16_t val) {
  while (hue > 359) hue -= 360;
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

  r = r>>DEPTH;
  g = g>>DEPTH;
  b = b>>DEPTH;

  // Output the hues on each LED...
  if(HUE_DEBUG == 1) {
    Serial.print(p);
    Serial.print("=");
    Serial.print(hue); 
    Serial.print(" ");
    if(p == 4) {
      Serial.println();
    }
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
  for(uint16_t f = 0; f<time * (1 << (DEPTH-1)); f++) { 
    draw_frame();   
  }
}

// Anode | cathode
const uint8_t led_dir[15] = {
  ( 1<<LINE_B | 1<<LINE_A ), // 1 r
  ( 1<<LINE_A | 1<<LINE_E ), // 2 r
  ( 1<<LINE_E | 1<<LINE_D ), // 3 r
  ( 1<<LINE_D | 1<<LINE_C ), // 4 r
  ( 1<<LINE_C | 1<<LINE_B ), // 5 r

  ( 1<<LINE_B | 1<<LINE_C ), // 1 g
  ( 1<<LINE_A | 1<<LINE_B ), // 2 g
  ( 1<<LINE_E | 1<<LINE_A ), // 3 g
  ( 1<<LINE_D | 1<<LINE_E ), // 4 g
  ( 1<<LINE_C | 1<<LINE_D ), // 5 g

  ( 1<<LINE_B | 1<<LINE_D ), // 1 b
  ( 1<<LINE_A | 1<<LINE_C ), // 2 b
  ( 1<<LINE_E | 1<<LINE_B ), // 3 b
  ( 1<<LINE_D | 1<<LINE_A ), // 4 b
  ( 1<<LINE_C | 1<<LINE_E ), // 5 b
};

//PORTB output config for each LED (1 = High, 0 = Low)
const uint8_t led_out[15] = {
  ( 1<<LINE_B ), // 1
  ( 1<<LINE_A ), // 2
  ( 1<<LINE_E ), // 3
  ( 1<<LINE_D ), // 4
  ( 1<<LINE_C ), // 5
  
  ( 1<<LINE_B ), // 1
  ( 1<<LINE_A ), // 2
  ( 1<<LINE_E ), // 3
  ( 1<<LINE_D ), // 4
  ( 1<<LINE_C ), // 5
  
  ( 1<<LINE_B ), // 1
  ( 1<<LINE_A ), // 2
  ( 1<<LINE_E ), // 3
  ( 1<<LINE_D ), // 4
  ( 1<<LINE_C ), // 5
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

  for ( led=0; led<15; led++ ) { 
    if(DRAW_DEBUG == 1) {
      Serial.print(led); Serial.print(":"); Serial.print(led_grid[led]); Serial.print(" ");
      if((led == 4) || (led == 9) || (led == 14)) {
	Serial.println();
      }
    }

    for( b=0; b<led_grid[led]; b++) {
      light_led(led);
    }
    // and turn the LEDs off for the amount of time in the led_grid array between LED brightness and 255.
    // BUG alert - at timescale 3, 
    // input "68" is 33 total cycles, 9 lit and 24 dark, 
    // input "63" is 32 total cycles, 8 lit and 24 dark.                                                                              
    for( b=led_grid[led]; b < ((1<<(8 - DEPTH))-1); b++ ) {
      leds_off();
    }
  }
}


void quickSort(int arr[], int left, int right) {
  int i = left, j = right;
  int tmp;
  int pivot = arr[(left + right) / 2];
  
  /* partition */
  while (i <= j) {
    while (arr[i] < pivot)
      i++;
    while (arr[j] > pivot)
      j--;
    if (i <= j) {
      tmp = arr[i];
      arr[i] = arr[j];
      arr[j] = tmp;
      i++;
      j--;
    }
  }
  /* recursion */
  if (left < j)
    quickSort(arr, left, j);
  if (i < right)
    quickSort(arr, i, right);
}
