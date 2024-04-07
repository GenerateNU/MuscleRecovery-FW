#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <EEPROM.h>
#include "ADS1X15.h"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define DATA_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define OFFLOAD_DATETIME_CHARACTERISTIC_UUID "a5b17d6a-68e5-4f33-abe0-e393e4cd7305"
#define SESSION_START_CHARACTERISTIC_UUID "87ffeadd-3d01-45cd-89bd-ec5a6880c009"
#define OFFLOAD_DATA_CHARACTERISTIC_UUID "f392f003-1c58-4017-9e01-bf89c7eb53bd"
#define OFFLOAD_SESSION_COUNT_UUID "630f3455-b378-4b93-8cf5-79225891f94c"
#define EEPROM_SIZE 512
#define IDENTIFIER_BYTES 4

#define SAMPLES_PER_SEC 860

// DO NOT UNCOMMENT UNLESS YOU WANT TO ERASE EEPROM
//#define CLEAR_EEPROM 

#define SESSION_BYTES 16

#define DATA_BYTES 10

#define DATETIME_BYTES 6

//TODO: set pin
int SESSION_BUTTON = 4;

BLECharacteristic *pData;
BLECharacteristic *pDateTime;
BLECharacteristic *pSessionStart;
BLECharacteristic *pOffloadData;
BLECharacteristic *pNumSessions;
BLEServer *pServer;
bool deviceConnected = false;

bool oneSessionStreamed = true;

unsigned long prvMillis;
int valNotify;
bool eepromCleared = false;

// ADS1115 ADS(0x48);

uint32_t startTime = 0;
uint32_t prevTime = 0;
uint32_t numSamples = 0;
uint32_t sumSample = 0;
uint32_t averagedEMGValue = 0;

uint32_t startSessionCount = 0;

uint8_t numDataSent = 0;

bool offloadedDataBefore = true;

struct data {
  uint8_t muscleData[DATA_BYTES];
  uint8_t dateTime[DATETIME_BYTES]; // Send day, month, year, time
};

data currData;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;

      prvMillis = millis();
      Serial.println("Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected");
      Serial.println("Starting BLE advertising again");
      BLEDevice::startAdvertising();
    }
};

int dataRecorded() {
  uint8_t oneToTwoDigit; // ex 1...99
  uint8_t threeToFourDigit; // ex 100...9900
  uint8_t fiveToSixDigit; // ex 10,000...990,000
  uint8_t sevenToEightDigit; // ex 1,000,000...99,000,000
  EEPROM.get(0, oneToTwoDigit);
  EEPROM.get(1, threeToFourDigit);
  EEPROM.get(2, fiveToSixDigit);
  EEPROM.get(3, sevenToEightDigit);

  return oneToTwoDigit + (100 * threeToFourDigit) + (10000 * fiveToSixDigit) + (1000000 * sevenToEightDigit);
}

void clearEEPROM() {
    for (int i = 0; i < EEPROM_SIZE; i++) {
      EEPROM.put(i, 0);
    }
    EEPROM.put(0, 4); // first four bytes used
    EEPROM.commit();
}


void storeData(data d) {

  // 4 first bytes represent number of bytes used
  uint8_t oneToTwoDigit; // ex 1...99
  uint8_t threeToFourDigit; // ex 100...9900
  uint8_t fiveToSixDigit; // ex 10,000...990,000
  uint8_t sevenToEightDigit; // ex 1,000,000...99,000,000
  EEPROM.get(0, oneToTwoDigit);
  EEPROM.get(1, threeToFourDigit);
  EEPROM.get(2, fiveToSixDigit);
  EEPROM.get(3, sevenToEightDigit);

  if (oneToTwoDigit > 99) {
    threeToFourDigit += oneToTwoDigit / 100;
    oneToTwoDigit = oneToTwoDigit % 100;
  }

  if (threeToFourDigit > 99) {
    fiveToSixDigit += threeToFourDigit / 100;
    threeToFourDigit = threeToFourDigit % 100;
  }

  if (fiveToSixDigit > 99) {
    sevenToEightDigit += fiveToSixDigit / 100;
    fiveToSixDigit = fiveToSixDigit % 100;
  }

  int sizeOfData = sizeof(d);

  uint8_t d_sevenToEightDigit = sizeOfData / 1000000;
  uint8_t d_fiveToSixDigit = (sizeOfData - (d_sevenToEightDigit * 1000000)) / 10000;
  uint8_t d_threeToFourDigit = (sizeOfData - (d_sevenToEightDigit * 1000000) - (d_fiveToSixDigit * 10000)) / 100;
  uint8_t d_oneToTwoDigit = sizeOfData % 100;

  int availableSpace = EEPROM_SIZE - dataRecorded();

  if (availableSpace >= sizeOfData) {
  oneToTwoDigit += d_oneToTwoDigit;
  threeToFourDigit += d_threeToFourDigit;
  fiveToSixDigit += d_fiveToSixDigit;
  sevenToEightDigit += d_sevenToEightDigit;
  
  EEPROM.put(EEPROM_SIZE - availableSpace, d);
  }

  EEPROM.put(0, oneToTwoDigit);
  EEPROM.put(1, threeToFourDigit);
  EEPROM.put(2, fiveToSixDigit);
  EEPROM.put(3, sevenToEightDigit);
  EEPROM.commit();
}


