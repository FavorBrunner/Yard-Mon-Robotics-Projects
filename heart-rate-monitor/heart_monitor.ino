/*
  Heart Rate Monitor — Arduino + PulseSensor + OLED + Buzzer
  ===========================================================
  BEST VERSION — Uses U8g2 library (better than Adafruit SSD1306).
  Features: real-time BPM display, adaptive buzzer beep duration,
  heart flash animation, non-blocking buzzer timing.
  PulseSensor→A0, Buzzer→D7, OLED I2C: SCL=A5, SDA=A4.
*/
#include <Wire.h>
#include <U8g2lib.h>
#include <PulseSensorPlayground.h>
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0,U8X8_PIN_NONE);
const int PULSE_PIN=A0,BUZZER_PIN=7;const bool BUZZER_ACTIVE_LOW=false;
int threshold=580;PulseSensorPlayground pulseSensor;
bool buzzerOn=false;unsigned long buzzerOffAt=0;
const uint8_t BEEP_MS_MIN=20,BEEP_MS_MAX=70,BPM_MIN=40,BPM_MAX=180;
unsigned long lastDisplayMs=0;const uint16_t DISPLAY_PERIOD_MS=40;
unsigned long beatFlashUntilMs=0;const uint16_t BEAT_FLASH_MS=150;
int lastBPM=0;
void setup(){Serial.begin(115200);delay(50);Serial.println(F("Yard Mon Robotics — Heart Rate Monitor"));
  pinMode(BUZZER_PIN,OUTPUT);setBuzzer(false);
  pulseSensor.analogInput(PULSE_PIN);pulseSensor.setThreshold(threshold);pulseSensor.setSerial(Serial);
  if(!pulseSensor.begin())Serial.println(F("PulseSensor init failed."));
  else Serial.printf("PulseSensor started. Threshold=%d\n",threshold);
  u8g2.begin();u8g2.clearBuffer();u8g2.setDrawColor(1);u8g2.setFont(u8g2_font_6x10_tr);u8g2.drawStr(0,10,"Yard Mon Robotics");
  u8g2.setFont(u8g2_font_logisoso20_tr);u8g2.drawStr(0,38,"Heartbeat");u8g2.setFont(u8g2_font_6x10_tr);u8g2.drawStr(0,60,"Initializing...");u8g2.sendBuffer();delay(600);}
void loop(){
  if(pulseSensor.sawStartOfBeat()){int bpm=(int)pulseSensor.getBeatsPerMinute();lastBPM=bpm;
    int bpmClamped=constrain(bpm,BPM_MIN,BPM_MAX);uint8_t beepMs=map(bpmClamped,BPM_MAX,BPM_MIN,BEEP_MS_MIN,BEEP_MS_MAX);
    Serial.printf("Beat! BPM=%d (beep %dms)\n",bpm,beepMs);startBeep(beepMs);beatFlashUntilMs=millis()+BEAT_FLASH_MS;}
  if(buzzerOn&&millis()>=buzzerOffAt)setBuzzer(false);
  if(millis()-lastDisplayMs>=DISPLAY_PERIOD_MS){lastDisplayMs=millis();renderDisplay();}}
void startBeep(uint16_t d){setBuzzer(true);buzzerOffAt=millis()+d;}
void setBuzzer(bool on){buzzerOn=on;if(BUZZER_ACTIVE_LOW)digitalWrite(BUZZER_PIN,on?LOW:HIGH);else digitalWrite(BUZZER_PIN,on?HIGH:LOW);}
void renderDisplay(){u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);u8g2.drawStr(0,10,"Yard Mon Robotics");u8g2.drawHLine(0,14,128);
  char buf[8];int shown=(lastBPM>0&&lastBPM<255)?lastBPM:0;snprintf(buf,sizeof(buf),"%3d",shown);
  u8g2.setFont(u8g2_font_logisoso24_tr);u8g2.drawStr(0,40,buf);u8g2.setFont(u8g2_font_logisoso16_tr);u8g2.drawStr(78,40,"BPM");
  bool flashOn=(millis()<beatFlashUntilMs);int lineX=118,lineBottom=62,lineTop=lineBottom-(flashOn?18:4);
  u8g2.drawVLine(lineX,lineTop,lineBottom-lineTop);u8g2.drawHLine(lineX-2,lineBottom,5);
  if(flashOn)drawHeart(100,24,7);u8g2.sendBuffer();}
void drawHeart(int x,int y,int s){u8g2.drawCircle(x-s/2,y,s/2,U8G2_DRAW_ALL);u8g2.drawCircle(x+s/2,y,s/2,U8G2_DRAW_ALL);u8g2.drawLine(x-s,y,x,y+s);u8g2.drawLine(x+s,y,x,y+s);}
