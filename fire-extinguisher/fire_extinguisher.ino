/*
  Arduino Fire Extinguisher
  ==========================
  Flame-detecting servo mechanism that automatically responds to fire.
  Flame sensor→D8, Servo→D5.
*/
#include <Servo.h>
const int flamePin=8,servoPin=5;
Servo myServo;
int currentPos=90;bool flipped=false;
void setup(){pinMode(flamePin,INPUT);myServo.attach(servoPin);myServo.write(currentPos);delay(500);}
void loop(){
  int flameState=digitalRead(flamePin);
  if(flameState==HIGH&&!flipped){currentPos=constrain(currentPos-230,0,180);myServo.write(currentPos);flipped=true;delay(500);}
  else if(flameState==LOW&&flipped){currentPos=constrain(currentPos+85,0,180);myServo.write(currentPos);flipped=false;delay(500);}
}
