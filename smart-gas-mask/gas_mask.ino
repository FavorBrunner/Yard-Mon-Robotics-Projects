/*
  Smart Gas Mask — ESP32 + MQ-2 + RGB LED + Buzzer
  Wearable safety device: calibrates to ambient air, alerts on dangerous gas.
  MQ-2→GPIO25, RGB: R=23 G=22 B=21, Buzzer→GPIO19
*/
const int MQ2_PIN=25,RED_PIN=23,GREEN_PIN=22,BLUE_PIN=21,BUZZER_PIN=19;
const int LEDC_FREQ=5000,LEDC_RES=8,CH_RED=0,CH_GREEN=1,CH_BLUE=2;
unsigned long lastSensorReadTime=0;const unsigned long SENSOR_READ_INTERVAL=100;
bool calibrated=false;unsigned long calibStartTime=0;const unsigned long CALIBRATION_TIME=10000UL;
long calibSum=0;int calibCount=0,baselineValue=0;const int GAS_MARGIN=150;int gasThreshold=0;
unsigned long breathStartTime=0;const unsigned long BREATH_PERIOD=3000;
bool gasDetected=false;unsigned long lastBlinkTime=0;const unsigned long BLINK_PERIOD=300;bool redOn=false;
unsigned long lastBeepToggleTime=0;const unsigned long BEEP_PERIOD=200;bool buzzerOn=false;
void setRGB(uint8_t r,uint8_t g,uint8_t b){ledcWrite(CH_RED,r);ledcWrite(CH_GREEN,g);ledcWrite(CH_BLUE,b);}
void safeModeBreathing(){digitalWrite(BUZZER_PIN,LOW);unsigned long elapsed=(millis()-breathStartTime)%BREATH_PERIOD;float phase=(float)elapsed/BREATH_PERIOD;float brightness=(phase<0.5f)?(phase/0.5f):((1.0f-phase)/0.5f);setRGB(0,(uint8_t)(brightness*255.0f),0);}
void dangerModeAlarm(){unsigned long now=millis();if(now-lastBlinkTime>=BLINK_PERIOD){lastBlinkTime=now;redOn=!redOn;}if(redOn)setRGB(255,0,0);else setRGB(0,0,0);if(now-lastBeepToggleTime>=BEEP_PERIOD){lastBeepToggleTime=now;buzzerOn=!buzzerOn;digitalWrite(BUZZER_PIN,buzzerOn?HIGH:LOW);}}
void setup(){Serial.begin(115200);delay(1000);pinMode(MQ2_PIN,INPUT);pinMode(BUZZER_PIN,OUTPUT);digitalWrite(BUZZER_PIN,LOW);ledcSetup(CH_RED,LEDC_FREQ,LEDC_RES);ledcSetup(CH_GREEN,LEDC_FREQ,LEDC_RES);ledcSetup(CH_BLUE,LEDC_FREQ,LEDC_RES);ledcAttachPin(RED_PIN,CH_RED);ledcAttachPin(GREEN_PIN,CH_GREEN);ledcAttachPin(BLUE_PIN,CH_BLUE);setRGB(0,0,0);calibStartTime=millis();Serial.println("Calibrating MQ-2...");}
void loop(){unsigned long now=millis();if(!calibrated){if(now-calibStartTime<CALIBRATION_TIME){int r=analogRead(MQ2_PIN);calibSum+=r;calibCount++;}else{baselineValue=(calibCount>0)?calibSum/calibCount:analogRead(MQ2_PIN);gasThreshold=baselineValue+GAS_MARGIN;calibrated=true;breathStartTime=millis();Serial.printf("Calibrated. Baseline:%d Threshold:%d\n",baselineValue,gasThreshold);}return;}if(now-lastSensorReadTime>=SENSOR_READ_INTERVAL){lastSensorReadTime=now;int val=analogRead(MQ2_PIN);gasDetected=(val>gasThreshold);Serial.printf("MQ-2:%d|Thresh:%d|Gas:%s\n",val,gasThreshold,gasDetected?"YES":"NO");}if(gasDetected)dangerModeAlarm();else safeModeBreathing();}
