#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <EEPROM.h>
#include "ADS1X15.h"
#include <cstdint>
#include <RTClib.h>

//imports for tft screen 
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

//font imports for tft screen
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

//REV 2 PCB PINOUT
#define TFT_CS 4
#define TFT_RST 15
#define TFT_DC 2

//button pinouts 
#define POWER_BUTTON_PIN 26
#define ACTION_BUTTON_PIN 35 //need to change later 

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

// For 1.14", 1.3", 1.54", 1.69", and 2.0" TFT with ST7789:
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

//TFT SCREEN CONSTANTS
const int screenWidth = tft.width(); 
const int screenHeight = tft.height(); 
const int centerX = tft.width() / 2;
const int centerY = tft.height() / 2;

//for starting a session timer
unsigned long sessionStartTime;

//boolenas for power on and off might not need
bool isPowerOn; 
bool isSessionActive = false; //might not need !!!

int powerState;            // the current reading from the input pin
int lastPowerState = LOW;  // the previous reading from the input pin
unsigned long lastPowerDebounceTime = 0;  // the last time the output pin was toggledc


int sessionState;            // the current reading from the input pin
int lastSessionState = LOW;  // the previous reading from the input pin
unsigned long lastSessionDebounceTime = 0;  // the last time the output pin was toggled

const unsigned long debounceDelay = 50; // Debounce time in milliseconds


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

RTC_PCF8563 rtc;

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

// This function will take the BLE characteristic pDateTimeSet and update the RTC
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
  //rtc.adjust(DateTime(2000, 1, 1, 0, 0, 0));
  // Start rtc

  //rtc.start();
  EEPROM.begin(EEPROM_SIZE);  
  Serial.begin(115200);

  Wire.begin();

  ADS.begin();
  ADS.setGain(0);      //  6.144 volt
  ADS.setDataRate(7);  //  0 = slow   4 = medium   7 = fast
  ADS.setMode(0);      //  continuous mode
  ADS.readADC(0);      //  first read to trigger

  pinMode(POWER_BUTTON_PIN, INPUT);
  pinMode(ACTION_BUTTON_PIN, INPUT);

    // use this initializer if using a 1.3" or 1.54" 240x240 TFT:
  tft.init(240, 240);           // Init ST7789 240x240
  tft.fillScreen(ST77XX_BLACK);

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

  isPowerOn = false;
  curr_state = OFF;

  // Serial.println("DATA INITIALLY STORED: " + String(dataRecorded() - 4));

  // for (int i = 0; i < 512; i++) {
  //   uint8_t num;
  //   EEPROM.get(i, num);
  //   Serial.println(num);
  // }
}

//TFT SCREEN CODE

//draw arrow helper method
void drawArrow() {

  int base = 50;
  int height = 150;
  
  int x = centerX + 60;
  int y = centerY - (height / 4 );

  
  // Coordinates for a right-pointing arrow
  int x0 = x - base / 2;
  int y0 = y - height / 2;
  int x1 = x - base / 2;
  int y1 = y + height / 2;
  int x2 = x + base / 2;
  int y2 = y;

// char buffer[256]; // Ensure the buffer is large enough to hold the entire string
//sprintf(buffer, "x0: %d y0: %d x1: %d y1: %d x2: %d y2: %d", x0, y0, x1, y1, x2, y2);
//Serial.println(buffer);

  tft.fillTriangle(x0, y0, x1, y1, x2, y2, ST77XX_RED);
  tft.fillTriangle(x0, y0 , x1, y1 , x2 - 20, y2, ST77XX_BLACK);
}


//center text helper method
void centerText(String text, int numLines, int x, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  int lineHeight = 30;
  int totalHeight = lineHeight * numLines;
  int currentY = y - (totalHeight / 2);
  tft.setFont(&FreeMonoBold12pt7b);
  
  for (int i = 0; i < numLines; i++) {
    int newLinePos = text.indexOf('\n');
    String line = (newLinePos != -1) ? text.substring(0, newLinePos) : text;
    text = (newLinePos != -1) ? text.substring(newLinePos + 1) : "";
    tft.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(x - w / 2, currentY);
    tft.print(line);
    currentY += lineHeight;
  }
}

