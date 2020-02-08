/*
     SUPER-DUPER COOL ARDUINO BASED MULTICOLOR SOUND PLAYING LIGHTSABER!
   HARDWARE:
     Addressable LED strip (WS2811) to get any blade color and smooth turn on effect
     MicroSD card module to play some sounds
     IMU MPU6050 (accel + gyro) to generate hum. Frequency depends on angle velocity of blade
     OR measure angle speed and play some hum sounds from SD
   CAPABILITIES:
     Smooth turning on/off with lightsaber-like sound effect
     Randomly pulsing color (you can turn it off)
     Sounds:
       MODE 1: generated hum. Frequency depends on angle velocity of blade
       MODE 2: hum sound from SD card
         Slow swing - long hum sound (randomly from 4 sounds)
         Fast swing - short hum sound (randomly from 5 sounds)
       Silent Mode: Press 5 Times when saber is turned off - disable all sounds
     Bright white flash when hitting
     Play one of 16 hit sounds, when hit
       Weak hit - short sound
       Hard hit - long "bzzzghghhdh" sound
   CONTROL BUTTON:
     HOLD - turn on / turn off GyverSaber
     TRIPLE CLICK - change color (red - green - blue - yellow - pink - ice blue)
     QUINARY CLICK - change sound mode (hum generation - hum playing)
     Selected color and sound mode stored in EEPROM (non-volatile memory)
     
   Project GitHub repository: https://github.com/AlexGyver/EnglishProjects/tree/master/GyverSaber
   YouTube channel: https://www.youtube.com/channel/UCNEOyqhGzutj-YS-d5ckYdg?sub_confirmation=1
   Author: MadGyver
*/

// ---------------------------- SETTINGS -------------------------------
//      connect LEDs in Parallel (total 98 LEDs -> too much for memory!)
#define NUM_LEDS 49         // number of microcircuits WS2811 on LED strip (note: one WS2811 controls 3 LEDs!)
#define BTN_TIMEOUT 800     // button hold delay, ms
#define BRIGHTNESS 100       // max LED brightness (0 - 255)
#define NUM_COLORS 6        // 0 - red, 1 - blue, 2 - green, 3 - pink, 4 - yellow, 5 - ice blue, 6 - white

#define SWING_TIMEOUT 500   // timeout between swings
#define SWING_L_THR 150     // swing angle speed threshold
#define SWING_THR 300       // fast swing angle speed threshold
#define STRIKE_THR 150      // hit acceleration threshold
#define STRIKE_S_THR 320    // hard hit acceleration threshold
#define FLASH_DELAY 80      // flash time while hit
#define IDLE_HUM_TIME 8000  // idle or HUM soundfile length (original 9000, adafruit one 8000)
#define SPEAKER_VOLUME 4    // 0..7      TMRpcm library
                            // 5 --> 4.2V Pegel
                            // 4 --> 2.2V
                            // 3 --> 1.1V
                            // 2 --> 0.56V
#define TONE_AC_VOLUME 6    // 0..10
                            // 10 --> 2.2V Pegel
                            //  9 --> 0.4V
                            //  8 --> 0.2V
                            //  7 --> 0.15V
                            //  6 --> 0.125V

#define PULSE_ALLOW 1       // blade pulsation (1 - allow, 0 - disallow)
#define PULSE_AMPL 20       // pulse amplitude
#define PULSE_DELAY 30      // delay between pulses (orig. 30)

#define DEBUG 1             // debug information in Serial (1 - allow, 0 - disallow)
// ---------------------------- SETTINGS -------------------------------

#define LED_PIN 6
#define BTN 3
#define BTN_LED 4
#define SD_ChipSelectPin 8
#define SPEAKER_PIN 9 // toneAC needs Pin 9 and 10

// -------------------------- LIBS ---------------------------
#include <avr/pgmspace.h>   // PROGMAM library
//#include <SD.h>
#include <TMRpcm.h>         // audio from SD library
#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"
#include <toneAC.h>         // hum generation library
#include "FastLED.h"        // addressable LED library
#include <EEPROM.h>

