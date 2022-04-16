//-----------------------------------------------------------------------------
// Copyright 2021 Peter Balch
// displays an ECG
//-----------------------------------------------------------------------------

#include <Arduino.h>
#include <SPI.h>
#include "SimpleILI9341.h"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------

//#define bHasFakeECG

// get register bit - faster: doesn't turn it into 0/1
#ifndef getBit
#define getBit(sfr, bit) (_SFR_BYTE(sfr) & _BV(bit))
#endif

//-----------------------------------------------------------------------------
// Global Constants and Typedefs
//-----------------------------------------------------------------------------

const int TFT_WIDTH = 320;
const int TFT_HEIGHT = 240;
  
// pins
const int ECG_IN = A2;
#ifdef bHasFakeECG
const int FAKE_OUT = 3;
#endif
const int BUTTON_IN = A5;
const int LO_P_IN = A0;
const int LO_N_IN = A1;

// Display pins
const int TFT_CS    = 10;
const int TFT_CD    = 8;
const int TFT_RST   = 9;

const int SamplePeriod = 5; // mSec
const int PoincareScale = 10; 
const int PoincareLeft = 40; 
const int PoincareBottom = TFT_HEIGHT-12; 

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

uint8_t Trace[TFT_WIDTH];
uint16_t DisplayRepeat = TFT_WIDTH;
uint8_t ignoreBeats = 0;

enum TMode { mdLargeECG, mdSmallECG, mdPoincare } mode = mdLargeECG;

//-----------------------------------------------------------------------------
// testADCfast
//-----------------------------------------------------------------------------
int getADCfast(void)
{
  while (!getBit(ADCSRA, ADIF)) ; // wait for ADC
  int i = ADCL;
  i += ADCH << 8;
  bitSet(ADCSRA, ADIF); // clear the flag
  bitSet(ADCSRA, ADSC); // start ADC conversion

  return i;
}

//-----------------------------------------------------------------------------
// DrawGrid
//-----------------------------------------------------------------------------
void DrawGrid(void)
{
  switch (mode) {
    case mdLargeECG: DrawGridLarge(); break;
    //case mdSmallECG: DrawGridSmall(); break;  
    case mdPoincare: DrawGridPoincare(); break;
  }
  DisplayRepeat = TFT_WIDTH;
  DrawPoincare(0);
  ignoreBeats = 2;
}

//-----------------------------------------------------------------------------
// DrawGridLarge
//-----------------------------------------------------------------------------
void DrawGridLarge(void)
{
  int i;
  ClearDisplay(TFT_BLACK);
  for (i=0; i < TFT_HEIGHT; i++)
    if (i % 8 == 0)
      DrawHLine(0, i, TFT_WIDTH, TFT_MAROON); 
  for (i=0; i < TFT_WIDTH; i++)
    if (i*25 % (1000/SamplePeriod) == 0)
      DrawVLine(i, 0, TFT_HEIGHT, TFT_MAROON); 
      
  for (i=0; i < TFT_HEIGHT; i++)
    if (i % 40 == 0)
      DrawHLine(0, i, TFT_WIDTH, TFT_RED); 
  for (i=0; i < TFT_WIDTH; i++)
    if (i*5 % (1000/SamplePeriod) == 0)
      DrawVLine(i, 0, TFT_HEIGHT, TFT_RED); 
}
  
//-----------------------------------------------------------------------------
// DrawGridSmall
//-----------------------------------------------------------------------------
void DrawGridSmall(void)
{
  int i;
  ClearDisplay(TFT_BLACK);
  for (i=0; i < TFT_WIDTH; i++)
    if (i*4*5 % (1000/SamplePeriod) == 0)
      DrawVLine(i, 0, TFT_HEIGHT, TFT_RED);
}
  
//-----------------------------------------------------------------------------
// DrawPoincareLine
//-----------------------------------------------------------------------------
void DrawPoincareLine(int x1, int y1, int x2, int y2, uint16_t color)
{
  DrawLine(
    constrain(PoincareLeft+x1/PoincareScale,0,TFT_WIDTH-1), 
    constrain(PoincareBottom-y1/PoincareScale,0,TFT_HEIGHT-1), 
    constrain(PoincareLeft+x2/PoincareScale,0,TFT_WIDTH-1), 
    constrain(PoincareBottom-y2/PoincareScale,0,TFT_HEIGHT-1), 
    color);     
}
  
