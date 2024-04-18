#include <EEPROM.h>
#include <stdint.h>
#include <vector>
#include <Arduino.h>

#define SESSION_BYTES 16
#define DATA_BYTES 10
#define DATETIME_BYTES 6
#define EEPROM_SIZE 512


/**
 * Our data struct that contains the EMG readings and datetime.
*/
struct Data {
  uint8_t muscleData[DATA_BYTES];
  uint8_t dateTime[DATETIME_BYTES]; // Send day, month, year, time
};

class Storage {

    private:

    public:
        Storage();

        /**
        * Clears the entirety of EEPROM and resets the first four identifier bytes.
        */
        int dataRecorded();

        /**
         * Determines the number of sessions in bytes.
        */
        uint8_t* sessionCountInBytes();

        /**
        * Clears the entirety of EEPROM and resets the first four identifier bytes.
        */
        void clearEEPROM();

        /**
        * Places all sessions stored in flash in the given vector of Data.
        */
        void getAllData(std::vector<Data>& allData);

        /**
        * Stores the given Data in flash.
        */
        void storeData(Data d);

};