CRGB leds[NUM_LEDS];
TMRpcm tmrpcm;
MPU6050 accelgyro;
// -------------------------- LIBS ---------------------------


// ------------------------------ VARIABLES ---------------------------------
int16_t ax, ay, az;
int16_t gx, gy, gz;
unsigned long ACC, GYR, COMPL;
int gyroX, gyroY, gyroZ, accelX, accelY, accelZ, freq, freq_f = 20;
float k = 0.2;
unsigned long humTimer = -IDLE_HUM_TIME, mpuTimer, nowTimer;
int stopTimer;
boolean bzzz_flag, ls_chg_state, ls_state;
boolean btnState, btn_flag, hold_flag;
boolean silent_mode = false;
byte btn_counter;
unsigned long btn_timer, PULSE_timer, swing_timer, swing_timeout, bzzTimer;
byte nowNumber;
byte nowColor, red, green, blue, redOffset, greenOffset, blueOffset;
boolean eeprom_flag, swing_flag, swing_allow, strike_flag, HUMmode;
int PULSEOffset;
// ------------------------------ VARIABLES ---------------------------------

// --------------------------------- SOUNDS ----------------------------------
const char strike1[] PROGMEM = "SK1.wav";
const char strike2[] PROGMEM = "SK2.wav";
const char strike3[] PROGMEM = "SK3.wav";
const char strike4[] PROGMEM = "SK4.wav";
const char strike5[] PROGMEM = "SK5.wav";
const char strike6[] PROGMEM = "SK6.wav";
const char strike7[] PROGMEM = "SK7.wav";
const char strike8[] PROGMEM = "SK8.wav";

const char* const strikes[] PROGMEM  = {
  strike1, strike2, strike3, strike4, strike5, strike6, strike7, strike8
};

int strike_time[8] = {779, 563, 687, 702, 673, 661, 666, 635};

const char strike_s1[] PROGMEM = "SKS1.wav";
const char strike_s2[] PROGMEM = "SKS2.wav";
const char strike_s3[] PROGMEM = "SKS3.wav";
const char strike_s4[] PROGMEM = "SKS4.wav";
const char strike_s5[] PROGMEM = "SKS5.wav";
const char strike_s6[] PROGMEM = "SKS6.wav";
const char strike_s7[] PROGMEM = "SKS7.wav";
const char strike_s8[] PROGMEM = "SKS8.wav";

const char* const strikes_short[] PROGMEM = {
  strike_s1, strike_s2, strike_s3, strike_s4,
  strike_s5, strike_s6, strike_s7, strike_s8
};
int strike_s_time[8] = {270, 167, 186, 250, 252, 255, 250, 238};

const char swing1[] PROGMEM = "SWS1.wav";
const char swing2[] PROGMEM = "SWS2.wav";
const char swing3[] PROGMEM = "SWS3.wav";
const char swing4[] PROGMEM = "SWS4.wav";
const char swing5[] PROGMEM = "SWS5.wav";

const char* const swings[] PROGMEM  = {
  swing1, swing2, swing3, swing4, swing5
};
int swing_time[8] = {389, 372, 360, 366, 337};

const char swingL1[] PROGMEM = "SWL1.wav";
const char swingL2[] PROGMEM = "SWL2.wav";
const char swingL3[] PROGMEM = "SWL3.wav";
const char swingL4[] PROGMEM = "SWL4.wav";

const char* const swings_L[] PROGMEM  = {
  swingL1, swingL2, swingL3, swingL4
};
int swing_time_L[8] = {636, 441, 772, 702};

char BUFFER[10];
// --------------------------------- SOUNDS ---------------------------------