//-----------------------------------------------------------------------------
// DrawPoincareFrame
//-----------------------------------------------------------------------------
void DrawPoincareFrame(int x1, int y1, int x2, int y2, uint16_t color)
{
  DrawPoincareLine(x1,y1,x2,y1,color);
  DrawPoincareLine(x2,y1,x2,y2,color);
  DrawPoincareLine(x2,y2,x1,y2,color);
  DrawPoincareLine(x1,y2,x1,y1,color);
}
  
//-----------------------------------------------------------------------------
// DrawGridPoincare
//-----------------------------------------------------------------------------
void DrawGridPoincare(void)
{
  #define TFT_DARKDARKGRAY    RGB(64,64,64)
  const uint8_t f[4] = { 30, 60, 100, 160 };
  int i,b;
  
  ClearDisplay(TFT_BLACK);

  for (i=0; i < sizeof(f); i++) {
    b = 60000 / f[i] / PoincareScale;
    DrawVLine(PoincareLeft+b, 0, PoincareBottom, TFT_DARKDARKGRAY);   
    DrawHLine(PoincareLeft, PoincareBottom-b, TFT_WIDTH, TFT_DARKDARKGRAY);   
    ILI9341SetCursor(PoincareLeft+b-8,TFT_HEIGHT);
    DrawInt(f[i], MediumFont, TFT_WHITE);
    ILI9341SetCursor(PoincareLeft-20,PoincareBottom-b+4);
    DrawInt(f[i], MediumFont, TFT_WHITE);
  }
  DrawLine(PoincareLeft, PoincareBottom, PoincareLeft+PoincareBottom, 0, TFT_DARKDARKGRAY);     
  
  DrawPoincareLine(500,500,1115,916,TFT_DARKGREEN); // normal
  DrawPoincareLine(1115,916,1115,1115,TFT_DARKGREEN);     
  DrawPoincareLine(1115,1115,916,1115,TFT_DARKGREEN);     
  DrawPoincareLine(916,1115,500,500,TFT_DARKGREEN);     
  DrawPoincareFrame(500,1200,700,2500, TFT_MAROON);  // N-V-N    
  DrawPoincareFrame(1200,500,2500,700, TFT_MAROON);     
  DrawPoincareFrame(500,500,700,1200, RGB(48,48,0)); // premature ventricular
  DrawPoincareFrame(500,500,1200,700, RGB(48,48,0));
  DrawPoincareLine(320,320,320,650,RGB(0,0,64)); // atrial fibrilation
  DrawPoincareLine(320,650,800,1700,RGB(0,0,64)); 
  DrawPoincareLine(800,1700,1700,1700,RGB(0,0,64));     
  DrawPoincareLine(1700,1700,1700,800,RGB(0,0,64));     
  DrawPoincareLine(650,320,1700,800,RGB(0,0,64)); 
  DrawPoincareLine(650,320,320,320,RGB(0,0,64)); 
  DrawPoincareFrame(1700,900,2500,1400,RGB(0,32,32)); // missed beat
  DrawPoincareFrame(900,1700,1400,2500,RGB(0,32,32));     
   
  DrawVLine(PoincareLeft, 0, PoincareBottom, TFT_WHITE);   
  DrawHLine(PoincareLeft, PoincareBottom, TFT_WIDTH, TFT_WHITE);
}

//-----------------------------------------------------------------------------
// DrawGridVLine
//-----------------------------------------------------------------------------
void DrawGridVLine(int x, int y1, int y2)
{
  int y;
  
  if (y1 > y2)
    DrawGridVLine(x, y2, y1); else
  {
    if (x*5 % (1000/SamplePeriod) == 0)
      DrawVLine(x, y1, y2-y1+1, TFT_RED); else
    if (x*25 % (1000/SamplePeriod) == 0)
      DrawVLine(x, y1, y2-y1+1, TFT_MAROON); else
    {
      DrawVLine(x, y1, y2-y1+1, TFT_BLACK);   
      for (y=y1; y <= y2; y++)
        if (y % 40 == 0)
          DrawPixel(x,y,TFT_RED); else
        if (y % 8 == 0)
          DrawPixel(x,y,TFT_MAROON); 
    }
  }
}

