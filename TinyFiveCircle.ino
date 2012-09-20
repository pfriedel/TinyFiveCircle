// Tiny85 versioned - ATmega requires these defines to be redone as well as the
// DDRB/PORTB calls later on.

// Because I tend to forget - the number here is the Port B pin in use.  Pin 5 is PB0, Pin 6 is PB1, etc.
#define LINE_A 0 // PB0 / Pin 5 on ATtiny85 / Pin 14 on an ATmega328 (D8)
#define LINE_B 1 // PB1 / Pin 6 on ATtiny85 / Pin 15 on an ATmega328 (D9) 
#define LINE_C 2 // PB2 / Pin 7 on ATtiny85 / Pin 17 on an ATmega328 (D11)
#define LINE_D 3 // PB3 / Pin 2 on ATtiny85 / Pin 18 on an ATmega328 (D12)
#define LINE_E 4 // PB4 / Pin 3 on ATtiny85

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <EEPROM.h>

// How many modes do we want to go through?
#define MAX_MODE 3
// How long should I draw each color on each LED?
#define DRAW_TIME 5

#define BODS 7 // Location of the brown-out disable switch in the MCU Control Register
#define BODSE 2 // Location of the Brown-out disable switch enable bit in the MCU Control Register

#define time_multiplier 60000 // change the base for the time statement - right now it's 1 minute
#define short_run 30
#define medium_run 240 // 4 hours
#define long_run 480 // 8 hours

byte last_mode;

//#define F_CPU 16000000UL

uint8_t led_grid[15] = {
  000 , 000 , 000 , 000 , 000 , // R
  000 , 000 , 000 , 000 , 000 , // G
  000 , 000 , 000 , 000 , 000  // B
};

void setup() {
  // Try and set the random seed more randomly.  Alternate solutions involve using the eeprom and writing the last seed there.
  uint16_t seed=0;
  uint8_t count=32;
  while (--count) {
    seed = (seed<<1) | (analogRead(1)&1);
  }
  randomSeed(seed);

  // Read the last mode out of the eeprom.
  EEReadSettings();
  last_mode++;
  if(last_mode > MAX_MODE) {
    last_mode = 0;
  }

  // save whichever mode we're using now back to eeprom.
  EESaveSettings();
}

void loop() {
  // indicate which mode we're entering
  light_led(last_mode);
  delay(500);
  leds_off();
  delay(250);

  // and go into the modes

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
    BrightnessWalk(short_run);
    break;
  case 3:
    // One LED at a time, PWM up to full brightness and back down again.
    // 9-10mA depending on which LEDs are lit
    PrimaryColors(short_run);
    break;
  case 4:
    RandomColorRandomPosition(medium_run);
    break;
  case 5:
    HueWalk(medium_run);
    break;
  case 6:
    BrightnessWalk(medium_run);
    break;
  case 7:
    PrimaryColors(medium_run);
    break;
  case 8:
    RandomColorRandomPosition(long_run);
    break;
  case 9:
    HueWalk(long_run);
    break;
  case 10:
    BrightnessWalk(long_run);
    break;
  case 11:
    PrimaryColors(long_run);
    break;
  }
}

void SleepNow(void) {
  
  // I don't think this matters in my circuit, but it doesn't hurt either - my
  // meter can't actually read the low current mode it goes in to when BOD is
  // disabled.
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  digitalWrite(0, HIGH);
  digitalWrite(1, HIGH);
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);

  // attempt to re-enter the same mode that was running before I reset.
  last_mode--;

  // last_mode is type byte, so when it rolls below 0, it will become a Very
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
  
void RandomColorRandomPosition(uint16_t time) {
  while(1) {
    setLedColorHSV(random(5),random(360), 1, 1);
    draw_for_time(1000);
    if(millis() >= time*time_multiplier) { SleepNow(); }
  }
}

void HueWalk(uint16_t time) {
  //  uint8_t width = random(16,20);
  uint8_t width = 5;
  while(1) {
    for(uint16_t colorshift=0; colorshift<360; colorshift++) {
      for(uint8_t led = 0; led<5; led++) {
	uint16_t hue = ((led) * 360/(width)+colorshift)%360;
	setLedColorHSV(led,hue,1,1);
	draw_for_time(1);
	if(millis() >= time*time_multiplier) { SleepNow(); }
      }
    }
  }
}