void setup() {
  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(BRIGHTNESS);  // ~40% of LED strip brightness
  setAll(0, 0, 0);             // and turn it off

  Wire.begin();
  Serial.begin(9600);

  // ---- Pins ----
  pinMode(BTN, INPUT_PULLUP);
  pinMode(BTN_LED, OUTPUT);
  digitalWrite(BTN_LED, 0); // button LED
  // ---- Pins ----

  randomSeed(analogRead(2));    // starting point for random generator

  // IMU initialization
  accelgyro.initialize();
  accelgyro.setFullScaleAccelRange(MPU6050_ACCEL_FS_16);
  accelgyro.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
  if (DEBUG) {
    if (accelgyro.testConnection()) Serial.println(F("MPU6050 OK"));
    else Serial.println(F("MPU6050 fail"));
  }

  // SD initialization
  tmrpcm.speakerPin = SPEAKER_PIN;
  tmrpcm.setVolume(SPEAKER_VOLUME); // 0..7
  tmrpcm.quality(2);   // 1 or 2 oversampling
  if (DEBUG) {
    if (SD.begin(SD_ChipSelectPin)) Serial.println(F("SD OK"));
    else Serial.println(F("SD fail"));
  } else {
    SD.begin(SD_ChipSelectPin);
  }

  if ((EEPROM.read(0) >= 0) && (EEPROM.read(0) <= 5)) {  // check first start
    nowColor = EEPROM.read(0);   // remember color
    HUMmode = EEPROM.read(1);    // remember mode
  } else {                       // first start
    EEPROM.write(0, 0);          // set default
    EEPROM.write(1, 0);          // set default
    nowColor = 0;                // set default
  }

  setColor(nowColor);
  if (DEBUG) Serial.println(F("Blade ready"));
  digitalWrite(BTN_LED, 1); // turn button LED on
}

// --- MAIN LOOP---
void loop() {
  randomPULSE();
  getFreq();
  on_off_sound();
  btnTick();
  strikeTick();
  if (!silent_mode) swingTick();
}
// --- MAIN LOOP---


void btnTick() {
  btnState = !digitalRead(BTN);
  if (btnState && !btn_flag) {
    if (DEBUG) Serial.println(F("BTN PRESS"));
    btn_flag = 1;
    btn_counter++;
    btn_timer = millis();
  }
  if (!btnState && btn_flag) {
    btn_flag = 0;
    hold_flag = 0;
  }
  // hold to turn saber on
  if (btn_flag && btnState && (millis() - btn_timer > BTN_TIMEOUT) && !hold_flag) {
    ls_chg_state = 1;                     // flag to change saber state (on/off)
    hold_flag = 1;
    btn_counter = 0;
  }
  // press multiple times
  if ((millis() - btn_timer > BTN_TIMEOUT) && (btn_counter != 0)) {
    if (ls_state) {                    // only if blade is turned on
      if (btn_counter == 3) {               // 3 press count
        nowColor++;                         // change color
        if (nowColor >= NUM_COLORS+1) nowColor = 0;
        setColor(nowColor);
        setAll(red, green, blue);
        eeprom_flag = 1;
      }
      if (btn_counter == 5 && (!silent_mode)) {               // 5 press count
        HUMmode = !HUMmode;
        if (HUMmode) {
          if (DEBUG) Serial.println(F("HUM Soundfile"));
          noToneAC();
          tmrpcm.play("HUM.wav");
        } else {
          if (DEBUG) Serial.println(F("Tone AC"));
          tmrpcm.disable();
          toneAC(freq_f, TONE_AC_VOLUME); // frequency, volume 0..10
        }
        eeprom_flag = 1;
      }
    }
    if (!ls_state) {  // only if blade is turned off
      if (btn_counter == 5) {               // 5 press count --> silent mode   
          if (DEBUG) Serial.println(F("Silent Mode"));
          silent_mode = !silent_mode;
          // visual feedback
          digitalWrite(BTN_LED, 0); // turn button LED off
          delay(250);
          digitalWrite(BTN_LED, 1); // turn button LED on
          delay(250);
          digitalWrite(BTN_LED, 0); // turn button LED off
          delay(250);
          digitalWrite(BTN_LED, 1); // turn button LED on
      }
    }
    btn_counter = 0;
  }
}

