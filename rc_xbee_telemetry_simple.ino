/*
Simple RC Telemetry using Xbee.
Based on CatTracker
Prototype.
By Blair Kelly
*/

#include <SoftwareSerial.h>
#include <TinyGPS.h>

TinyGPS gps;
SoftwareSerial nss(3, 4);

static void gpsdump(TinyGPS &gps);
static bool feedgps();
static void print_float(float val, float invalid, int len, int prec);
static void print_int(unsigned long val, unsigned long invalid, int len);
static void print_date(TinyGPS &gps);
static void print_str(const char *str, int len);

//pins
int xPin = A0;    // select the input pin for the potentiometer
int yPin = A1;    // select the input pin for the potentiometer
int zPin = A2;    // select the input pin for the potentiometer
int xVal;
int yVal;
int zVal;
int battPin = A5;    // select the input pin for the potentiometer

//mostFrequentlyChanged
boolean sleepCycling = true;
boolean pinging = false;
//constants
float vRef = 3.30;
float vLow = 3.66; //if battery voltage drops below this level, trigger low voltage alarm.
int pingDelay = 5000;
int sleepDuration = 12000;
int wakePeriod = 3000; //awake for this duration before going back to sleep.
//variables
unsigned long theTime = millis();
unsigned long sleepTime = millis();
unsigned long wakeTime = millis();
unsigned long pingTime = theTime + pingDelay;
boolean sleeping = false;
boolean awake = false;
//led
unsigned long lsct = theTime;
boolean ledIS = false; //the state the user wants the led to be.
boolean ledState = ledIS;
boolean ledBlink = false;
int ledBTon = 333;
int ledBToff = 222;
//gps
boolean gpsState = false;
boolean gpsOnly = true; //display only gps info at ping?
//battery
float battValue = 0.0;  // variable to store the value coming from the sensor
float theVoltage;
boolean lowBatt = false;
boolean lowBattInd = true; //indicate low battery?
//accel
boolean rA = true; //reads accel by default.
boolean pA = false; //prints accel values at regular ping
String orientation = "o:notset";
//command
String iD = ""; //incoming data stored as string
boolean clearCMD = false;


void setup() {
  pinMode(xPin, INPUT);
  pinMode(yPin, INPUT);
  pinMode(zPin, INPUT);
  pinMode(battPin, INPUT);
  
  //turn on xbee
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  //turn on gps
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH);
  delay(666);
  nss.begin(4800);
  delay(666);
  digitalWrite(6, LOW);
  //turn on accelerometer.
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  
  delay(66);
  
  // declare the ledPin as an OUTPUT:
  Serial.begin(9600);
  Serial.println("Meow.");
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
}

void readAccel() {
  xVal = analogRead(xPin);
  yVal = analogRead(yPin);
  zVal = analogRead(zPin);
  
  orientation = "o:" + (String)zVal;
}

void readBatt() {
  battValue = analogRead(battPin) * 2.0; //batt voltage is half what it actually should be, so double the value.
  float tempVolt = battValue / 1023.0; //dividing by 1023...
  theVoltage = tempVolt * vRef; // ...and then multiplying by vRef should give approximate battery voltage.
  //Serial.print(", batt: ");
  //Serial.println(theVoltage);
  //delay(500);
  //Serial.println("");
  if((theVoltage >= vRef) && (theVoltage < vLow) && lowBattInd) {
    lowBatt = true;
  } else {
    //if low voltage is detected any time, it doesn't come out of the lowbatt state.
  }
}

