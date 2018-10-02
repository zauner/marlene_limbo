#include <Stepper.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

#include <DMXSerial.h>

#define SLAVE_ID 8

#define NO_EASE 0
#define EASE_OUT 1
#define EASE_EASE 2

#define STATE_TUNE 0
#define STATE_CALIBRATE 1
#define STATE_PLAY 2

#define ADDR_OFFSET 5

#define FPS 40

#define IN1  5
#define IN2  6
#define IN3  7
#define IN4  8
//#define Button1  A4
#define ENABLE 4
#define SENSOR_PIN 13
int Steps = 0;
boolean Direction = true;// gre
boolean manual = false;

short state = STATE_CALIBRATE;

int Button1 = A4;
int Button2 = A5;

int p = 0;
unsigned long t = 0;
int df = 1000/FPS;

int tuneSteps = 0;

SoftwareSerial mySerial = SoftwareSerial(9,10);

typedef struct animation{
  bool active;
  int startPos;
  int targetPos;
  int offset;
  unsigned long duration;
  unsigned long startTime;
  unsigned long endTime;
  int ease;
} Animation;

Animation currAnim;

void setup()
{
  DMXSerial.init(DMXReceiver);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(Button1, INPUT);
  pinMode(Button2, INPUT);
  pinMode(ENABLE, OUTPUT);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  digitalWrite(ENABLE, LOW);
  DMXSerial.write(SLAVE_ID, 0);

  pinMode(9, INPUT);
  pinMode(10, OUTPUT);
  mySerial.begin(9600);
  mySerial.println("Hello I am a Receiver.");

  byte storedTuneSteps = EEPROM.read(0);
  if (storedTuneSteps != 255)
    tuneSteps = storedTuneSteps - 128;
  mySerial.println("Read Tune Steps from EEPROM:");
  mySerial.println(tuneSteps);
}

int stepdelay = 0;
unsigned long prevloop_t = 0;

void loop()
{

  t = millis();

  if (state==STATE_TUNE) {
    if(analogRead(Button1) > 500){
      Direction = true;
      for (int i=0; i<5; i++) {
        stepper(1);
        delay(10);
      }
      tuneSteps--;
      EEPROM.write(0, tuneSteps+128);
      mySerial.println("SAVED TUNE STEPS");
      mySerial.println(tuneSteps);
      delay(100);
    }
    
    if(analogRead(Button2) > 500){
      Direction = false;
      for (int i=0; i<5; i++) {
        stepper(1);
        delay(10);
      }
      tuneSteps++;
      EEPROM.write(0, tuneSteps+128);
      mySerial.println("SAVED TUNE STEPS");
      mySerial.println(tuneSteps);
      delay(100);
    }

    if(DMXSerial.dataUpdated()){
      DMXSerial.resetUpdated();
      if (DMXSerial.read(253)>10) {
        mySerial.println("Tuning is over, show is about to start!");
        state = STATE_PLAY;
      }
    }
  }
  else if (state==STATE_CALIBRATE) {
    Direction = true;
    if (digitalRead(SENSOR_PIN)==LOW || analogRead(Button1)>500) {
      // Replay manual tune steps
      if (tuneSteps>0)
        Direction = false;
      else
        Direction = true;
      for (int i=0; i<abs(tuneSteps)*5; i++) {
        stepper(1);
        delay(10);
      }

      Direction = true;
      p = 0;
      state = STATE_TUNE;
    }
    else {
      stepper(1);
      delay(10);
    }
  }
  else if (state==STATE_PLAY) {

    if(DMXSerial.dataUpdated()){
      DMXSerial.resetUpdated();
      int orig = DMXSerial.read(SLAVE_ID * ADDR_OFFSET + 1);
      int subOrig = DMXSerial.read(SLAVE_ID * ADDR_OFFSET + 2);
      int recEase = DMXSerial.read(SLAVE_ID * ADDR_OFFSET + 3);
      int recTime = DMXSerial.read(SLAVE_ID * ADDR_OFFSET + 4)*100;
      int recPos = (int)((float)orig + (float)subOrig/100.0)/256.0 * 4096;
      if (orig>0 && currAnim.targetPos!=recPos && DMXSerial.read(254)>10) {
        if (recTime==0) {
          //mySerial.println("Weird Packet. Ignoring ...");
        }
        else {
          mySerial.println("Got Something!");
          createAnimation(
            recPos,
            recTime,
            recEase
          );
        }
      }
      //newPos = DMXSerial.read(myID) * 10 ;
      //stepDelay = DMXSerial.read(254);
    }
  
    if (t-prevloop_t < stepdelay)
      return;
  
    prevloop_t = t;
  
    if (currAnim.active && currAnim.targetPos!=p) {
      int deltaPos = getPos(currAnim, t + df) - p;
      Direction = deltaPos<0;
      stepper(1);
      if (!Direction)
        p++;
      else
        p--;
      if (abs(currAnim.targetPos-p)<3) {
        mySerial.println("and stop");
        currAnim.active = false;
        // CHECK: needs delay?
        return;
      }
      if (deltaPos!=0)
        stepdelay = df / abs(deltaPos);
      else
        stepdelay = 1;
    }
    else {
      currAnim.active = false;
    }
  }
   
}

void createAnimation(int targetPos, long duration, int ease) {
  currAnim.active = true;
  currAnim.startPos = p;
  currAnim.targetPos = targetPos;
  currAnim.startTime = t;
  currAnim.endTime = t + duration;

  currAnim.offset = targetPos - p;
  currAnim.duration = duration;

  currAnim.ease = ease;
}

int getPos(Animation anim, long tf) {
  int pos = 0;
  switch (anim.ease) {
    case NO_EASE:
      pos = ((float)anim.offset / (float)anim.duration) * (tf - anim.startTime) + anim.startPos;
      break;
    case EASE_OUT:
      pos = sin((float)(tf - anim.startTime) / (float)anim.duration * HALF_PI) * anim.offset + anim.startPos; 
      break;
    case EASE_EASE:
      pos = (sin((float)(tf - anim.startTime) / (float)anim.duration * PI + HALF_PI) * -1 + 1) / 2 * anim.offset + anim.startPos;
      break;
  }  
  return pos;
}

// Low Level Stepper functions

int prevStep = 0;

void stepper(int xw) {
  for (int x = 0; x < xw; x++) {
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
    prevStep = Steps;
  }
  
  /*digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  */
}
void SetDirection() {
  if (Direction == true) {
    Steps++;
  }
  if (Direction == false) {
    Steps--;
  }
  if (Steps > 7) {
    Steps = 0;
  }
  if (Steps < 0) {
    Steps = 7;
  }
}