void on_off_sound() {
  if (ls_chg_state) {                // if change flag
    if (!ls_state) {                 // if GyverSaber is turned on
        if (DEBUG) Serial.println(F("SABER ON"));
        if (!silent_mode) {
            if (nowColor == 0) {
              tmrpcm.play("kyloON.wav");
            }
            else {
              tmrpcm.play("ON.wav");
            }
        }
        delay(200);
        light_up();
        delay(200);
        bzzz_flag = 1;
        ls_state = true;               // remember that turned on
        if (HUMmode) {
          noToneAC();
          if (!silent_mode) {
            if (nowColor == 0) {
              tmrpcm.play("kyloHUM.wav");
            }
            else {
              tmrpcm.play("HUM.wav");
            }
          }
        } else {
          tmrpcm.disable();
          if (!silent_mode) toneAC(freq_f, TONE_AC_VOLUME); // frequency, volume 0..10
        }
        
    } else {                         // if GyverSaber is turned off
      noToneAC();
      bzzz_flag = 0;
      if (!silent_mode) {
        if (nowColor == 0) {
          tmrpcm.play("kyloOFF.wav");
        }
        else {
          tmrpcm.play("OFF.wav");
        }
      }
      delay(300);
      light_down();
      delay(300);
      tmrpcm.disable();
      if (DEBUG) Serial.println(F("SABER OFF"));
      ls_state = false;
      if (eeprom_flag) {
        eeprom_flag = 0;
        EEPROM.write(0, nowColor);   // write color in EEPROM
        EEPROM.write(1, HUMmode);    // write mode in EEPROM
      }
    }
    ls_chg_state = 0;
  }

  // keep playing the idle HUM sound
  if (((millis() - humTimer) > IDLE_HUM_TIME) && bzzz_flag && HUMmode) {           // adjust timer for audio file length
    if (ls_state && !silent_mode) {
      if (nowColor == 0) {
        tmrpcm.play("kyloHUM.wav");
        }
      else {
        tmrpcm.play("HUM.wav");
      }
    }
    humTimer = millis();
    swing_flag = 1;
    strike_flag = 0;
  }
  long delta = millis() - bzzTimer;
  if ((delta > 3) && bzzz_flag && !HUMmode) {
    if (strike_flag) {
      tmrpcm.disable();
      strike_flag = 0;
    }
    if (!silent_mode) toneAC(freq_f, TONE_AC_VOLUME); // frequency, volume 0..10;
    bzzTimer = millis();
  }
}

void randomPULSE() {
  if (PULSE_ALLOW && ls_state && (millis() - PULSE_timer > PULSE_DELAY)) {
    PULSE_timer = millis();
    PULSEOffset = PULSEOffset * k + random(-PULSE_AMPL, PULSE_AMPL) * (1 - k);  // k = 0.2, PULSE_AMPL = 20
    if (nowColor == 0) PULSEOffset = constrain(PULSEOffset, -30, 5);            // ??? bei rot andere Limits
    redOffset = constrain(red + PULSEOffset, 0, 255);
    greenOffset = constrain(green + PULSEOffset, 0, 255);
    blueOffset = constrain(blue + PULSEOffset, 0, 255);
    setAll(redOffset, greenOffset, blueOffset);
  }
}

void strikeTick() {
  if ((ACC > STRIKE_THR) && (ACC < STRIKE_S_THR)) {
    if (!HUMmode) noToneAC();
    nowNumber = random(8);
    // читаем название трека из PROGMEM
    strcpy_P(BUFFER, (char*)pgm_read_word(&(strikes_short[nowNumber])));
    if (!silent_mode) tmrpcm.play(BUFFER);
    hit_flash();
    if (!HUMmode)
      bzzTimer = millis() + strike_s_time[nowNumber] - FLASH_DELAY;
    else
      humTimer = millis() - IDLE_HUM_TIME + strike_s_time[nowNumber] - FLASH_DELAY;
    strike_flag = 1;
  }
  if (ACC >= STRIKE_S_THR) {
    if (!HUMmode) noToneAC();
    nowNumber = random(8);
    // читаем название трека из PROGMEM
    strcpy_P(BUFFER, (char*)pgm_read_word(&(strikes[nowNumber])));
    if (!silent_mode) tmrpcm.play(BUFFER);
    hit_flash();
    if (!HUMmode)
      bzzTimer = millis() + strike_time[nowNumber] - FLASH_DELAY;
    else
      humTimer = millis() - IDLE_HUM_TIME + strike_time[nowNumber] - FLASH_DELAY;
    strike_flag = 1;
  }
}

