volatile int gpsHit = 0;
int timeLastGPS = 0;
int timeCurAbs = 0;
int timeCurRel = 0;
int pinVal = 127;
int lastPolarity = -1;
int curPolarity = 0;
int frequency = 16;

// Make sure to match these to your physical set-up!
int gpsPin = 2;
int pinNum = 5;

void gpsIn()
{
   gpsHit = 1;
}

int getAnalogValue(int output)
{
    if (output == 0) {
        if (curPolarity != 0) {
            lastPolarity = curPolarity;
            curPolarity = 0;
        }
        return 127;
    }

    if (lastPolarity == 1) {
        curPolarity = -1;
        return 0;
    } else {
        curPolarity = 1;
        return 255;
    }
}

int getDigitalFromMillis(int relMillis)
{
    if (relMillis < 1000/16) {
        return 0;
    }
    float periodMillis = 1000.0 / frequency;
    int nCycles = relMillis / periodMillis;
    float normMillis = relMillis - nCycles * periodMillis;
    if (normMillis < periodMillis / 2) {
        return 0;
    } else {
        return 1;
    }
}

void setup()
{
    pinMode(pinNum, OUTPUT);
    delay(100);
    attachInterrupt(digitalPinToInterrupt(gpsPin),gpsIn,RISING);
}

void loop()
{
    if (gpsHit) {
        timeLastGPS = millis();
        gpsHit = 0;
    }
    timeCurAbs = millis();
    timeCurRel = timeCurAbs - timeLastGPS;
    pinVal = getAnalogValue(getDigitalFromMillis(timeCurRel));
    analogWrite(pinNum, pinVal);
}
