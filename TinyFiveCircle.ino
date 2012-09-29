// Tiny85 versioned - ATmega requires these defines to be redone as well as the
// DDRB/PORTB calls later on.

/* commentary: 

Future modes:

1. random color random position - existing mode

2. hue walk at constant brightness - existing mode

3. brightness walk at constant hue - existing mode

4. saturation walk is kind of obnoxious - existing mode

5. "Rain" - start at LED1 with a random color.  Advance color to LED2 and pick a
   new LED1.  Rotate around the ring.

6. non-equidistant hue walks - instead of 0 72 144 216 288 incrementing,
   something like...  0 30 130 230 330 rotating around the ring.  The hue += and
   -= parts might help here?

7. Color morphing: Pick 5 random colors.  Transition from color on LED1 to LED2
   and around the ring.  Could be seen as a random variant of #2.

8. Larson scanner in primaries - Doable with 5 LEDs? (probably, I've seen
   larsons with 2 actively lit LEDs in 4 LEDs before.)

9. Pick a color and sweep brightness around the circle (red! 0-100
   larson-style), then green 0-100, the blue 0-100, getting new color on while
   old color is fading out.

*/

// Because I tend to forget - the number here is the Port B pin in use.  Pin 5 is PB0, Pin 6 is PB1, etc.
#define LINE_A 0 // PB0 / Pin 5 on ATtiny85 / Pin 14 on an ATmega328 (D8)
#define LINE_B 1 // PB1 / Pin 6 on ATtiny85 / Pin 15 on an ATmega328 (D9) 
#define LINE_C 2 // PB2 / Pin 7 on ATtiny85 / Pin 17 on an ATmega328 (D11)
#define LINE_D 3 // PB3 / Pin 2 on ATtiny85 / Pin 18 on an ATmega328 (D12)
#define LINE_E 4 // PB4 / Pin 3 on ATtiny85

#include <avr/sleep.h>
#include <avr/power.h>
#include <EEPROM.h>

// How many modes do we want to go through?
#define MAX_MODE 6
// How long should I draw each color on each LED?
#define DRAW_TIME 5

// Location of the brown-out disable switch in the MCU Control Register (MCUCR)
#define BODS 7 
// Location of the Brown-out disable switch enable bit in the MCU Control Register (MCUCR)
#define BODSE 2 

// The base unit for the time comparison. 1000=1s, 10000=10s, 60000=1m, etc.
#define time_multiplier 60000 // change the base for the time statement - right now it's 1 minute
// How long should it run before going to sleep?
#define short_run 30

#define MAX_HUE 764 // normalized

byte __attribute__ ((section (".noinit"))) last_mode;

uint8_t led_grid[15] = {
  000 , 000 , 000 , 000 , 000 , // R
  000 , 000 , 000 , 000 , 000 , // G
  000 , 000 , 000 , 000 , 000  // B
};

void setup() {
  if(bit_is_set(MCUSR, PORF)) { // Power was just established!
    MCUSR = 0; // clear MCUSR
    EEReadSettings(); // read the last mode out of eeprom
  } 
  else if(bit_is_set(MCUSR, EXTRF)) { // we're coming out of a reset condition.
    MCUSR = 0; // clear MCUSR
    last_mode++; // advance mode

    if(last_mode > MAX_MODE) {
      last_mode = 0; // reset mode
    }
  }

  // Try and set the random seed more randomly.  Alternate solutions involve
  // using the eeprom and writing the last seed there.
  uint16_t seed=0;
  uint8_t count=32;
  while (--count) {
    seed = (seed<<1) | (analogRead(1)&1);
  }
  randomSeed(seed);
}