void BrightnessWalk(uint16_t time) {
  uint16_t hue = random(360); // initial color
  uint8_t led_val[5] = {1,9,17,25,33}; // some initial distances
  bool led_dir[5] = {1,1,1,1}; // everything is initially going towards higher brightnesses
  while(1) {
    for(uint8_t led = 0; led<5; led++) {
      if(millis() >= time*time_multiplier) { SleepNow(); }
      setLedColorHSV(led,hue,1,led_val[led]*.01);
      draw_for_time(DRAW_TIME);
      // if the current value for the current LED is about to exceed the top or the bottom, invert that LED's direction
      if((led_val[led] >= 99) or (led_val[led] <= 0)) { 
	led_dir[led] = !led_dir[led]; 
	hue++; // actually increments hue by the number of LEDs (4) as each LED goes through 99 or 0, but 360 is a loooong way from here.
	if(hue >= 360) { 
	  hue = 0;
	}
      }
      if(led_dir[led] == 1) { 
	led_val[led]++; 
      }
      else { 
	led_val[led]--; 
      }
    }
  }
}

void PrimaryColors(uint16_t time) {
  uint8_t led_bright = 1;
  bool led_dir = 1;
  uint8_t led = 0;
  while(1) {
    if(millis() >= time*time_multiplier) { SleepNow(); }
    
    // flip the direction when the LED is at full brightness or no brightness.
    if((led_bright >= 100) or (led_bright <= 0)) { led_dir = !led_dir; } 
    
    // increment or decrement the brightness
    if(led_dir == 1) { led_bright++; }
    else { led_bright--; }
    
    // if the decrement will turn off the current LED, switch to the next LED
    if( led_bright <= 0 ) { led_grid[led] = 0; led++; }
    
    // And if that change pushes the current LED off the end of the spire, reset to the first LED.
    if( led >=15) { led = 0; }
    
    led_grid[led] = led_bright;
    draw_for_time(DRAW_TIME);
  }
}


// to-do: Convert this to integer mode.
void setLedColorHSV(uint8_t p, uint16_t h, float s, float v) {
  // Lightly adapted from http://eduardofv.com/read_post/179-Arduino-RGB-LED-HSV-Color-Wheel-
  //this is the algorithm to convert from HSV to RGB
  float r=0; 
  float g=0; 
  float b=0;

  uint8_t i=(int)floor(h/60.0);
  float f = h/60.0 - i;
  float pv = v * (1 - s);
  float qv = v * (1 - s*f);
  float tv = v * (1 - s * (1 - f));

  switch (i)
    {
    case 0: //red
      r = v;
      g = tv;
      b = pv;
      break;
    case 1: // green
      r = qv;
      g = v;
      b = pv;
      break;
    case 2: 
      r = pv;
      g = v;
      b = tv;
      break;
    case 3: // blue
      r = pv;
      g = qv;
      b = v;
      break;
    case 4:
      r = tv;
      g = pv;
      b = v;
      break;
    case 5: // mostly red (again)
      r = v;
      g = pv;
      b = qv;
      break;
    }

  set_led_rgb(p,constrain((int)100*r,0,100),constrain((int)100*g,0,100),constrain((int)100*b,0,100));
}

/* Args:
   position - 0-3, bottom to top
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
  uint8_t led, bright_val, b;
  // giving the loop a bit of breathing room seems to prevent the last LED from flickering.  Probably optimizes into oblivion anyway.
  for ( led=0; led<=15; led++ ) { 
    //software PWM
    bright_val = led_grid[led];
    for( b=0 ; b < bright_val ; b+=1)  { light_led(led); } //delay while on
    for( b=bright_val ; b<100 ; b+=1)  { leds_off(); } //delay while off
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
  
  if (detectBad) {
    last_mode = 1; // I prefer the rainbow effect.
  }
}

void EESaveSettings (void){
  //EEPROM.write(Addr, Value);
  
  // Careful if you use  this function: EEPROM has a limited number of write
  // cycles in its life.  Good for human-operated buttons, bad for automation.
  
  byte value = EEPROM.read(0);

  if(value != last_mode) {
    EEPROM.write(0, last_mode);
  }
}