void swingTick() {
  if (GYR > 80 && (millis() - swing_timeout > 100) && HUMmode) {
    swing_timeout = millis();
    if (((millis() - swing_timer) > SWING_TIMEOUT) && swing_flag && !strike_flag) {
      if (GYR >= SWING_THR) {      
        nowNumber = random(5);          
        // читаем название трека из PROGMEM
        strcpy_P(BUFFER, (char*)pgm_read_word(&(swings[nowNumber])));
        tmrpcm.play(BUFFER);               
        humTimer = millis() - IDLE_HUM_TIME + swing_time[nowNumber];
        swing_flag = 0;
        swing_timer = millis();
        swing_allow = 0;
      }
      if ((GYR > SWING_L_THR) && (GYR < SWING_THR)) {
        nowNumber = random(5);            
        // читаем название трека из PROGMEM
        strcpy_P(BUFFER, (char*)pgm_read_word(&(swings_L[nowNumber])));
        tmrpcm.play(BUFFER);              
        humTimer = millis() - IDLE_HUM_TIME + swing_time_L[nowNumber];
        swing_flag = 0;
        swing_timer = millis();
        swing_allow = 0;
      }
    }
  }
}

void getFreq() {
  if (ls_state) {                                               // if GyverSaber is on
    if (millis() - mpuTimer > 500) {                            
      accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);       

      // find absolute and divide on 100
      gyroX = abs(gx / 100);
      gyroY = abs(gy / 100);
      gyroZ = abs(gz / 100);
      accelX = abs(ax / 100);
      accelY = abs(ay / 100);
      accelZ = abs(az / 100);

      // vector sum
      ACC = sq((long)accelX) + sq((long)accelY) + sq((long)accelZ);
      ACC = sqrt(ACC);
      GYR = sq((long)gyroX) + sq((long)gyroY) + sq((long)gyroZ);
      GYR = sqrt((long)GYR);
      COMPL = ACC + GYR;
      /*
         // отладка работы IMU
         Serial.print("$");
         Serial.print(gyroX);
         Serial.print(" ");
         Serial.print(gyroY);
         Serial.print(" ");
         Serial.print(gyroZ);
         Serial.println(";");
      */
      freq = (long)COMPL * COMPL / 1500;                        // parabolic tone change
      freq = constrain(freq, 18, 300);                          
      freq_f = freq * k + freq_f * (1 - k);                     // smooth filter
      mpuTimer = micros();                                     
    }
  }
}

void setPixel(int Pixel, byte red, byte green, byte blue) {
  leds[Pixel].r = red;
  leds[Pixel].g = green;
  leds[Pixel].b = blue;
}

void setAll(byte red, byte green, byte blue) {
  for (int i = 0; i < NUM_LEDS; i++ ) {
    setPixel(i, red, green, blue);
  }
  FastLED.show();
}

void light_up() {
  for (char i = 0; i <= (NUM_LEDS - 1); i++) {        
    setPixel(i, red, green, blue);
    FastLED.show();
    delay(20);
  }
}

void light_down() {
  for (char i = (NUM_LEDS - 1); i >= 0; i--) {      
    setPixel(i, 0, 0, 0);
    FastLED.show();
    delay(20);
  }
}

void hit_flash() {
  setAll(255, 255, 255); // white flash           
  delay(FLASH_DELAY);                
  setAll(red, blue, green);        
}

void setColor(byte color) {
  switch (color) {
    // 0 - red, 1 - blue, 2 - green, 3 - pink, 4 - yellow, 5 - ice blue, 6 - white
    case 0:
      red = 255;
      green = 0;
      blue = 0;
      break;
    case 1:
      red = 0;
      green = 0;
      blue = 255;
      break;
    case 2:
      red = 0;
      green = 255;
      blue = 0;
      break;
    case 3:
      red = 255;
      green = 0;
      blue = 255;
      break;
    case 4:
      red = 255;
      green = 255;
      blue = 0;
      break;
    case 5:
      red = 0;
      green = 255;
      blue = 255;
      break;
    case 6:
      red = 255;
      green = 255;
      blue = 255;
      break;
  }
}