void loop() {
  // indicate which mode we're entering
  led_grid[last_mode] = 500;
  draw_for_time(1000);
  led_grid[last_mode] = 0;
  delay(250);

  // If EXTRF hasn't been asserted yet, save the mode
  EESaveSettings();

  // go into the modes
  switch(last_mode) {
  case 0:
    // random colors on random LEDs.
    // ~15-120mA depending on which LEDs are lit.
    RandomColorRandomPosition(short_run);
    break;
  case 1: 
    // downward flowing rainbow inspired from the shiftPWM library example.
    // this walks through hue space at a constant saturation and brightness
    // 14-18mA depending on which LEDs are lit.
    HueWalk(short_run);
    break;
  case 2: 
    // Walk through brightness values across all hues.
    // this walks through brightnesses, slowly shifting hues.
    // 10-14mA depending on which LEDs are lit
    SBWalk(short_run,1,1);
    break;
  case 3:
    SBWalk(short_run,14,1);
    break;
  case 4:
    // One LED at a time, PWM up to full brightness and back down again.
    // 9-10mA depending on which LEDs are lit
    PrimaryColors(short_run);
    break;
  case 5:
    SBWalk(short_run,14,2);
    break;
  case 6:
    LarsonScanner(short_run);
    break;
    
  }
}

void SleepNow(void) {
  // decrement last_mode, so the EXTRF increment sets it back to where it was.
  // note: this actually doesn't work that great in cases where power is removed after the MCU goes to sleep.
  // On the other hand, that's an edge condition that I'm not going to worry about too much.
  last_mode--;

  // mode is type byte, so when it rolls below 0, it will become a Very
  // Large Number compared to MAX_MODE.  Set it to MAX_MODE and the setup
  // routine will jump it up and down by one.
  if(last_mode > MAX_MODE) { last_mode = MAX_MODE; }

  EESaveSettings();

  // Important power management stuff follows
  ADCSRA &= ~(1<<ADEN); // turn off the ADC
  ACSR |= _BV(ACD);     // disable the analog comparator
  MCUCR |= _BV(BODS) | _BV(BODSE); // turn off the brown-out detector

  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // do a complete power down
  sleep_enable(); // enable sleep mode
  sei();  // allow interrupts to end sleep mode
  sleep_cpu(); // go to sleep
  delay(500);
  sleep_disable(); // disable sleep mode for safety
}

void LarsonScanner(uint16_t time) {
  int8_t direction = 1;
  uint8_t active = 1;
  uint8_t maxbright = 255;
  uint8_t decayfactor = 10;
  uint8_t width = 5;
  uint8_t virtualwidth = 3;
  int array[width+(2*virtualwidth)];

  // blank out the array...
  for(uint8_t x = 0; x<width+(2*virtualwidth); x++) {
    array[x] = 0;
  }

  while(1) {
    if(millis() >= time*time_multiplier) { SleepNow(); }
    //    Serial.print(active); Serial.print(" "); Serial.print(direction); Serial.print("\t");

    // If the active element will go outside of the realm of the array
    if((active >= (width+(2*virtualwidth)-1)) // high side match
       || (active < 1 )) {  // low side match
      if(direction == 1) 
	direction = -1;
      else if(direction == -1) 
	direction = 1;
    }

    // change which member is the active member
    if(direction == 1)
      active++;
    else 
      active--;

    // pin the currently active member to the maximum brightness
    array[active] = maxbright;

    // dim the other members of the array

    // start 1 element off the current.  If I start at 0, I would be
    // dimming the currently active element.
    for(uint8_t x = 1; x<width+2; x++) {
      //      Serial.print(x); Serial.print("\t");
      // constrain the edits to real array elements.
      if(active - (direction * x) >= 0)
        array[active - (direction * x)] -= (maxbright / width + decayfactor);

      // clear out the subzero dross.
      if(array[active - (direction *x)] < 0)
        array[active - (direction * x)] = 0;
    }

    uint8_t displaypos = 0;
    for(uint8_t x = virtualwidth; x < width + virtualwidth; x++) {
      //      Serial.print(array[x]); Serial.print("\t");
      led_grid[displaypos] = array[x];
      displaypos++;
    }
    //    Serial.println("");
    draw_for_time(30);
  }
}
  
void RandomColorRandomPosition(uint16_t time) {
  // preload all the LEDs with a color
  for(int x = 0; x<5; x++) {
    setLedColorHSV(x,random(MAX_HUE), 128, 255);
  }
  // and start blinking new ones in once a second.
  while(1) {
    setLedColorHSV(random(5),random(MAX_HUE), 128, 255);
    draw_for_time(1000);
    if(millis() >= time*time_multiplier) { SleepNow(); }
  }
}

