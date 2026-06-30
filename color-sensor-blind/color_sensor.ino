/*
  TCS3200 Color Sensor for Blind People
  =======================================
  Accessibility device that identifies colors and announces them via Serial.
  Wide detection ranges for Blue, Green, Brown, Yellow, Grey.
  Pins: OUT=D12, S2=D11, S3=D10, S1=D9, S0=D8
*/
const byte PIN_S0=8,PIN_S1=9,PIN_S2=11,PIN_S3=10,PIN_OUT=12;
const uint16_t FILTER_SETTLE_US=200;const uint8_t SAMPLES=5;const unsigned long PULSE_TIMEOUT_US=100000UL;
struct Range{float lo;float hi;};
Range BLUE_R={1059,1431},BLUE_G={1670,2260},BLUE_B={3635,4920};
Range GREEN_R={1915,2435},GREEN_G={2735,3475},GREEN_B={2250,2860};
Range BROWN_R={4645,5915},BROWN_G={3005,3820},BROWN_B={2910,3700};
Range YELL_R={6875,8745},YELL_G={5295,6735},YELL_B={3735,4755};
Range GREY_R={3470,4700},GREY_G={3220,4365},GREY_B={3450,4675};
void setup(){pinMode(PIN_S0,OUTPUT);pinMode(PIN_S1,OUTPUT);pinMode(PIN_S2,OUTPUT);pinMode(PIN_S3,OUTPUT);pinMode(PIN_OUT,INPUT);digitalWrite(PIN_S0,HIGH);digitalWrite(PIN_S1,LOW);Serial.begin(115200);while(!Serial){}}
bool inRange(float v,const Range& R){return(v>=R.lo&&v<=R.hi);}
float measureHzOnce(){unsigned long tLow=pulseIn(PIN_OUT,LOW,PULSE_TIMEOUT_US);if(tLow==0)return 0.0;return 500000.0f/(float)tLow;}
void setFilterRed(){digitalWrite(PIN_S2,LOW);digitalWrite(PIN_S3,LOW);}
void setFilterGreen(){digitalWrite(PIN_S2,HIGH);digitalWrite(PIN_S3,HIGH);}
void setFilterBlue(){digitalWrite(PIN_S2,LOW);digitalWrite(PIN_S3,HIGH);}
float readChannelHz(uint8_t idx){switch(idx){case 0:setFilterRed();break;case 1:setFilterGreen();break;default:setFilterBlue();break;}delayMicroseconds(FILTER_SETTLE_US);float sum=0;uint8_t ok=0;for(uint8_t i=0;i<SAMPLES;i++){float hz=measureHzOnce();if(hz>0){sum+=hz;ok++;}}if(!ok)return 0.0;return sum/ok;}
void loop(){float rHz=readChannelHz(0),gHz=readChannelHz(1),bHz=readChannelHz(2);
  if(inRange(rHz,BLUE_R)&&inRange(gHz,BLUE_G)&&inRange(bHz,BLUE_B)){Serial.println(F("Blue"));delay(2000);return;}
  if(inRange(rHz,GREEN_R)&&inRange(gHz,GREEN_G)&&inRange(bHz,GREEN_B)){Serial.println(F("Green"));delay(2000);return;}
  if(inRange(rHz,BROWN_R)&&inRange(gHz,BROWN_G)&&inRange(bHz,BROWN_B)){Serial.println(F("Brown"));delay(2000);return;}
  if(inRange(rHz,YELL_R)&&inRange(gHz,YELL_G)&&inRange(bHz,YELL_B)){Serial.println(F("Yellow"));delay(2000);return;}
  if(inRange(rHz,GREY_R)&&inRange(gHz,GREY_G)&&inRange(bHz,GREY_B)){Serial.println(F("Grey"));delay(2000);return;}}
