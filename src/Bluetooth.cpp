#include "Bluetooth.hpp"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected");
      Serial.println("Starting BLE advertising again");
      BLEDevice::startAdvertising();
    }
};

class Bluetooth {
    private:
        BLEServer *pServer;
        BLEService *pService;
        BLECharacteristic *pData;
        BLECharacteristic *pDateTime;
        BLECharacteristic *pSessionStart;
        BLECharacteristic *pOffloadData;
        BLECharacteristic *pNumSessions;
        BLEAdvertising *pAdvertising;
        bool offloadedDataBefore = true;
        bool oneSessionStreamed = true;

        void Bluetooth::sessionStartedOverBLE() {
            std::string s = pSessionStart->getValue();
            if (s=="yes") {
                Serial.println("Session started via BLE");
                oneSessionStreamed = false;
            }
        }

    public:
        Bluetooth::Bluetooth() {
            Serial.println("Starting BLE work!");
            BLEDevice::init("Generate ECE Muscle Recovery");
            pServer = BLEDevice::createServer();
            pServer->setCallbacks(new MyServerCallbacks());
            pServer->setCallbacks(new MyServerCallbacks());
            pService = pServer->createService(SERVICE_UUID);
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
            pAdvertising = BLEDevice::getAdvertising();
            pAdvertising->addServiceUUID(SERVICE_UUID);
            pAdvertising->setScanResponse(true);
            pAdvertising->setMinPreferred(0x06);
            pAdvertising->setMinPreferred(0x12);
            BLEDevice::startAdvertising();
        }

        void Bluetooth::offLoadData(Storage& storage) {
            if (!offloadedDataBefore) {
                Serial.println("Offloading saved data");
                std::vector<Data> allData;
                storage.getAllData(allData);
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
                pNumSessions->setValue(storage.sessionCountInBytes(), sizeof(storage.sessionCountInBytes()));
                pOffloadData->setValue(muscleDataOffloadArray, DATA_BYTES * allData.size());
                pDateTime->setValue(muscleDatetimeOffloadArray, DATETIME_BYTES * allData.size());
                Serial.println("All " + String(storage.dataRecorded() - 4) + " bytes in EEPROM cleared or " +
                String((storage.dataRecorded() - 4) / 16) + " sessions.");
                storage.clearEEPROM();
                offloadedDataBefore = true;
            }
        }

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
};