void HueWalk(uint16_t time) {
  uint8_t width = 20;
  //  uint8_t width = 5;
  while(1) {
    for(uint16_t colorshift=0; colorshift<MAX_HUE; colorshift++) {
      for(uint8_t led = 0; led<5; led++) {
	uint16_t hue = ((led) * MAX_HUE/(width)+colorshift)%MAX_HUE;
	setLedColorHSV(led,hue,128,255);
	draw_for_time(2);
	if(millis() >= time*time_multiplier) { SleepNow(); }
      }
    }
  }
}

/*
time: How long it should run
jump: How much should hue increment every time an LED flips direction?
mode: 
  1 = walk through brightnesses at full saturation, 
  2 = walk through saturations at full brightness.
1 tends towards colors & black, 2 tends towards colors & white.
*/
void SBWalk(uint16_t time, uint8_t jump, uint8_t mode) {
  uint8_t scale_max, delta;
  uint16_t hue = random(MAX_HUE); // initial color
  uint8_t led_val[5] = {5,13,21,29,37}; // some initial distances
  bool led_dir[5] = {1,1,1,1,1}; // everything is initially going towards higher brightnesses

  // set the appropriate threshholds for the mode - brightness caps at 255 while
  // saturation caps at 128.
  if(mode == 1) { 
    scale_max = 255; 
    delta = 2; 
  }
  else if (mode == 2) { 
    scale_max = 128; 
    delta = 1; 
  }

  while(1) {
    if(millis() >= time*time_multiplier) 
      SleepNow();
    for(uint8_t led = 0; led<5; led++) {
      if(mode == 1)
	setLedColorHSV(led, hue, 128, led_val[led]);
      else if (mode == 2)
	setLedColorHSV(led, hue, led_val[led], 255);
      draw_for_time(2);
      
      // if the current value for the current LED is about to exceed the top or the bottom, invert that LED's direction
      if((led_val[led] >= (scale_max-delta)) or (led_val[led] <= (0+delta))) {
	led_dir[led] = !led_dir[led];
	hue += jump;
	if(hue > MAX_HUE)
	  hue = 0;
      }
      if(led_dir[led] == 1)
        led_val[led] += delta;
      else 
        led_val[led] -= delta;
    }
  }
}

void PrimaryColors(uint16_t time) {
  uint8_t led_bright = 1;
  bool led_dir = 1;
  uint8_t led = 0;
  while(1) {
    if(millis() >= time*time_multiplier)
      SleepNow();
    
    // flip the direction when the LED is at full brightness or no brightness.
    if((led_bright >= 255) or (led_bright <= 0))
      led_dir = !led_dir;
    
    // increment or decrement the brightness
    if(led_dir == 1)
      led_bright++;
    else
      led_bright--;
    
    // if the decrement will turn off the current LED, switch to the next LED
    if( led_bright <= 0 ) { 
      led_grid[led] = 0; 
      led++; 
    }
    
    // And if that change pushes the current LED off the end of the spire, reset to the first LED.
    if( led >=15) 
      led = 0; 

    // push the change out to the array.
    led_grid[led] = led_bright;
    draw_for_time(DRAW_TIME);
  }
}


// from http://mbed.org/forum/mbed/topic/1251/?page=1#comment-6216
//4056 bytes on this version.
/*-------8-Bit-PWM-|-Light-Emission-normalized------
hue: 0 to 764
sat: 0 to 128 (0 to 127 with small inaccuracy)
bri: 0 to 255
all variables int16_t
*/
void setLedColorHSV(uint8_t p, int16_t hue, int16_t sat, int16_t bri) {
  int16_t red_val, green_val, blue_val;

  while (hue > 764) hue -= 765;
  while (hue < 0) hue += 765;
  
  if (hue < 255) {
    red_val = (10880 - sat * (hue - 170)) >> 7;
    green_val = (10880 - sat * (85 - hue)) >> 7;
    blue_val = (10880 - sat * 85) >> 7;
  }
  else if (hue < 510) {
    red_val = (10880 - sat * 85) >> 7;
    green_val = (10880 - sat * (hue - 425)) >> 7;
    blue_val = (10880 - sat * (340 - hue)) >> 7;
  }
  else {
    red_val = (10880 - sat * (595 - hue)) >> 7;
    green_val = (10880 - sat * 85) >> 7;
    blue_val = (10880 - sat * (hue - 680)) >> 7;
  }
  
  int16_t red = (uint16_t)((bri + 1) * red_val) >> 8;
  int16_t green = (uint16_t)((bri + 1) * green_val) >> 8;
  int16_t blue = (uint16_t)((bri + 1) * blue_val) >> 8;

  // remap that to a 0-100 range.  The map is 1 over the input and outputs to
  // allow for the full range to be handled.

//  red = constrain(map(red,0,256,0,101)
//		  , 0
//		  , 100);
//  green = constrain(map(green,0,256,0,101)
//		    , 0
//		    , 100);
//  blue = constrain(map(blue,0,256,0,101)
//		   , 0 
//		   , 100);
  
  set_led_rgb(p,red,green,blue);
}