//-----------------------------------------------------------------------------
// centrePeak
//   attempt to keep a peak in the first third of the screen
//-----------------------------------------------------------------------------
void centrePeak(uint16_t period, uint16_t x)
{
  int b,c;
  static int dr = 480;
  static int dt = 0;

  c = period/SamplePeriod;
  b = c;
  while (b < TFT_WIDTH)
    b += c;
  if (b > dr+10)
    dr += 10; else
  if (b < dr-10)
    dr -= 10; 
  if (b > dr)
    dr++; else
    dr--; 
  dr = constrain(dr,TFT_WIDTH-1,2*TFT_WIDTH);
  DisplayRepeat = dr; 
  if (x < 80)
    dt--; else
  if (x < 160)
    dt++;
  DisplayRepeat += dt; 
  DisplayRepeat = constrain(DisplayRepeat,TFT_WIDTH-1,2*TFT_WIDTH);
}

//-----------------------------------------------------------------------------
// DrawPoincare
//    Poincare plot
//    t is in range 400..2000 mSec
//-----------------------------------------------------------------------------
void DrawPoincare(uint16_t t)
{
  const int nPeriods = 500;
  static byte Periods[nPeriods] = {0};
  static int i = 0;
  int prev;

  if (t <= 0) {
    memset(Periods,0,sizeof(Periods));
    return;
  }

  t = t / PoincareScale;

  prev = Periods[i];
  i = (i+1) % nPeriods;
  if (PoincareLeft+Periods[i] > 0)
    DrawPixel(PoincareLeft+Periods[i], PoincareBottom-(Periods[(i+1) % nPeriods]), TFT_BLACK);
  Periods[i] = t;
  if (prev > 0)
    DrawPixel(PoincareLeft+prev, PoincareBottom-t, TFT_WHITE);
}  

//-----------------------------------------------------------------------------
// calcBPM
//-----------------------------------------------------------------------------
void calcBPM(uint8_t ecg, uint16_t x)
{
  static uint8_t prevy = 0;
  uint32_t t;
  static uint32_t prevt = 0;
  static int InPeak = 0;
  static int bpm = 50;
  const int threshHi = 40;
  const int threshLo = threshHi - 5;
  int b;
  static bool first = true;

  if (ecg > prevy)
    prevy++; else
    prevy--;

  if (InPeak > 0) {
    InPeak++;
    if (ecg < prevy+threshLo) {     
      t = millis();
      b = 60000 / (t-prevt);
      if ((b >= 20) && (b <= 250) && (InPeak < 100/SamplePeriod)) { // peak must be narrow and BPM must be reasonable
        if (ignoreBeats > 0) {
          ignoreBeats--;
        } else {
          bpm = (bpm*7+b)/8; // smoothing
          bpm += constrain(b-bpm,-1,+1);
          
          DrawBox(0, 0, 30, 20, TFT_NAVY);
          ILI9341SetCursor(4,17);
          DrawInt(bpm, LargeFont, TFT_WHITE);
           //Serial.print("=");
           //Serial.println(bpm);


          if (mode == mdPoincare)
            DrawPoincare(t-prevt);
            
          centrePeak(t-prevt, x);
        }

      }
      prevt = t;      
      InPeak = 0;
    }    
  } else {
    if (ecg > prevy+threshHi) 
      InPeak = 1;
  }  
}

//-----------------------------------------------------------------------------
// DrawTraceLarge
//-----------------------------------------------------------------------------
void DrawTraceLarge(uint8_t y)
{
  static uint16_t x = 0;
  static uint8_t pt = 0;
  static uint8_t py = 0;
  int i;

  calcBPM(y, x);

  x++;
  x = x % DisplayRepeat;
  y = TFT_HEIGHT-y;
  
  if (x < TFT_WIDTH) {
    DrawGridVLine(x, pt, Trace[x]);
    if (y >= py)
      DrawVLine(x, py, y-py+1, TFT_WHITE); else
      DrawVLine(x, y, py-y+1, TFT_WHITE);
    py = y;

    pt = Trace[x];
    Trace[x] = y;
  }
}

//-----------------------------------------------------------------------------
// DrawTraceSmall
//-----------------------------------------------------------------------------
void DrawTraceSmall(uint8_t y)
{
  static uint16_t x = 0;
  static uint8_t pt = 0;
  static uint8_t py = 0;
  static uint8_t yofs = 0;
  int i, x4, y4;

  x++;
  x = x % (TFT_WIDTH*4);
  x4 = x/4;
  y4 = y/4;

  if (x == 0) {
    yofs = (yofs+TFT_HEIGHT/4);
    if (yofs >= TFT_HEIGHT)
      yofs = 0;
  }

  if (x % 4 == 0) {
    if (x4*4*5 % (1000/SamplePeriod) == 0)
      DrawVLine(x4, yofs, TFT_HEIGHT/4, TFT_RED); else
      DrawVLine(x4, yofs, TFT_HEIGHT/4, TFT_BLACK);
  }

  y4 = constrain(TFT_HEIGHT/4-y4, 0,TFT_HEIGHT/4-1);
  if (y4 >= py)
    DrawVLine(x4, py+yofs, y4-py+1, TFT_WHITE); else
    DrawVLine(x4, y4+yofs, py-y4+1, TFT_WHITE);

  py = y4;

  calcBPM(y, x);
}

