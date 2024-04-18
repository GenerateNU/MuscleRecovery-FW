#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <EEPROM.h>
#include "ADS1X15.h"
#include <cstdint>
#include <RTClib.h>
#include <SPI.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define DATA_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define OFFLOAD_DATETIME_CHARACTERISTIC_UUID "a5b17d6a-68e5-4f33-abe0-e393e4cd7305"
#define SESSION_START_CHARACTERISTIC_UUID "87ffeadd-3d01-45cd-89bd-ec5a6880c009"
#define OFFLOAD_DATA_CHARACTERISTIC_UUID "f392f003-1c58-4017-9e01-bf89c7eb53bd"
#define OFFLOAD_SESSION_COUNT_UUID "630f3455-b378-4b93-8cf5-79225891f94c"
#define DATETIME_SET_CHARACTERISTIC_UUID "cc7d583a-5c96-4299-8f18-3dde34a6b1d7"
#define PHYSCIAL_SESSION_START_CHARACTERISTIC_UUID "1a742a91-50e5-462e-bf5e-0d1fca990315"
#define EEPROM_SIZE 512
#define IDENTIFIER_BYTES 4

#define SAMPLES_PER_SEC 860

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
BLECharacteristic *pDateTimeSet;
BLECharacteristic *pPhysicalSessionStart;
BLEServer *pServer;
bool deviceConnected = false;

bool oneSessionStreamed = true;

unsigned long prvMillis;
int valNotify;
bool eepromCleared = false;

ADS1115 ADS(0x48);

//RTC_PCF8563 rtc;

uint32_t startTime = 0;
uint32_t prevTime = 0;
uint32_t numSamples = 0;
uint32_t sumSample = 0;
uint32_t averagedEMGValue = 0;

uint32_t startSessionCount = 0;

uint8_t numDataSent = 0;

int numSecondsStreamed = 0;
int indexToInsert = 0;

bool offloadedDataBefore = true;

// Declare any global constants or macros here
#define DC_OFFSET_SIZE 1.5

// Declare any global variables here
float value = 0;
float prev  = 0;
uint32_t lastSample = 0;
uint32_t lastSample2 = 0;
uint32_t lastTime = 0;
double cutoffFreq = 20;
double samplingRate = 860;
double alpha = cutoffFreq / (cutoffFreq + samplingRate);
double lastOutput = 0;
std::vector<float> dataWindow;


enum STATES {
  OFF, WELCOME, PROMPT, STREAM, STORE, SESSION_COMPLETE
};

STATES curr_state;

/**
 * Our data struct that contains the EMG readings and datetime.
*/
struct Data {
  uint8_t muscleData[DATA_BYTES];
  uint8_t dateTime[DATETIME_BYTES]; // Send day, month, year, time
};

// Raw data being recorded in present time from a button press
Data currData;

  // TODO: Display on TFT here

  // TODO: Retrieve date time

  // Sends value using BLE

/**
 * Simple callbacks to determine a BLE connection.
*/
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


// // This function will store the current date time into data struct for spi flash storage
// void storeCurrentDateTime() {
//   DateTime now = rtc.now();
//   currData.dateTime[0] = now.year(); // year
//   currData.dateTime[1] = now.month(); // month
//   currData.dateTime[2] = now.day(); // day
//   currData.dateTime[3] = now.hour(); // hour
//   currData.dateTime[4] = now.minute(); // minute
//   currData.dateTime[5] = now.second(); // second
// }

// // This function will take the BLE characteristic pDateTimeSet and update the RTC
// void setCurrentDateTime() {
//   std::string newDateTime = pDateTimeSet->getValue();
//   rtc.adjust(DateTime((uint8_t) newDateTime[0], (uint8_t) newDateTime[1],
//                       (uint8_t) newDateTime[2], (uint8_t) newDateTime[3], 
//                       (uint8_t) newDateTime[4], (uint8_t) newDateTime[5]));
// }

/**
 * The total data in bytes currently stored in flash memory.
*/
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

/**
 * Clears the entirety of EEPROM and resets the first four identifier bytes.
*/
void clearEEPROM() {
    for (int i = 0; i < EEPROM_SIZE; i++) {
      EEPROM.put(i, 0);
    }
    EEPROM.put(0, 4); // first four bytes used
    EEPROM.commit();
}