void gpsPower(boolean gpsPwr) {
  if (gpsPwr) {
    digitalWrite(6, HIGH);
    delay(333);
    Serial.println("gps ON");
    Serial.print("Sizeof(gpsobject) = "); Serial.println(sizeof(TinyGPS));
  } else {
    digitalWrite(6, LOW);
    Serial.println("gps OFF");
  }
}
void led() {
  if(lowBatt) {
    ledBTon = 6;
    ledBToff = 666;
  }
  if(ledBlink || lowBatt) {
    if(theTime > lsct) {
      if(ledState) {
        ledState = false;
        lsct = theTime + ledBToff;
      } else {
        ledState = true;
        lsct = theTime + ledBTon;
      }
    }  
  }
  if(ledState) {
    digitalWrite(13, HIGH);
  } else {
    digitalWrite(13, LOW);
  }
}
void interpret(String cmd) {
  //Serial.print("Command Received: ");
  //Serial.println(cmd);
  if(cmd == "blink") {
    if(!ledBlink) {
      //blinkkitty on
      ledBTon = 333;
      ledBToff = 222;
      ledBlink = true;
      Serial.println("blink");
    } else {
      ledBlink = false;
      ledState = ledIS;
      Serial.println("noblink");
    }
  } else if (cmd == "gps") {
    if(!gpsState) {
      gpsPower(true);
      gpsState = true;
    } else {
      gpsPower(false);
      gpsState = false;
    }
  } else if (cmd == "batt") {
    if(!lowBattInd) {
      lowBattInd = true;
      Serial.println("lowbatt indicator ON");
    } else {
      lowBattInd = false;
      lowBatt = false;
      ledState = ledIS;
      Serial.println("lowbatt indicator OFF");
    }
  } else if (cmd == "led") {
    if(ledIS) {
      ledIS = false;
      ledState = ledIS;
      Serial.println("led OFF");
    } else {
      ledIS = true;
      ledState = ledIS;
      Serial.println("led ON");
    }
  } else if (cmd == "sleep") {
    pinging = false;
    Serial.println("sleeping...");
    delay(666);
    sleepCycling = true;
  } else if (cmd == "s") {
    sleepCycling = false;
    Serial.println("sleep OFF");
  } else if (cmd == "ping") {
    if(pinging) {
      pinging = false;
      Serial.println("NOping");
    } else {
      pinging = true;
      Serial.println("pinging");
    }
  } else if (cmd == "pa") {
    if(pA) {
      pA = false;
      Serial.println("pA OFF");
    } else {
      pA = true;
      Serial.println("pA ON");
    }
  } else if (cmd == "ra") {
    if(rA) {
      rA = false;
      digitalWrite(10, LOW); //turns off elsewhere aswell, make sure to check.
      Serial.println("accel OFF");
    } else {
      rA = true;
      digitalWrite(10, HIGH);
      Serial.println("accel ON");
    }
  } else if (cmd == "slow") {
    //ping every five seconds
    pingDelay = 5000;
    Serial.println(pingDelay);
  } else if (cmd == "fast") {
    //ping every second
    pingDelay = 1000;
    Serial.println(pingDelay);
  } else if (cmd == "gpsonly") {
    if(gpsOnly) {
      gpsOnly = false;
      Serial.println("no gpsonly");
    } else {
      gpsOnly = true;
      Serial.println("gpsonly");
    }
  }
  
  clearCMD = true;
  //Serial.println("finCMD");
}
void ping() {
  if(lowBatt) {
    Serial.print("LOWBATT");
  }
  if(!gpsOnly || !gpsState) {
      Serial.print("Meow! ");
      
      Serial.print(theVoltage);
      //Serial.print(", ");
      
      if(ledBlink){
        Serial.print(", blinking");
      } else if (ledIS) {
        Serial.print(", ledON");
      }
      
      if(theTime >= 60000) {
        int aliveTime = theTime / 60000;
        Serial.print(", ");
        Serial.print(aliveTime);
        Serial.print(" M");
      }
      
      if(rA && !pA) {
        Serial.print(", ");
        Serial.print(orientation);
      } else if (pA) {
        Serial.println("");
        Serial.print("x: ");
        Serial.print(xVal);
        Serial.print(", y: ");
        Serial.print(yVal);
        Serial.print(", z: ");
        Serial.print(zVal);
        Serial.println("");
      } 
      
      if(gpsState) {
        Serial.println("");
        gpsdump(gps);
      } else {
        Serial.println("");
      }
  } else {
    gpsdump(gps);
  }      
      
}
void sleepWake() {
  if(sleeping) {
    if(theTime > sleepTime) {
      digitalWrite(5, HIGH); //turn on radio
      digitalWrite(10, HIGH); //turn on accel
      
      delay(666); //wait a moment.
    
      //Serial.println("AWAKE");
      
      ping();
      
      sleeping = false;
      wakeTime = millis() + wakePeriod; //sets the period for which to listen for incoming commands.
    }
  } else {
    //not in sleep mode
    if(theTime > wakeTime) {
      digitalWrite(5, LOW); //turn radio OFF
      digitalWrite(6, LOW); //turn gps OFF
      digitalWrite(10, LOW); //turn accel OFF
      sleepTime = theTime + sleepDuration;
      sleeping = true;
    }
  }
}