void textScreen(String text) {
  tft.fillScreen(ST77XX_BLACK); // Clear the screen and set the background to black
  
  tft.setFont(&FreeMonoBold18pt7b); 
  tft.setTextColor(ST77XX_WHITE); // Set the text color to white
  tft.setTextSize(1); // Set the text size

 int splitPosition = text.indexOf(' ');
  if (splitPosition != -1) {
    String firstPart = text.substring(0, splitPosition);
    String secondPart = text.substring(splitPosition + 1);

    // Calculate the width and height of the first part
    int16_t x1, y1;
    uint16_t w1, h1;
    tft.getTextBounds(firstPart, 0, 0, &x1, &y1, &w1, &h1);

    // Calculate the width and height of the second part
    int16_t x2, y2;
    uint16_t w2, h2;
    tft.getTextBounds(secondPart, 0, 0, &x2, &y2, &w2, &h2);

    // Calculate the total height to center the block vertically
    int totalHeight = h1 + h2 ;

    // Set cursor to print the first part centered horizontally and vertically
    tft.setCursor(centerX - w1/2 , centerY - totalHeight/2 - h1 - 15 );
    tft.println(firstPart);

    // Set cursor to print the second part below the first and centered horizontally
    tft.setCursor(centerX - w2/2 , centerY );
    tft.println(secondPart);
  } else {
    // Handle case where there is no space in the string
    int16_t x, y;
    uint16_t w, h;
    tft.getTextBounds(text, 0, 0, &x, &y, &w, &h);

    tft.setCursor(centerX - w/2, centerY - h/2);
    tft.println(text);
  }
}

void POWEROFF() {
  textScreen("Powering Off."); //NEED TO MOVE
   
  tft.fillScreen(ST77XX_BLACK);
}

void welcomeScreen() {
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setFont(&FreeMonoBold18pt7b); // Using a large font for better readability
  tft.setTextColor(ST77XX_WHITE); // Setting text color to white
  tft.setTextSize(1);

  int charWidth = 20; 
  int xWelcome = (screenWidth - (7 * charWidth)) / 2 ; // "Welcome" has 7 characters
  int xTo = (screenWidth - (2 * charWidth)) / 2; // "to" has 2 characters
  int xMuscle = (screenWidth - (6 * charWidth)) / 2; // "Muscle" has 6 characters
  int xRecovery = (screenWidth - (8 * charWidth)) / 2; // "Recovery!" has 9 characters

  int start = 70;
  int offset = 40;
  tft.setCursor(xWelcome, start);
  tft.println(F("Welcome"));
  tft.setCursor(xTo, start + offset);
  tft.println(F("to"));
  tft.setCursor(xMuscle, start + (offset*2));
  tft.println(F("Muscle"));
  tft.setCursor(xRecovery, start + (offset*3));
  tft.println(F("Recovery"));

  delay(4000);
}

void promptStart() {
  tft.fillScreen(ST77XX_BLACK); // Fill background with blue color

  // Set text properties
  tft.setTextColor(ST77XX_WHITE); 
  tft.setFont(&FreeMonoBold12pt7b);
  tft.setTextSize(1);  

  // Center text block
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2;
  String text = "Press\nButton\nTo\nStart\nSession";

  centerText(text, 5, centerX - 45, centerY + 20); // Adjust Y position as needed


  drawArrow();
}

