// #ifndef BLUETOOTH_H // include guard
// #define BLUETOOTH_H

// #include <BLEDevice.h>
// #include <BLEUtils.h>
// #include <BLEServer.h>
// #include "Storage.hpp"

// #define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
// #define DATA_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
// #define OFFLOAD_DATETIME_CHARACTERISTIC_UUID "a5b17d6a-68e5-4f33-abe0-e393e4cd7305"
// #define SESSION_START_CHARACTERISTIC_UUID "87ffeadd-3d01-45cd-89bd-ec5a6880c009"
// #define OFFLOAD_DATA_CHARACTERISTIC_UUID "f392f003-1c58-4017-9e01-bf89c7eb53bd"
// #define OFFLOAD_SESSION_COUNT_UUID "630f3455-b378-4b93-8cf5-79225891f94c"

// bool deviceConnected = false;

// class Bluetooth {
//     private:
//         /**
//          * Alerts the device that a session has been started over BLE.
//         */
//         void sessionStartedOverBLE();

//     public:
//         /**
//          * Offloads all data in flash, sends over BLE, and clears memory.
//         */
//         void offLoadData(Storage& storage);

//         /**
//          * Streams data over BLE if a session was started over BLE.
//         */
//         void streamData();

// };

// #endif