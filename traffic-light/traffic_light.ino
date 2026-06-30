/*
  Arduino Traffic Light Control System
  =====================================
  Foundational embedded systems build demonstrating timed-state
  logic with LEDs. Red=D2, Yellow=D3, Green=D4.
*/
const int RED_LED=2,YELLOW_LED=3,GREEN_LED=4;
const int GREEN_DURATION=5000,YELLOW_DURATION=2000,RED_DURATION=5000;
void setup(){pinMode(RED_LED,OUTPUT);pinMode(YELLOW_LED,OUTPUT);pinMode(GREEN_LED,OUTPUT);digitalWrite(RED_LED,LOW);digitalWrite(YELLOW_LED,LOW);digitalWrite(GREEN_LED,LOW);}
void loop(){
  digitalWrite(GREEN_LED,HIGH);digitalWrite(YELLOW_LED,LOW);digitalWrite(RED_LED,LOW);delay(GREEN_DURATION);
  digitalWrite(GREEN_LED,LOW);digitalWrite(YELLOW_LED,HIGH);digitalWrite(RED_LED,LOW);delay(YELLOW_DURATION);
  digitalWrite(GREEN_LED,LOW);digitalWrite(YELLOW_LED,LOW);digitalWrite(RED_LED,HIGH);delay(RED_DURATION);
}