//-------------------------------------------------------------------------
// MakeFakePulse
//-------------------------------------------------------------------------
/*#ifdef bHasFakeECG
void MakeFakePulse(void)
{
  static uint16_t fakePeriod = 1000;
  static uint16_t i = 0;
  i++;
  i = i % (fakePeriod/SamplePeriod);
  digitalWrite(FAKE_OUT,i < 10);
  if (i == 0)
    fakePeriod = (fakePeriod*3 + random(600,1300)) / 4;
}
#endif
*/
//-----------------------------------------------------------------------------
// CheckButton
//   check pushbutton and cnahge mode
//-----------------------------------------------------------------------------
void CheckButton(void)
{
  static int btnCnt = 0;
  if (digitalRead(BUTTON_IN)) {
    btnCnt = 0;
  } else {
    btnCnt++;      
    if (btnCnt == 20) {
      mode = (mode+1) % 3;
      DrawGrid();
    }
  }
}

//-----------------------------------------------------------------------------
// CheckLeadsOff
//-----------------------------------------------------------------------------
void CheckLead(byte pin, int y, char *s, bool *LO)
{
  static uint8_t i = 0;
  i++;
  
  if (digitalRead(pin)) {
    if ((!*LO) || (i < 2)) {
      DrawBox(0, y, 54, 20, TFT_MAROON);
      DrawStringAt(4, y+15, s, LargeFont, TFT_WHITE);
      *LO = true;
    }
    ignoreBeats = 10;
  } else {
    if (*LO) {
      DrawGrid();
      *LO = false;
    }
  }
}

void CheckLeadsOff(void)
{
  static bool LO_P = false;
  static bool LO_N = false;
  CheckLead(LO_P_IN, 20, "L off", &LO_P);
  CheckLead(LO_N_IN, 40, "R off", &LO_N);
}

//-------------------------------------------------------------------------
// Filter
//   Low Pass Filter
//-------------------------------------------------------------------------
int LowPassFilter(int ecg)
{
    static int py = 0;
    static int ppy = 0;
    static int ppecg = 0;
    static int pecg = 0;
    int y;
    static int mid = 0;

    const long filt_a0 = 8775;
    const long filt_a1 = 17550;
    const long filt_a2 = 8775;
    const long filt_b1 = -50049;
    const long filt_b2 = 19612;

    if (ecg > mid)
      mid++; else
      mid--;

    ecg -= mid; // to remove DC offset
    y = (filt_a0*ecg + filt_a1*pecg + filt_a2*ppecg - filt_b1*py - filt_b2*ppy) >> 16;
    ppy = py;
    py = y;
    ppecg = pecg;
    pecg = ecg;
    return y + mid;
}

//-------------------------------------------------------------------------
// FilterLowPass
//   Low Pass Filter
//-------------------------------------------------------------------------
int FilterLowPass(int ecg)
{
    static int py = 0;
    static int ppy = 0;
    static int ppecg = 0;
    static int pecg = 0;
    int y;
    static int mid = 0;

    const long filt_a0 = 8775;
    const long filt_a1 = 17550;
    const long filt_a2 = 8775;
    const long filt_b1 = -50049;
    const long filt_b2 = 19612;

    if (ecg > mid) 
      mid++; else 
      mid--;
    
    ecg -= mid; // to remove DC offset
    y = (filt_a0*ecg + filt_a1*pecg + filt_a2*ppecg - filt_b1*py - filt_b2*ppy) >> 16;
    ppy = py;
    py = y;
    ppecg = pecg;
    pecg = ecg;
    return constrain(y + mid,0,1023);
}

//-------------------------------------------------------------------------
// FilterNotch50HzQ1
//   Notch Filter 50Hz
//   Q = 1 or 2
//-------------------------------------------------------------------------
int FilterNotch50HzQ1(int ecg)
{
    static int py = 0;
    static int ppy = 0;
    static int ppecg = 0;
    static int pecg = 0;
    int y;
    static int mid = 0;

    const long filt_a0 = 43691; // Q=1
    const long filt_b2 = 21845; // Q=1

    if (ecg > mid) 
      mid++; else 
      mid--;
    
    ecg -= mid; // to remove DC offset
    y = (filt_a0*(ecg + ppecg) - filt_b2*ppy) >> 16;
    ppy = py;
    py = y;
    ppecg = pecg;
    pecg = ecg;
    return constrain(y + mid,0,1023);
}

