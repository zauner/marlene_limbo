#include <SoftwareSerial.h>
#include <DMXSerial.h>
#include "Keyframe.h"
#include "animations.h"

#define NO_EASE 0
#define EASE_OUT 1
#define EASE_EASE 2

#define STATE_CALIBRATING 0
#define STATE_TUNE 1
#define STATE_PLAY 2
#define STATE_PAUSE 3

#define IN1  5
#define IN2  6
#define IN3  7
#define IN4  8
#define ENABLE 4
#define REEDPIN 13

#define CALIBRATION_TIME 30000

int Steps = 0;
boolean Direction = true;// gre
unsigned long last_time;
int steps_left = 4095;
long time;

short state = STATE_TUNE;

int Button1 = A5;
int Button2 = A4;

SoftwareSerial mySerial = SoftwareSerial(9,10);

void setup()
{
  DMXSerial.init(DMXController);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(Button1, INPUT);
  pinMode(ENABLE, OUTPUT);
  pinMode(REEDPIN, INPUT_PULLUP);
  digitalWrite(ENABLE, HIGH);

  pinMode(9, INPUT);
  pinMode(10, OUTPUT);
  mySerial.begin(9600);
  mySerial.println("Hello");
  delay(1000);

}

int keyframe = -1;
long transitionStart = -1;

void loop()
{
  int addrOffset = 6;

  if (state == STATE_CALIBRATING) {
    if (analogRead(Button1) > 500) {
      state = STATE_TUNE;
    }
    else {
      if (millis()>CALIBRATION_TIME)
        state = STATE_PLAY;
    }
  }
  else if (state == STATE_TUNE) {
    if(analogRead(Button2) > 500) {
      DMXSerial.write(253, 100);
      state = STATE_PLAY;
      delay(1000);
    }
  }
  else if (state == STATE_PLAY) {

    if(analogRead(Button2) > 500) {
      DMXSerial.write(253, 100);
      state = STATE_TUNE;
      delay(1000);
    }
    
    if ((millis() - transitionStart)>1000) {
      DMXSerial.write(253, 0);
    }
    if (transitionStart<0 || (millis() - transitionStart)>anim[keyframe].dt) {
      mySerial.println("Starting new because");
      mySerial.println(millis());
      mySerial.println("-");
      mySerial.println(transitionStart);
      mySerial.println(millis() - transitionStart);
      mySerial.println(">");
      mySerial.println(anim[keyframe].dt);
      //delay(3000);
      transitionStart = millis();
      keyframe++;
      if (keyframe>sizeof(anim)/sizeof(MyKeyframe)-1)
        keyframe = 0;
      mySerial.println("Starting Transition");
      mySerial.println(keyframe);
      for (int i=0; i<sizeof(anim[keyframe].pos)/sizeof(int); i++) {
        mySerial.println(anim[keyframe].pos[i]);
      }
      mySerial.println(anim[keyframe].dt);

      for (int i=0; i<sizeof(anim[keyframe].pos)/sizeof(int); i++) {
        float mappedPos = (float)anim[keyframe].pos[i] / 4096.0 * 256.0;
        int subPrecisionPos = (mappedPos - floor(mappedPos))*100;
        if (mappedPos<1) {
          mappedPos = 1;
          subPrecisionPos = 0;
        }
        DMXSerial.write(i * addrOffset + 3, anim[keyframe].ease);
        DMXSerial.write(i * addrOffset + 4, (int)(anim[keyframe].dt/100.0));
        DMXSerial.write(i * addrOffset + 1, floor(mappedPos));
        DMXSerial.write(i * addrOffset + 2, subPrecisionPos);
        int checksum = ((int)(anim[keyframe].dt/100.0) + (int)floor(mappedPos) + (int)subPrecisionPos) % 256;
        DMXSerial.write(i * addrOffset + 5, checksum);
        
        /*DMXSerial.write(i * addrOffset + 3, anim[keyframe].ease);
        DMXSerial.write(i * addrOffset + 4, (int)(anim[keyframe].dt/100.0));
        DMXSerial.write(i * addrOffset + 1, (anim[keyframe].pos[i] & (255 << 8)) >> 8);
        DMXSerial.write(i * addrOffset + 2, anim[keyframe].pos[i] & 255);
        */
      }
      DMXSerial.write(253, 100);
      
    }
    for (int i=0; i<sizeof(anim[keyframe].pos)/sizeof(int); i++) {
      float mappedPos = (float)anim[keyframe].pos[i] / 4096.0 * 256.0;
      int subPrecisionPos = (mappedPos - floor(mappedPos))*100;
      if (mappedPos<1) {
        mappedPos = 1;
        subPrecisionPos = 0;
      }
      DMXSerial.write(i * addrOffset + 3, anim[keyframe].ease);
      DMXSerial.write(i * addrOffset + 4, (int)(anim[keyframe].dt/100.0));
      DMXSerial.write(i * addrOffset + 1, floor(mappedPos));
      DMXSerial.write(i * addrOffset + 2, subPrecisionPos);
      int checksum = ((int)(anim[keyframe].dt/100.0) + (int)floor(mappedPos) + (int)subPrecisionPos) % 256;
      DMXSerial.write(i * addrOffset + 5, checksum);
      
      /*DMXSerial.write(i * addrOffset + 3, anim[keyframe].ease);
      DMXSerial.write(i * addrOffset + 4, (int)(anim[keyframe].dt/100.0));
      DMXSerial.write(i * addrOffset + 1, (anim[keyframe].pos[i] & (255 << 8)) >> 8);
      DMXSerial.write(i * addrOffset + 2, anim[keyframe].pos[i] & 255);
      */
    }
  }
  
}

void stepper(int xw) {
  for (int x = 0; x < xw; x++) {
    delayMicroseconds(1000);
    switch (Steps) {
      case 0:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
        break;
      case 1:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, HIGH);
        break;
      case 2:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        break;
      case 3:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        break;
      case 4:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        break;
      case 5:
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, HIGH);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        break;
      case 6:
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        break;
      case 7:
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
        break;
      default:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        break;
    }
    SetDirection();
  }

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}
void SetDirection() {
  if (Direction == 1) {
    Steps++;
  }
  if (Direction == 0) {
    Steps--;
  }
  if (Steps > 7) {
    Steps = 0;
  }
  if (Steps < 0) {
    Steps = 7;
  }
}