void getAllData(std::vector<data>& allData) {
  for (int address = 4; address < dataRecorded(); address += SESSION_BYTES) {
    data session;
    EEPROM.get(address, session);
    allData.push_back(session);
  }
}

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

  Serial.println("Starting BLE work!");

  BLEDevice::init("Generate ECE Muscle Recovery");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Sends muscle data to client
  pData = pService->createCharacteristic(
                                         DATA_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_NOTIFY
                                       );

  pDateTime = pService->createCharacteristic(
                                         OFFLOAD_DATETIME_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_NOTIFY 
                                       );
  pOffloadData = pService->createCharacteristic(
                                         OFFLOAD_DATA_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_NOTIFY 
                                       );                                   
  pSessionStart = pService->createCharacteristic(
                                         SESSION_START_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_WRITE |
                                         BLECharacteristic::PROPERTY_NOTIFY 
                                       );

  pNumSessions = pService->createCharacteristic(
                                        OFFLOAD_SESSION_COUNT_UUID,
                                        BLECharacteristic::PROPERTY_READ |
                                        BLECharacteristic::PROPERTY_NOTIFY 
                                      );                                   
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("EEPROM size " + String(EEPROM.length()));


  clearEEPROM();
  data data1;
  data data2;
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
  data2.dateTime[0] = 190;
  data2.dateTime[1] = 200;
  data2.dateTime[2] = 210;
  data2.dateTime[3] = 220;
  data2.dateTime[4] = 230;
  data2.dateTime[5] = 240;
  storeData(data1);
  storeData(data2);
  storeData(data1);

  // Serial.println("DATA INITIALLY STORED: " + String(dataRecorded() - 4));

  // for (int i = 0; i < 512; i++) {
  //   uint8_t num;
  //   EEPROM.get(i, num);
  //   Serial.println(num);
  // }
}

// TODO: client side must stop sending after it gets values
void sessionStartedOverBLE() {
  std::string s = pSessionStart->getValue();
  if (s=="yes") {
    Serial.println("Session started via BLE");
    oneSessionStreamed = false;
  }
}

boolean sessionStartedFromButtonPress() {
  return false;//digitalRead(SESSION_BUTTON) == LOW;
}

uint8_t* sessionCountInBytes() {
  uint8_t* sessionCount = new uint8_t[4]; // Allocate memory dynamically
  sessionCount[0] = EEPROM.read(0) - 4;
  sessionCount[1] = EEPROM.read(1);
  sessionCount[2] = EEPROM.read(2);
  sessionCount[3] = EEPROM.read(3);
  return sessionCount;
} 

// Offloads all data stored in flash
void offLoadData() {
  if (!offloadedDataBefore) {
    Serial.println("Offloading saved data");
    std::vector<data> allData;
    getAllData(allData);
    uint8_t* muscleDataOffloadArray = new uint8_t[allData.size() * DATA_BYTES];
    uint8_t* muscleDatetimeOffloadArray = new uint8_t[allData.size() * DATETIME_BYTES];
    for (int dataIndex = 0; dataIndex < allData.size(); dataIndex++) {
      for (int muscleIndex = 0; muscleIndex < DATA_BYTES; muscleIndex++) {
        muscleDataOffloadArray[(dataIndex * DATA_BYTES) + muscleIndex] = allData.at(dataIndex).muscleData[muscleIndex];
      }
      for (int dateIndex = 0; dateIndex < DATETIME_BYTES; dateIndex++) {
        muscleDatetimeOffloadArray[(dataIndex * DATETIME_BYTES) + dateIndex] = allData.at(dataIndex).dateTime[dateIndex];
      }
    }
    pNumSessions->setValue(sessionCountInBytes(), sizeof(sessionCountInBytes()));
    pOffloadData->setValue(muscleDataOffloadArray, DATA_BYTES * allData.size());
    pDateTime->setValue(muscleDatetimeOffloadArray, DATETIME_BYTES * allData.size());
    Serial.println("All " + String(dataRecorded() - 4) + " bytes in EEPROM cleared or " + String((dataRecorded() - 4) / 16) + " sessions.");
    clearEEPROM();
    offloadedDataBefore = true;
  }
}

//TODO: Need to save date time 
void saveData() {
  if (sessionStartedFromButtonPress()) {
    Serial.println("Session started off button press");
    if (numDataSent < SESSION_BYTES) {
      Serial.println("Getting array of data");
      currData.muscleData[numDataSent] = averagedEMGValue;
      numDataSent++;
    }
    else if (numDataSent == SESSION_BYTES) {
      Serial.println("Saving data");
      //storeData(currData);
      numDataSent = 0;
    }
  }
}

// TODO: something weird here
void streamData() {
  sessionStartedOverBLE();
  if (!oneSessionStreamed) {
    // if (numDataSent <= SESSION_BYTES) {
    //   // int t = 3;
    //   // uint8_t data[sizeof(t)];
    //   // memcpy(data, &t, sizeof(t));

    //   // // Set the value of the characteristic
    //   // pData->setValue(data, sizeof(data));
    //   pData->setValue(averagedEMGValue);
    //   numDataSent++;
    // } 
    // else {
    //   Serial.println("One session streamed");
    //   numDataSent = 0;
    //   oneSessionStreamed = true;
    // }
    for (int i = 0; i < 10; i++) {
      pData->setValue(i);
      delay(1000);
    }
    oneSessionStreamed = true;
  }
  
}

void updateReading() {
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

  updateReading();

  // TODO: Display on TFT here

  // TODO: Retrieve date time

  // Sends value using BLE
  if (deviceConnected) {
    offLoadData();
    streamData();
    }
  else {
    saveData();
    offloadedDataBefore = false;
    }
}

//  -- END OF FILE --