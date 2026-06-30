/*
  ESP32 Interview Smartwatch — GC9A01 Round Display
  ===================================================
  Custom round-display smartwatch with scrolling bio text and
  JPEG splash screen (Omnitrix animation). Built for a professional
  interview to showcase embedded UI design.
  Pins: SCK=18, MOSI=23, DC=2, CS=4, RST=-1
  Libraries: Adafruit_GC9A01A, Adafruit_GFX, TJpg_Decoder, LittleFS
*/
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <TJpg_Decoder.h>
#include <LittleFS.h>

#define TFT_CS 4
#define TFT_DC 2
#define TFT_RST -1
Adafruit_GC9A01A tft(TFT_CS,TFT_DC,TFT_RST);

const int16_t W=240,H=240;
const uint16_t BG_COLOR=0x0000,FG_COLOR=0xFFFF,ACCENT=0x07E0;
const uint16_t SCROLL_DELAY_MS=40;const int16_t SCROLL_PIXELS_PER_STEP=1;
const uint32_t SPLASH_PERIOD_MS=120000UL,SPLASH_DURATION_MS=10000UL,BOOT_SPLASH_MS=5000UL;

const char* HEADER_LINE="Yvonne Thevenot — Cooper Union";
const char* SUBHEADER="Director of STEM Outreach";
const char* PARAS[]={
  "Director of STEM Outreach at The Cooper Union.",
  "Manages and innovates programs for NYC students in STEM.",
  "Programs include STEM Saturdays, Summer STEM, STEM Inventors, and STEM Days.",
  "Focus: hands-on engineering, computer science, and creative tech for high-school students.",
  "Joined Cooper Union to expand access and spark interest in STEM careers.",
  "Founder and Executive Director of STEM Kids NYC, expanding STEM access.",
  "In 2025, STEM Kids NYC marked 10 years and opened a Jersey City location.",
  "Recognized on national TV for community impact.",
  "Advocates equitable pathways into STEM for underrepresented learners.",
  "Belief: all students are capable; opportunity and mentorship change trajectories.",
  "Ask about collaborations: workshops, curriculum, and STEM makerspace events."
};
const size_t N_PARAS=sizeof(PARAS)/sizeof(PARAS[0]);

const int16_t TEXT_LEFT=14,TEXT_RIGHT=W-14,HEADER_TOP_Y=34,SUBHDR_Y=68,DIV1_Y=44,DIV2_Y=78,TEXT_TOP=84,FIRST_LINE_Y=100;
const int MAX_WRAPPED=160;String WRAPPED[MAX_WRAPPED];int WRAPPED_COUNT=0;
int16_t scrollY=H;uint32_t lastStepMs=0,lastSplashStart=0;bool showingSplash=false;

