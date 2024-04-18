#include "ADS1X15.h"
#include "Bluetooth.hpp"
#define EEPROM_SIZE 512
#define IDENTIFIER_BYTES 4

#define SAMPLES_PER_SEC 860

#define SESSION_BYTES 16
#define DATA_BYTES 10
#define DATETIME_BYTES 6

//TODO: set pin
int SESSION_BUTTON = 4;


int valNotify;

Storage storage;
Bluetooth bluetooth;


// ADS1115 ADS(0x48);

uint32_t startTime = 0;
uint32_t prevTime = 0;
uint32_t numSamples = 0;
uint32_t sumSample = 0;
uint32_t averagedEMGValue = 0;

uint32_t startSessionCount = 0;

uint8_t numDataSent = 0;

bool offloadedDataBefore = true;

// Raw data being recorded in present time from a button press
Data currData;



void setup() {
  EEPROM.begin(EEPROM_SIZE);  
  Serial.begin(115200);

  // Wire.begin();

  // ADS.begin();
  // ADS.setGain(0);      //  6.144 volt
  // ADS.setDataRate(7);  //  0 = slow   4 = medium   7 = fast
  // ADS.setMode(0);      //  continuous mode
  // ADS.readADC(0);      //  first read to trigger

  pinMode(SESSION_BUTTON, INPUT);
                                
  Serial.println("EEPROM size " + String(EEPROM.length()));

  storage.clearEEPROM();
  Data data1;
  Data data2;
  data1.muscleData[0] = 0;
  data1.muscleData[1] = 1;
  data1.muscleData[2] = 2;
  data1.muscleData[3] = 0;
  data1.muscleData[4] = 1;
  data1.muscleData[5] = 2;
  data1.muscleData[6] = 0;
  data1.muscleData[7] = 1;
  data1.muscleData[8] = 2;
  data1.muscleData[9] = 3;
  data1.dateTime[0] = 3;
  data1.dateTime[1] = 4;
  data1.dateTime[2] = 5;
  data1.dateTime[3] = 6;
  data1.dateTime[4] = 7;
  data1.dateTime[5] = 8;

  data2.muscleData[0] = 0;
  data2.muscleData[1] = 100;
  data2.muscleData[2] = 110;
  data2.muscleData[3] = 120;
  data2.muscleData[4] = 130;
  data2.muscleData[5] = 140;
  data2.muscleData[6] = 150;
  data2.muscleData[7] = 160;
  data2.muscleData[8] = 170;
  data2.muscleData[9] = 180;
  data2.dateTime[0] = 24;
  data2.dateTime[1] = 4;
  data2.dateTime[2] = 11;
  data2.dateTime[3] = 2;
  data2.dateTime[4] = 27;
  data2.dateTime[5] = 30;
  storage.storeData(data1);
  storage.storeData(data2);

  // Serial.println("DATA INITIALLY STORED: " + String(dataRecorded() - 4));

  // for (int i = 0; i < 512; i++) {
  //   uint8_t num;
  //   EEPROM.get(i, num);
  //   Serial.println(num);
  // }
}


/**
 * Session started from button press.
*/
boolean sessionStartedFromButtonPress() {
  //TODO: Unimplemented, may be difficult due to timing and debounce
  return false;//digitalRead(SESSION_BUTTON) == LOW;
}


/**
 * Saves raw Data from a session started from button press into flash.
*/
// void saveData() {
//   if (sessionStartedFromButtonPress()) {
//     Serial.println("Session started off button press");
//     if (numDataSent < SESSION_BYTES) {
//       Serial.println("Getting array of data");
//       currData.muscleData[numDataSent] = averagedEMGValue;
//       numDataSent++;
//     }
//     else if (numDataSent == SESSION_BYTES) {
//       Serial.println("Saving data");
//       //storeData(currData);
//       numDataSent = 0;
//     }
//   }
// }


/**
 * Sensor reading 860 times per second.
*/
void updateReading() {
  //TODO: Unimplemented
  // Averages out 860 samples per second
  startTime = micros();
  if (startTime - prevTime >= 1160)   //  almost exact 860 SPS
  {
    prevTime = startTime;
    //sumSample += ADS.getValue();  // Must be connected to sensor
    sumSample++;
    numSamples++;
  }

  if (numSamples == SAMPLES_PER_SEC) {
    numSamples = 0;
    averagedEMGValue = sumSample / SAMPLES_PER_SEC;
    sumSample = 0;
  }
}

void loop() {

  //updateReading();

  // TODO: Display on TFT here

  // TODO: Retrieve date time

  // Sends value using BLE
  if (deviceConnected) {
    bluetooth.offLoadData(storage);
    bluetooth.streamData();
    }
  else {
    offloadedDataBefore = false;
    }
}

//  -- END OF FILE --