<<<<<<< HEAD:src/EMG.h
// #ifndef EMG_H
// #define EMG_H
=======
#ifndef EMG_HPP
#define EMG_HPP
>>>>>>> 4c3f4966a824d589c9f0810b5bbb9c3adc3e3be7:src/EMG.hpp

// // Include any necessary headers here
// #include "ADS1X15.h"
// #include <cstdint>

// // Declare any global constants or macros here
// #define DC_OFFSET_SIZE 1.5

// // Declare any global variables here
// float value = 0;
// float prev  = 0;
// uint32_t lastSample = 0;
// uint32_t lastSample2 = 0;
// uint32_t lastTime = 0;

// // Declare any class declarations here
// class LowPassFilter {
// private:
//     double alpha;
//     double lastOutput;
// public:
//     LowPassFilter(double cutoffFreq, double samplingRate)
//     double filter(double inputValue)
// };

<<<<<<< HEAD:src/EMG.h
// // Declare any function prototypes here
// void dataAcquisition();
// float ADStomV(uint16_t newValue);
=======
// Declare any function prototypes here
float ADStomV(uint16_t newValue);
void dataAcquisition();
>>>>>>> 4c3f4966a824d589c9f0810b5bbb9c3adc3e3be7:src/EMG.hpp

// #endif // EMG_H