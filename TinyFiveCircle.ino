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

// integer HSV to RGB calculation
//#define MAX_HUE 1529 // non-normalized
//#define MAX_HUE 764 // normalized
#define MAX_HUE 360 

/* commentary: 

The _imperfect HSV to RGB code is buggy and shouldn't be use when brightness or
saturation varies.  It's fine with varying hues, though.  The fact that it
clings to the ceiling is sort of a feature, not a bug.

Future modes:

1. random color random position - existing mode

2. hue walk at constant brightness - existing mode

3. brightness walk at constant hue - existing mode

4. saturation walk is kind of obnoxious, but might be worth trying now (inverse
   of #3 - that one goes to black, this would go to white.)

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
  light_led(last_mode);
  delay(1000);
  leds_off();
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
  
void RandomColorRandomPosition(uint16_t time) {
  while(1) {
    setLedColorHSV(random(5),random(MAX_HUE), 255, 255);
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
	setLedColorHSV_imperfect(led,hue,255,255);
	draw_for_time(DRAW_TIME);
	if(millis() >= time*time_multiplier) { SleepNow(); }
      }
    }
  }
}

void BrightnessWalk(uint16_t time) {
#define BRT_SCALE 4 // how quickly brightnesses should increment.
  uint16_t hue = random(MAX_HUE); // initial color
  uint8_t led_val[5] = {5,13,21,29,37}; // some initial distances
  bool led_dir[5] = {1,1,1,1,1}; // everything is initially going towards higher brightnesses
  while(1) {
    if(millis() >= time*time_multiplier) { SleepNow(); }
    for(uint8_t led = 0; led<5; led++) {
      setLedColorHSV(led, hue, 255, led_val[led]);
      draw_for_time(DRAW_TIME);
      
      // if the current value for the current LED is about to exceed the top or the bottom, invert that LED's direction
      if((led_val[led] >= (255-BRT_SCALE)) or (led_val[led] <= (0+BRT_SCALE))) {
        led_dir[led] = !led_dir[led];
        hue++; // actually increments hue by the number of LEDs as each LED goes through the ceiling or the floor, but MAX_HUE is a loooong way from here.
        if(hue >= MAX_HUE) {
          hue = 0;
        }
      }
      if(led_dir[led] == 1) {
        led_val[led] += BRT_SCALE;
      }
      else {
        led_val[led] -= BRT_SCALE;
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


/* ---------------8-Bit-PWM--------------------------
hue: 0 to 1529
sat: 0 to 256 (0 to 255 with small inaccuracy)
bri: 0 to 255
all variables uint16_t

This is actually not quite perfect, since it jumps to uint16t in a nonlinear
fashion, but the errors mostly cancel each other out.  The upshot is that once a
color channel gets over halfway, it snaps to full brightness and stays there.
That spoils the color change a bit, but it's sort of a nice looking bug.

That said, it does horrible things when you're slewing around in brightness.

*/

void setLedColorHSV_imperfect(uint8_t p, int16_t hue, int16_t sat, int16_t bri) {
  int16_t red_val, green_val, blue_val;

  if(MAX_HUE == 360) {
    hue = map(hue,0,360,0,1529);
  }

  while (hue > 1529) hue -= 1530;
  while (hue < 0) hue += 1530;
  
  if (hue < 255) {
    red_val = 255;
    green_val = (65280 - sat * (255 - hue)) >> 8;
    blue_val = 255 - sat;
  }
  else if (hue < 510) {
    red_val = (65280 - sat * (hue - 255)) >> 8;
    green_val = 255;
    blue_val = 255 - sat;
  }
  else if (hue < 765) {
    red_val = 255 - sat;
    green_val = 255;
    blue_val = (65280 - sat * (765 - hue)) >> 8;
  }
  else if (hue < 1020) {
    red_val = 255 - sat;
    green_val = (65280 - sat * (hue - 765)) >> 8;
    blue_val = 255;
  }
  else if (hue < 1275) {
    red_val = (65280 - sat * (1275 - hue)) >> 8;
    green_val = 255 - sat;
    blue_val = 255;
  }
  else {
    red_val = 255;
    green_val = 255 - sat;
    blue_val = (65280 - sat * (hue - 1275)) >> 8;
  }
  
  // ranges from 0-127,65408-65535
  uint16_t red = ((bri + 1) * red_val) >> 8;
  uint16_t green = ((bri + 1) * green_val) >> 8;
  uint16_t blue = ((bri + 1) * blue_val) >> 8;

  set_led_rgb(p,map(red,0,255,0,100),map(green,0,255,0,100),map(blue,0,255,0,100));
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

  // manage the case where I set to floating calculations but don't change the
  // routine.
  if(MAX_HUE == 360) {
    hue = map(hue,0,360,0,764);
  }

  if(MAX_SAT == 255) {
    sat = map(sat,0,255,0,128);
  }

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
  
  set_led_rgb(p,map(red,0,255,0,100),map(green,0,255,0,100),map(blue,0,255,0,100));
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