/* Args:
   position - 0-4
   red value - 0-100
   green value - 0-100
   blue value - 0-100
*/
void set_led_rgb (uint8_t p, uint8_t r, uint8_t g, uint8_t b) {
  // red usually seems to need to be attenuated a bit.
  led_grid[p] = r;
  led_grid[p+5] = g;
  led_grid[p+10] = b;
}

// runs draw_frame a supplied number of times.
void draw_for_time(uint16_t time) {
  for(uint16_t f = 0; f<time; f++) { draw_frame(); }
}

const uint8_t led_dir[15] = {
  ( 1<<LINE_E | 1<<LINE_D ), // 1 r
  ( 1<<LINE_D | 1<<LINE_C ), // 2 r
  ( 1<<LINE_C | 1<<LINE_B ), // 3 r
  ( 1<<LINE_B | 1<<LINE_A ), // 4 r
  ( 1<<LINE_A | 1<<LINE_E ), // 5 r

  ( 1<<LINE_E | 1<<LINE_A ), // 1 g
  ( 1<<LINE_D | 1<<LINE_E ), // 2 g
  ( 1<<LINE_C | 1<<LINE_D ), // 3 g
  ( 1<<LINE_B | 1<<LINE_C ), // 4 g
  ( 1<<LINE_A | 1<<LINE_B ), // 5 g

  ( 1<<LINE_E | 1<<LINE_B ), // 1 b
  ( 1<<LINE_D | 1<<LINE_A ), // 2 b
  ( 1<<LINE_C | 1<<LINE_E ), // 3 b
  ( 1<<LINE_B | 1<<LINE_D ), // 4 b
  ( 1<<LINE_A | 1<<LINE_C ), // 5 b
};

//PORTB output config for each LED (1 = High, 0 = Low)
const uint8_t led_out[15] = {
  ( 1<<LINE_E ), // 1 r
  ( 1<<LINE_D ), // 2 r
  ( 1<<LINE_C ), // 3 r
  ( 1<<LINE_B ), // 4 r
  ( 1<<LINE_A ), // 5 r
  
  ( 1<<LINE_E ), // 1 g
  ( 1<<LINE_D ), // 2 g
  ( 1<<LINE_C ), // 3 g
  ( 1<<LINE_B ), // 4 g
  ( 1<<LINE_A ), // 5 g
  
  ( 1<<LINE_E ), // 1 b
  ( 1<<LINE_D ), // 2 b
  ( 1<<LINE_C ), // 3 b
  ( 1<<LINE_B ), // 4 b
  ( 1<<LINE_A ), // 5 b
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
    //software PWM
    // Light the LED in proportion to the value in the led_grid array
    for( b=0; b<led_grid[led]; b+=2 )
      light_led(led);
    // and turn the LED off for the amount of time in the led_grid array beneath 200
    for( b=led_grid[led]; b<200; b+=2 )
      leds_off();
  }
}

void EEReadSettings (void) {  // TODO: Detect ANY bad values, not just 255.
  byte detectBad = 0;
  byte value = 255;
  
  value = EEPROM.read(0);
  if (value > MAX_MODE)
    detectBad = 1;
  else
    last_mode = value;  // MainBright has maximum possible value of 8.
  
  if (detectBad)
    last_mode = 1; // I prefer the rainbow effect.
}

void EESaveSettings (void){
  //EEPROM.write(Addr, Value);
  
  // Careful if you use  this function: EEPROM has a limited number of write
  // cycles in its life.  Good for human-operated buttons, bad for automation.
  
  byte value = EEPROM.read(0);

  if(value != last_mode)
    EEPROM.write(0, last_mode);
}