/**
 * Stores the given Data in flash.
*/
void storeData(Data d) {

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

/**
 * Places all sessions stored in flash in the given vector of Data.
*/
void getAllData(std::vector<Data>& allData) {
  for (int address = 4; address < dataRecorded(); address += SESSION_BYTES) {
    Data session;
    EEPROM.get(address, session);
    allData.push_back(session);
  }
}

void setup() {
  // Set RTC to 1/1/2000 at midnight
  // rtc.adjust(DateTime(2000, 1, 1, 0, 0, 0));
  // Start rtc

  // rtc.start();
  EEPROM.begin(EEPROM_SIZE);  
  Serial.begin(115200);

  Wire.begin();

  ADS.begin();
  ADS.setGain(0);      //  6.144 volt
  ADS.setDataRate(7);  //  0 = slow   4 = medium   7 = fast
  ADS.setMode(0);      //  continuous mode
  ADS.readADC(0);      //  first read to trigger

  pinMode(SESSION_BUTTON, INPUT);

  Serial.println("Starting BLE work!");

  BLEDevice::init("Generate ECE Muscle Recovery");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // BLE characteristic initializations
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

  pDateTimeSet = pService->createCharacteristic(
                                         DATETIME_SET_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_WRITE |
                                         BLECharacteristic::PROPERTY_NOTIFY 
                                       );       
  pPhysicalSessionStart = pService->createCharacteristic(
                                         PHYSCIAL_SESSION_START_CHARACTERISTIC_UUID,
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
  storeData(data1);
  storeData(data2);

  curr_state = WELCOME;

  // Serial.println("DATA INITIALLY STORED: " + String(dataRecorded() - 4));

  // for (int i = 0; i < 512; i++) {
  //   uint8_t num;
  //   EEPROM.get(i, num);
  //   Serial.println(num);
  // }
}

/**
 * Alerts the device that a session has been started over BLE.
*/
void sessionStartedOverBLE() {
  std::string s = pSessionStart->getValue();
  if (s=="yes") {
    Serial.println("Session started via BLE");
    oneSessionStreamed = false;
  }
}

/**
 * Session started from button press.
*/
boolean sessionStartedFromButtonPress() {
  //TODO: Unimplemented, may be difficult due to timing and debounce
  return false;//digitalRead(SESSION_BUTTON) == LOW;
}

/**
 * Determines the number of sessions in bytes.
*/
uint8_t* sessionCountInBytes() {
  uint8_t* sessionCount = new uint8_t[4]; // Allocate memory dynamically
  sessionCount[0] = EEPROM.read(0) - 4;
  sessionCount[1] = EEPROM.read(1);
  sessionCount[2] = EEPROM.read(2);
  sessionCount[3] = EEPROM.read(3);
  return sessionCount;
} 

/**
 * Offloads all data in flash, sends over BLE, and clears memory.
*/
void offLoadData() {
  if (!offloadedDataBefore) {
    Serial.println("Offloading saved data");
    std::vector<Data> allData;
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

/**
 * Saves raw Data from a session started from button press into flash.
*/
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

double filter(double inputValue) {
    lastOutput = alpha * inputValue + (1 - alpha) * lastOutput;
    return lastOutput;
}

float ADStomV(uint16_t newValue)
{
    // Convert the integer data to voltage.
    float dataPoint = static_cast<float>(newValue) / 17877.0 * 3.3;

    // Check if the value is below the baseline of the DC offset.
    if (dataPoint < DC_OFFSET_SIZE)
    {
        // "Flip" it over the baseline by reflecting it around the DC offset.
        dataPoint = DC_OFFSET_SIZE - (dataPoint - DC_OFFSET_SIZE);
    }
    
    // Subtract DC offset from all points after rectification
    dataPoint -= DC_OFFSET_SIZE;

    return dataPoint;
};

void dataAcquisitionForNoBLE()
{
  uint32_t now = micros();

  // Process EMG data
  if (now - lastSample >= 1160)   //  almost exact 860 SPS
  {
    lastSample = now;
    value = ADS.getValue();       // read value from ADC
    value = ADStomV(value);       // convert value to mV and rectify
    // Serial.print(value);
    prev = filter(value);
    // prev *= 2;

    // add data value to vector
    dataWindow.push_back(prev);
    // adcQueue.addValue(lpf.filter(value));     // add value to queue and filter it
    Serial.print("previous value is: ");
    Serial.println(prev); //value / 2^16 * 3.3
  }

  // every second, average the values in the dataWindow
  if (now - lastSample2 >= 1000000)
  {
    lastSample2 = now;
    // calculate the average of the dataWindow
    Serial.print("every half second, average the values in the dataWindow...");
    float average = 0;
    for (int dataIndex = 0; dataIndex < dataWindow.size(); dataIndex++) {
      average += dataWindow.at(dataIndex);
    }
    int pointToSend = average / 0.01259843;
    Serial.print("average is: ");
    Serial.println(average);
    Serial.print("pointToSend is: ");
    Serial.println(pointToSend);

    //currData.muscleData[numDataSent] = pointToSend;
    // pData->setValue(pointToSend);
    dataWindow.clear();

    if (indexToInsert == 0) {
      // storeCurrentDateTime();
      currData.dateTime[0] = 0;
      currData.dateTime[1] = 0;
      currData.dateTime[2] = 0;
      currData.dateTime[3] = 0;
      currData.dateTime[4] = 0;
      currData.dateTime[5] = 0;
      
    }
    currData.muscleData[indexToInsert] = pointToSend;
    indexToInsert++;
  }
}

void nonBLEStore() {
    if (!oneSessionStreamed) {
      dataAcquisitionForNoBLE();
    if (indexToInsert == DATA_BYTES) {
      oneSessionStreamed = true;
      indexToInsert = 0;
      storeData(currData);
    }
  }
}

void dataAcquisitionForBLE()
{
  uint32_t now = micros();

  // Process EMG data
  if (now - lastSample >= 1160)   //  almost exact 860 SPS
  {
    lastSample = now;
    value = ADS.getValue();       // read value from ADC
    value = ADStomV(value);       // convert value to mV and rectify
    // Serial.print(value);
    prev = filter(value);
    // prev *= 2;

    // add data value to vector
    dataWindow.push_back(prev);
    // adcQueue.addValue(lpf.filter(value));     // add value to queue and filter it
    Serial.print("previous value is: ");
    Serial.println(prev); //value / 2^16 * 3.3
  }

  // every second, average the values in the dataWindow
  if (now - lastSample2 >= 1000000)
  {
    lastSample2 = now;
    // calculate the average of the dataWindow
    Serial.print("every half second, average the values in the dataWindow...");
    float average = 0;
    for (int dataIndex = 0; dataIndex < dataWindow.size(); dataIndex++) {
      average += dataWindow.at(dataIndex);
    }
    int pointToSend = average / 0.01259843;
    Serial.print("average is: ");
    Serial.println(average);
    Serial.print("pointToSend is: ");
    Serial.println(pointToSend);

    //currData.muscleData[numDataSent] = pointToSend;
    // pData->setValue(pointToSend);

    dataWindow.clear();
    pData->setValue(pointToSend);
  }
}


/**
 * Streams data over BLE if a session was started over BLE.
*/
void streamData() {
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
    dataAcquisitionForBLE();
    numSecondsStreamed++;
    if (numSecondsStreamed == DATA_BYTES) {
      oneSessionStreamed = true;
      numSecondsStreamed = 0;
    }
  }
  
}

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

void stateMachine() {
  switch (curr_state) {
    case OFF:
      Serial.println("OFF state");
      // Black Screen
      // Wait for power button press
      delay(1000);
      curr_state = WELCOME;
      break;

    case WELCOME:
      Serial.println("WELCOME state");
      // Welcome Screen
      delay(1000);
      curr_state = PROMPT;
      break;

    case PROMPT:
      Serial.println("PROMPT state");

      if (deviceConnected) {
        curr_state = STREAM;
      }
      else {
        offloadedDataBefore = false;
        curr_state = STORE;
      }
      break;

    case STREAM:
      Serial.println("STREAM state");
      sessionStartedOverBLE();
      offLoadData();
      streamData();
      curr_state = SESSION_COMPLETE;
      break;


    // Check for session button press or over BLE
    case STORE:
      Serial.println("STORE state"); 
      nonBLEStore();
      curr_state = SESSION_COMPLETE;
      break;

    case SESSION_COMPLETE:
      Serial.println("SESSION_COMPLETE state");
      delay(1000);
      curr_state = PROMPT;
      break;

    default:
      break;
  }
}


void loop() {
  stateMachine();

}

//  -- END OF FILE --