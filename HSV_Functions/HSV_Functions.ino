/*
Notes:

Execution time - They all take roughly the same amount of time to execute - none
                 are particularly faster over 255 runs (at millis scales anyway
                 - all ~230ms at 16mhz)

------------------------------------------------------------------------

Code size - with Serial enabled:

c1: 4366 bytes
c2: 2844 bytes
c3: 3252 (with dim_curve, which is ~200 bytes)
c4: 2848 bytes

------------------------------------------------------------------------

Accuracy - numerically at the 3 primary colors + 3 secondary colors (i.e. full
           bright on R/G/B and Y/C/M):

c1: highly accurate - all on and all off at 60 degree intervals
c2: Kind of sloppy, never actually hits 0 brightness
c3: Identical to the floating point version
c4: Curious quirk - the scale seems to be 0-135, then a gap to 248-255.  Almost
    invisible on actual LEDs due to how they scale, but it makes fading
    trickier.

------------------------------------------------------------------------

Test Output

-- C1: Floating point
255 0 0
255 255 0
0 255 0
0 255 255
0 0 255
255 0 255
-- C2: Integer #1
255 1 1
254 255 1
1 255 4
1 250 255
8 1 255
255 1 246
-- C3: Integer #2 (brightness curve applied)
255 0 0
255 255 0
0 255 0
0 255 255
0 0 255
255 0 255
-- C4: Integer #3 (current)
255 0 0
127 128 0
0 254 1
0 126 129
2 0 253
130 0 125


*/

void setup() {
  Serial.begin(115200);
}

void loop() {

  int x;

  Serial.println("-- C1: Floating point");
  for(x = 0; x<360; x=x+60) { // Range is 0 .. 360, incrementing hue by 60 results in RYGCBM points
    c1(x,255,255);
  }  
  
  Serial.println("-- C2: Integer #1");
  for(x = 0; x<255; x=x+43) { // range is 0 .. 255, incremending by 42/43 results in the RYGCBM points
    c2(x,255,128);
  }

  Serial.println("-- C3: Integer #2 (brightness curve applied)");
  for(x = 0; x<360; x=x+60) { // Range is 0 .. 360 again
    c3(x,255,255);
  }

  Serial.println("-- C4: Integer #3 (current)");
  for(x = 0; x<764; x=x+128) { // range is 0 .. 764, incrementing by 127/128 results in RYGCBM points.
    c4(x,128,255);
  }

  delay(500000);
}

/* inputs:
hue 0-764
sat 0-128
bri 0-255
*/


void c4(int16_t hue, int16_t sat, int16_t bri) {
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

  int16_t r = (uint16_t)((bri + 1) * red_val) >> 8;
  int16_t g = (uint16_t)((bri + 1) * green_val) >> 8;
  int16_t b = (uint16_t)((bri + 1) * blue_val) >> 8;

  Serial.print(r); Serial.print(" ");
  Serial.print(g); Serial.print(" ");
  Serial.print(b); Serial.println();
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

  /* convert hue, saturation and brightness ( HSB/HSV ) to RGB
     The dim_curve is used only on brightness/value and on saturation (inverted).
     This looks the most natural.      
  */

// from http://www.kasperkamperman.com/blog/arduino/arduino-programming-hsb-to-rgb/

// Uses up 3252 bytes of flash, about 200 of which is the dim curve.

void c3(int hue, int sat, int val) { 

  val = dim_curve[val];
  sat = 255-dim_curve[255-sat];

  int r;
  int g;
  int b;
  int base;

  if (sat == 0) { // Acromatic color (gray). Hue doesn't mind.
    r=val;
    g=val;
    b=val;  
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
  Serial.print(r); Serial.print(" ");
  Serial.print(g); Serial.print(" ");
  Serial.print(b); Serial.println();
}

/* hue: 0 ... 255
   sat: 0 ... 255
   lum: 0 ... 255
   
In practice: 
  As saturation drops from 255, colors become muted
  As luminance rises, colors brighten up, but eventually the color gets washed out in 255,255,255.  Max color saturation is at 128 (255,0,0 at hue 0)

This is the integer code, with the Serial lines it uses up 2844 bytes of flash.

Source: http://qscribble.blogspot.com/2008/06/integer-conversion-from-hsl-to-rgb.html (adapted)
*/

void c2(int hue, int sat, int lum) {
  unsigned long v;
  int red, green, blue;

  // Original formula, simplified below.
  //  v = (lum < 128) ? (lum * (256 + sat)) >> 8 : (((lum + sat) << 8) - lum * sat) >> 8;

  if(lum < 128) {
    v = 256+sat;
    v *= lum;
    v = v >> 8;
  }
  else {
    v = lum + sat;
    v = v << 8;
    v -= ((long)lum * (long)sat);
    v = v >> 8;
  }

  if (v <= 0) {
    red = green = blue = 0;
  } 
  else {
    int m;
    int sextant;
    int fract, vsf, mid1, mid2;
    
    m = lum + lum - v;
    hue *= 6;
    sextant = hue >> 8;
    fract = hue - (sextant << 8);
    vsf = v * fract * (v - m) / v >> 8;
    mid1 = m + vsf;
    mid2 = v - vsf;
    switch (sextant) {
    case 0: red = v; green = mid1; blue = m; break;
    case 1: red = mid2; green = v; blue = m; break;
    case 2: red = m; green = v; blue = mid1; break;
    case 3: red = m; green = mid2; blue = v; break;
    case 4: red = mid1; green = m; blue = v; break;
    case 5: red = v; green = m; blue = mid2; break;
    }
  }
  Serial.print(red); Serial.print(" ");
  Serial.print(green); Serial.print(" ");
  Serial.print(blue); Serial.println();
}


/*******************************************************************************
 * Function HSV2RGB
 * Description: Converts an HSV color value into its equivalen in the RGB color space.
 * Copyright 2010 by George Ruinelli
 * The code I used as a source is from http://www.cs.rit.edu/~ncs/color/t_convert.html
 * Parameters:
 * Notes:
 *   - r, g, b values are from 0..255
 *   - h = [0,360], s = [0,255], v = [0,255]
 *   - NB: if s == 0, then h = 0 (undefined)
 * note: floating point - brings in floating libs, bloats code significantly.
 * 4366 bytes 

 Source: http://www.ruinelli.ch/rgb-to-hsv (adapted)

 ******************************************************************************/
void c1(uint16_t hue, uint8_t sat, uint8_t val) {
  int i;
  float f, p, q, t, h, s, v;
  
  int red, green, blue;
  
  h=(int)hue;
  s=(int)sat;
  v=(int)val;
  
  s /=255; 
  
  if( s == 0 ) { // achromatic (grey)
    red = green = blue = v;
  }
  else {
    h /= 60;            // sector 0 to 5
    i = floor( h );
    f = h - i;            // factorial part of h
    p = (unsigned char)(v * ( 1 - s ));
    q = (unsigned char)(v * ( 1 - s * f ));
    t = (unsigned char)(v * ( 1 - s * ( 1 - f ) ));
    
    switch( i ) {
    case 0: red = v; green = t; blue = p; break;
    case 1: red = q; green = v; blue = p; break;
    case 2: red = p; green = v; blue = t; break;
    case 3: red = p; green = q; blue = v; break;
    case 4: red = t; green = p; blue = v; break;
    default: red = v; green = p; blue = q; break;
    }
  }
  Serial.print(red); Serial.print(" ");
  Serial.print(green); Serial.print(" ");
  Serial.print(blue); Serial.println();
}

