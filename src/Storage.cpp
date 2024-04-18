#include "Storage.hpp"

class Storage {

    private:

    public:
    Storage::Storage() {
        
    }

    int Storage::dataRecorded() {
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

    uint8_t* Storage::sessionCountInBytes() {
        uint8_t* sessionCount = new uint8_t[4]; // Allocate memory dynamically
        sessionCount[0] = EEPROM.read(0) - 4;
        sessionCount[1] = EEPROM.read(1);
        sessionCount[2] = EEPROM.read(2);
        sessionCount[3] = EEPROM.read(3);
        return sessionCount;
    } 

    void Storage::clearEEPROM() {
        for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.put(i, 0);
        }
        EEPROM.put(0, 4); // first four bytes used
        EEPROM.commit();
    }

    void Storage::getAllData(std::vector<Data>& allData) {
        for (int address = 4; address < dataRecorded(); address += SESSION_BYTES) {
            Data session;
            EEPROM.get(address, session);
            allData.push_back(session);
        }
    }


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

};