static bool tft_output(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t* bitmap){if(y>=H||x>=W)return true;if(x+w>W)w=W-x;if(y+h>H)h=H-y;tft.startWrite();tft.setAddrWindow(x,y,w,h);tft.writePixels(bitmap,(uint32_t)w*(uint32_t)h,true);tft.endWrite();return true;}
int16_t lineHeight(const GFXfont* font){return font?(int16_t)font->yAdvance:16;}
int16_t textWidth(const String& s,const GFXfont* font){tft.setFont(font);int16_t x1,y1;uint16_t w,h;tft.getTextBounds((char*)s.c_str(),0,0,&x1,&y1,&w,&h);return(int16_t)w;}
void wrapParagraph(const char* p,int16_t maxWidth,const GFXfont* font){String src(p);src.trim();if(!src.length())return;String line;int pos=0;while(pos<(int)src.length()){int next=src.indexOf(' ',pos);if(next<0)next=src.length();String word=src.substring(pos,next);String test=line.length()?(line+" "+word):word;if(textWidth(test,font)<=maxWidth){line=test;pos=next+1;}else{if(line.length()){if(WRAPPED_COUNT<MAX_WRAPPED)WRAPPED[WRAPPED_COUNT++]=line;line="";}else{if(WRAPPED_COUNT<MAX_WRAPPED)WRAPPED[WRAPPED_COUNT++]=word;pos=next+1;}}}if(line.length()&&WRAPPED_COUNT<MAX_WRAPPED)WRAPPED[WRAPPED_COUNT++]=line;if(WRAPPED_COUNT<MAX_WRAPPED)WRAPPED[WRAPPED_COUNT++]="";}
void buildWrapped(){WRAPPED_COUNT=0;const int16_t usable=TEXT_RIGHT-TEXT_LEFT;for(size_t i=0;i<N_PARAS;++i)wrapParagraph(PARAS[i],usable,&FreeSans9pt7b);}
void centerText(const char* txt,int16_t y,const GFXfont* font,uint16_t color){tft.setFont(font);tft.setTextColor(color,BG_COLOR);int16_t x1,y1;uint16_t w,h;tft.getTextBounds((char*)txt,0,y,&x1,&y1,&w,&h);tft.setCursor((W-(int16_t)w)/2,y);tft.print(txt);}
void drawHeaderWrapped(){tft.fillScreen(BG_COLOR);tft.setFont(&FreeSansBold12pt7b);int16_t x1,y1;uint16_t w,h;tft.getTextBounds((char*)HEADER_LINE,0,HEADER_TOP_Y,&x1,&y1,&w,&h);const int16_t maxHW=W-24;if((int16_t)w<=maxHW){centerText(HEADER_LINE,HEADER_TOP_Y,&FreeSansBold12pt7b,FG_COLOR);tft.drawLine(20,DIV1_Y,W-20,DIV1_Y,0xFFFF);centerText(SUBHEADER,SUBHDR_Y,&FreeSans9pt7b,ACCENT);tft.drawLine(30,DIV2_Y,W-30,DIV2_Y,0x39E7);return;}String s=HEADER_LINE;int split=s.indexOf("—");if(split<0)split=s.indexOf('-');if(split<0)split=s.lastIndexOf(' ');String top=(split>0)?s.substring(0,split):s;String bottom=(split>0&&split+1<(int)s.length())?s.substring(split+1):"";centerText(top.c_str(),26,&FreeSansBold12pt7b,FG_COLOR);centerText(bottom.c_str(),46,&FreeSans9pt7b,FG_COLOR);tft.drawLine(20,DIV1_Y,W-20,DIV1_Y,0xFFFF);centerText(SUBHEADER,SUBHDR_Y,&FreeSans9pt7b,ACCENT);tft.drawLine(30,DIV2_Y,W-30,DIV2_Y,0x39E7);}
void drawBody(int16_t startY){tft.setFont(&FreeSans9pt7b);tft.setTextColor(FG_COLOR,BG_COLOR);const int16_t lh=lineHeight(&FreeSans9pt7b);const int16_t ls=6;int16_t y=startY;for(int i=0;i<WRAPPED_COUNT;++i){if(y>=TEXT_TOP&&y<H){tft.setCursor(TEXT_LEFT,y);tft.print(WRAPPED[i]);}y+=lh+ls;}}
void drawVectorOmnitrix(uint32_t elapsedMs){float phase=(elapsedMs%1200)/1200.0f;float pulse=0.5f-0.5f*cosf(2.0f*PI*phase);int baseR=76,pulseR=baseR+(int)(8*pulse);tft.fillScreen(BG_COLOR);tft.drawCircle(W/2,H/2,110,0xFFFF);tft.drawCircle(W/2,H/2,108,0xFFFF);tft.fillCircle(W/2,H/2,pulseR,0x07E0);tft.drawCircle(W/2,H/2,pulseR-10,0x0000);tft.drawCircle(W/2,H/2,pulseR-11,0x0000);int cx=W/2,cy=H/2,hx=38,hy=54;tft.fillTriangle(cx-hx,cy-hy,cx+hx,cy-hy,cx,cy,0xFFFF);tft.fillTriangle(cx-hx,cy+hy,cx+hx,cy+hy,cx,cy,0xFFFF);centerText("Omnitrix Mode",H-16,&FreeSans9pt7b,0xFFFF);}
bool drawOmnitrixJpeg(){if(!LittleFS.exists("/omnitrix.jpg"))return false;tft.fillScreen(BG_COLOR);TJpgDec.setJpgScale(1);TJpgDec.drawFsJpg(0,0,"/omnitrix.jpg");return true;}
void startSplash(uint32_t durationMs,bool animateIfMissing){uint32_t t0=millis();bool hasJpg=drawOmnitrixJpeg();while(millis()-t0<durationMs){if(!hasJpg&&animateIfMissing){drawVectorOmnitrix(millis()-t0);delay(20);}else delay(20);}drawHeaderWrapped();}
void setup(){LittleFS.begin(true);TJpgDec.setSwapBytes(true);TJpgDec.setCallback(tft_output);tft.begin();tft.setRotation(0);startSplash(BOOT_SPLASH_MS,true);buildWrapped();drawHeaderWrapped();scrollY=FIRST_LINE_Y;lastStepMs=millis();lastSplashStart=millis();}
void loop(){uint32_t now=millis();if(!showingSplash&&(now-lastSplashStart>=SPLASH_PERIOD_MS))showingSplash=true;if(showingSplash){uint32_t t0=millis();bool hasJpg=drawOmnitrixJpeg();while(millis()-t0<SPLASH_DURATION_MS){if(!hasJpg)drawVectorOmnitrix(millis()-t0);delay(20);}showingSplash=false;lastSplashStart=millis();drawHeaderWrapped();}if(now-lastStepMs>=SCROLL_DELAY_MS){tft.fillRect(0,TEXT_TOP,W,H-TEXT_TOP,BG_COLOR);drawBody(scrollY);scrollY-=SCROLL_PIXELS_PER_STEP;const int16_t lh=lineHeight(&FreeSans9pt7b);const int16_t ls=6;const int16_t totalH=(lh+ls)*WRAPPED_COUNT;if(scrollY<TEXT_TOP-totalH-lh)scrollY=FIRST_LINE_Y;lastStepMs=now;}delay(5);}