void updateTimeDisplay() {
  tft.setFont(NULL);
  unsigned long currentTime = millis();
  unsigned long elapsedSeconds = (currentTime - startTime) / 1000; // Convert milliseconds to seconds

  
  unsigned long minutes = elapsedSeconds / 60; // Convert seconds to minutes
  unsigned long seconds = elapsedSeconds % 60; // Correct the seconds to be within the range of 0-59
  
  
   // Create time string for display
  char timeString[6];
  sprintf(timeString, "%02lu:%02lu", minutes, seconds);

  // Get width and height of the time string with current text size
  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(3);
  tft.getTextBounds(timeString, 0, 0, &x1, &y1, &w, &h);

  // Clear the previous time display
  tft.fillRect((tft.width() - w) / 2, tft.height() - h - 10, w, h, ST77XX_BLACK);

  // Set cursor position to center the time at the bottom
  tft.setCursor((tft.width() - w) / 2, tft.height() - h - 10);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  // Draw new time
  tft.print(timeString);
}

//NEED TO EDIT THIS 
void updateDisplay(float measurement) {
  

  
  tft.setFont(NULL);
  tft.fillScreen(ST77XX_BLACK); // Clear the screen
  
 // Draw the measurement centered on the screen
  char measureBuffer[10]; // Buffer to hold measurement string
  dtostrf(measurement, 0, 2, measureBuffer); // Convert float to string
  tft.setTextSize(5);

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(measureBuffer, 0, 0, &x1, &y1, &w, &h); // Calculate bounds of measurement string
  tft.setCursor((tft.width() - w) / 2, (tft.height() - h) / 2); // Set cursor position to center the measurement
  tft.print(measureBuffer);

  tft.setTextSize(3); 
  tft.getTextBounds(".00", 0, 0, &x1, &y1, &w, &h); // Calculate bounds of measurement string
  tft.setCursor((tft.width()/2) + w, (tft.height() + h) / 2); // Set cursor position to center the measurement
  tft.print("mV");

  //NOT sure where this is happening 
  updateTimeDisplay();
  
}




//NOT BEING USED FOR SHOWCASE
void promptDecision() {
  tft.fillScreen(ST77XX_BLACK); // Fill background with blue color

  // Set text properties
  tft.setTextColor(ST77XX_WHITE); 
  tft.setFont(&FreeMonoBold12pt7b); 
  tft.setTextSize(1); 

  // Center text block
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2;
  String text = "Press\nTo Save\nHold To\nDelete";


  centerText(text, 4, centerX - 45, centerY + 20); // Adjust Y position as needed

 // Draw right-pointing arrow
  drawArrow(); 

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

    // if (indexToInsert == 0) {
    //   storeCurrentDateTime();
    // }
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

void checkPowerState() {

  
   // read the state of the switch into a local variable:
  int powerReading = digitalRead(POWER_BUTTON_PIN);

  
  // If the switch changed, due to noise or pressing:
  if (powerReading != lastPowerState) {
    // reset the debouncing timer
    lastPowerDebounceTime = millis();
  }

if ((millis() - lastPowerDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (powerReading != powerState) {
      powerState = powerReading;


      // only toggle the LED if the new button state is HIGH
      if (powerState == HIGH) {
        
        Serial.println("Power Button Pressed");
    
        isPowerOn = !isPowerOn;

        if (isPowerOn) {
          curr_state = WELCOME;
        } else 
        {
          curr_state = OFF;
        }

        Serial.println(isPowerOn? "Powered On" : "Powered Off");


    
      }
    }
  }
  lastPowerState = powerReading;

}

void stateMachine() {

  checkPowerState();
   
  switch (curr_state) {
    case OFF:
      Serial.println("OFF state");
      
      //TFT SCREEN
      POWEROFF();
      
 
      break;

    case WELCOME:
      Serial.println("WELCOME state");
      
      //TFT SCREEN
      welcomeScreen();

 
      curr_state = PROMPT;

   

      break;

    case PROMPT:
      Serial.println("PROMPT state");

      //TFT SCREEN
      promptStart();
      //doesn't account for button press yet 

      if (deviceConnected) {
        curr_state = STREAM;

        //STARTING TIMER 
        sessionStartTime = millis();
      }
      else {
        offloadedDataBefore = false;
        //curr_state = STORE;

        //should wait for a button press here 
      }

      break;

    case STREAM:

      updateDisplay(3.14);

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