void rSer() {
  while(Serial.available() > 0) {
    char c = Serial.read();
    if(c == 0x0D) {
      interpret(iD);  //send char to the interpret function.
    } else {
      iD += (String)c; //incoming data equated to c.
      if(clearCMD) {
        iD = "";
        clearCMD = false;
      }
    }
  }
}

void loop() {
  rSer(); //read incoming serial
  led();
  readBatt();
  theTime = millis();
  
  if(rA && !sleeping) {
    //read accelerometer
    readAccel();
  } else if (!rA) {
    digitalWrite(10, LOW); //turn accel off
  }
  
  if (pinging) {
    if(theTime > pingTime) {
      ping();
      pingTime = theTime + pingDelay;
    }
  } else if (sleepCycling) {
    sleepWake();
  }
  
  if(gpsState) {
    bool newdata = false;
    if (feedgps()) {
      newdata = true;
    }
  }
}



static void gpsdump(TinyGPS &gps)
{
  float flat, flon;
  unsigned long age, date, time, chars = 0;
  unsigned short sentences = 0, failed = 0;
  static const float LONDON_LAT = 51.508131, LONDON_LON = -0.128002;
  
  //print_int(gps.satellites(), TinyGPS::GPS_INVALID_SATELLITES, 5);
  //print_int(gps.hdop(), TinyGPS::GPS_INVALID_HDOP, 5);
  gps.f_get_position(&flat, &flon, &age);
  print_float(flat, TinyGPS::GPS_INVALID_F_ANGLE, 9, 5);
  print_float(flon, TinyGPS::GPS_INVALID_F_ANGLE, 10, 5);
  //print_int(age, TinyGPS::GPS_INVALID_AGE, 5);

  //print_date(gps);

  print_float(gps.f_altitude(), TinyGPS::GPS_INVALID_F_ALTITUDE, 8, 2);
  print_float(gps.f_course(), TinyGPS::GPS_INVALID_F_ANGLE, 7, 2);
  print_float(gps.f_speed_kmph(), TinyGPS::GPS_INVALID_F_SPEED, 6, 2);
  print_str(gps.f_course() == TinyGPS::GPS_INVALID_F_ANGLE ? "*** " : TinyGPS::cardinal(gps.f_course()), 6);
  //print_int(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0UL : (unsigned long)TinyGPS::distance_between(flat, flon, LONDON_LAT, LONDON_LON) / 1000, 0xFFFFFFFF, 9);
  //print_float(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : TinyGPS::course_to(flat, flon, 51.508131, -0.128002), TinyGPS::GPS_INVALID_F_ANGLE, 7, 2);
  //print_str(flat == TinyGPS::GPS_INVALID_F_ANGLE ? "*** " : TinyGPS::cardinal(TinyGPS::course_to(flat, flon, LONDON_LAT, LONDON_LON)), 6);

  //gps.stats(&chars, &sentences, &failed);
  //print_int(chars, 0xFFFFFFFF, 6);
  //print_int(sentences, 0xFFFFFFFF, 10);
  //print_int(failed, 0xFFFFFFFF, 9);
  Serial.println();
}

static void print_int(unsigned long val, unsigned long invalid, int len)
{
  char sz[32];
  if (val == invalid)
    strcpy(sz, "*******");
  else
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  feedgps();
}

static void print_float(float val, float invalid, int len, int prec)
{
  char sz[32];
  if (val == invalid)
  {
    strcpy(sz, "*******");
    sz[len] = 0;
        if (len > 0) 
          sz[len-1] = ' ';
    for (int i=7; i<len; ++i)
        sz[i] = ' ';
    Serial.print(sz);
  }
  else
  {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1);
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      Serial.print(" ");
  }
  feedgps();
}

static void print_date(TinyGPS &gps)
{
  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned long age;
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  if (age == TinyGPS::GPS_INVALID_AGE)
    Serial.print("*******    *******    ");
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d %02d:%02d:%02d   ",
        month, day, year, hour, minute, second);
    Serial.print(sz);
  }
  print_int(age, TinyGPS::GPS_INVALID_AGE, 5);
  feedgps();
}

static void print_str(const char *str, int len)
{
  int slen = strlen(str);
  for (int i=0; i<len; ++i)
    Serial.print(i<slen ? str[i] : ' ');
  feedgps();
}

static bool feedgps()
{
  while (nss.available())
  {
    if (gps.encode(nss.read()))
      return true;
  }
  return false;
}