//-------------------------------------------------------------------------
// FilterNotch50HzQ2
//   Notch Filter 50Hz
//   Q = 1 or 2
//-------------------------------------------------------------------------
int FilterNotch50HzQ2(int ecg)
{
    static int py = 0;
    static int ppy = 0;
    static int ppecg = 0;
    static int pecg = 0;
    int y;
    static int mid = 0;

    const long filt_a0 = 52429; // Q=2
    const long filt_b2 = 39322; // Q=2

    if (ecg > mid) 
      mid++; else 
      mid--;
    
    ecg -= mid; // to remove DC offset
    y = (filt_a0*(ecg + ppecg) - filt_b2*ppy) >> 16;
    ppy = py;
    py = y;
    ppecg = pecg;
    pecg = ecg;
    return constrain(y + mid,0,1023);
}

//-------------------------------------------------------------------------
// FilterNotch60Hz
//   Notch Filter 60Hz
//   Q = 1
//-------------------------------------------------------------------------
int FilterNotch60Hz(int ecg)
{
    static int py = 0;
    static int ppy = 0;
    static int ppecg = 0;
    static int pecg = 0;
    int y;
    static int mid = 0;

    const long filt_a0 = 44415;
    const long filt_a1 = 27450;
    const long filt_b2 = 23294;

    if (ecg > mid) 
      mid++; else 
      mid--;
    
    ecg -= mid; // to remove DC offset
    y = (filt_a0*(ecg + ppecg) + filt_a1*(pecg - py) - filt_b2*ppy) >> 16;
    ppy = py;
    py = y;
    ppecg = pecg;
    pecg = ecg;
    return constrain(y + mid,0,1023);
}

int cont;

//-------------------------------------------------------------------------
// setup
//-------------------------------------------------------------------------
void setup(void)
{
  Serial.begin(256000);
  //Serial.println("ECG");

  pinMode(ECG_IN, INPUT);
  analogReference(EXTERNAL);
  analogRead(ECG_IN); // initialise ADC to read audio input

  pinMode(BUTTON_IN, INPUT_PULLUP);   
  pinMode(LO_P_IN, INPUT);   
  pinMode(LO_N_IN, INPUT);
  pinMode(7, INPUT_PULLUP);   

//#ifdef bHasFakeECG
  //pinMode(FAKE_OUT, OUTPUT);
//#endif

  ILI9341Begin(TFT_CS, TFT_CD, TFT_RST, TFT_WIDTH, TFT_HEIGHT, ILI9341_Rotation3);

  DrawGrid();
  DrawStringAt(95, 90, "CONECTADO", LargeFont, TFT_WHITE);
  DrawStringAt(40, 118, "CAPTURANDO MUESTRA", LargeFont, TFT_WHITE);


  //Serial.println("LABEL,hora,lectura");

}

//-----------------------------------------------------------------------------
// Main routines
// loop
//-----------------------------------------------------------------------------
void loop(void)
{
   // Serial.flush();
  // if(t<=1000){
    //cont++;
   // Serial.print(cont);
   // Serial.print(":");
    //Serial.print(t);
    //escalar de 0 a 1
    //ecg_esc=(ecg-0|)/(1023-0);
    //Serial.print("DATA,TIME,");
 
  float ecg;
  float ecg_esc;
  static unsigned long nextTime = 0;  
  unsigned long t;  
  
  t = millis();
  
  if (t > nextTime) {
    if (t > nextTime+16)
      nextTime = t; else
      nextTime = nextTime + 8;

    ecg = getADCfast();

    ecg = FilterNotch50HzQ1(ecg);
//    ecg = FilterNotch50HzQ2(ecg);
    ecg = FilterLowPass(ecg);
    ecg = FilterNotch60Hz(ecg);
    Serial.print("#");  
    Serial.println(ecg);
                
    /*switch (mode) {
         case mdLargeECG: DrawTraceLarge(ecg / 4); break;
         //case mdSmallECG: DrawTraceSmall(ecg / 4); break;
         case mdPoincare: calcBPM(ecg / 4, 0); break;
              }*/
    CheckLeadsOff();
             
   }
           
}    

  

