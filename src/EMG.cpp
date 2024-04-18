// #include "ADS1X15.h"
// #include <cstdint>
// #include <vector>

// // Declare any global constants or macros here
// #define DC_OFFSET_SIZE 1.5

// // Declare any global variables here
// float value = 0;
// float prev  = 0;
// uint32_t lastSample = 0;
// uint32_t lastSample2 = 0;
// uint32_t lastTime = 0;
// double cutoffFreq = 20;
// double samplingRate = 860;
// double alpha = cutoffFreq / (cutoffFreq + samplingRate);
// double lastOutput = 0;

// std::vector<float> dataWindow;

// double filter(double inputValue) {
//     lastOutput = alpha * inputValue + (1 - alpha) * lastOutput;
//     return lastOutput;
// }

// float ADStomV(uint16_t newValue)
// {
//     // Convert the integer data to voltage.
//     float dataPoint = static_cast<float>(newValue) / 17877.0 * 3.3;

//     // Check if the value is below the baseline of the DC offset.
//     if (dataPoint < DC_OFFSET_SIZE)
//     {
//         // "Flip" it over the baseline by reflecting it around the DC offset.
//         dataPoint = DC_OFFSET_SIZE - (dataPoint - DC_OFFSET_SIZE);
//     }
    
//     // Subtract DC offset from all points after rectification
//     dataPoint -= DC_OFFSET_SIZE;

//     return dataPoint;
// };

// void dataAcquisition()
// {
//   uint32_t now = micros();

//   // Process EMG data
//   if (now - lastSample >= 1160)   //  almost exact 860 SPS
//   {
//     lastSample = now;
//     value = ADS.getValue();       // read value from ADC
//     value = ADStomV(value);       // convert value to mV and rectify
//     // Serial.print(value);
//     prev = lpf.filter(value);
//     // prev *= 2;

//     // add data value to vector
//     dataWindow.push_back(prev);
//     // adcQueue.addValue(lpf.filter(value));     // add value to queue and filter it
//     Serial.print("previous value is: ");
//     Serial.println(prev); //value / 2^16 * 3.3
//   }

//   // every second, average the values in the dataWindow
//   if (now - lastSample2 >= 1000000)
//   {
//     lastSample2 = now;
//     // calculate the average of the dataWindow
//     Serial.print("every half second, average the values in the dataWindow...");
//     float average = std::accumulate(dataWindow.begin(), dataWindow.end(), 0.0) / dataWindow.size();
//     uint8_t pointToSend = average / 0.01259843;
//     Serial.print("average is: ");
//     Serial.println(average);
//     Serial.print("pointToSend is: ");
//     Serial.println(pointToSend);

//     // currData.muscleData[numDataSent] = pointToSend;
//     pData->setValue(static_cast<std::string>(pointToSend));

//     dataWindow.clear();
//   }
// }