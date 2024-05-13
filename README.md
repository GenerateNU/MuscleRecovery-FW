## Muscle Recovery Firmware
The Muscle Recovery Wearable acts as a peripheral/server that is capable of both storing session data and streaming data to a BLE (Bluetooth Low Energy) connectable device. The C++ Bare Metal development of the device gives the user the option to use the Muscle Recovery Wearable as either a standalone device, connected device, or a mixture of both. This gives the user more flexibility to use the device as they deem fit and prevents the extra Bluetooth capability of the device from standing in the way of its primary purpose.

Because of the microcontroller’s rather minimal input peripherals (the EMG sensor and a couple push buttons), we opted for Bare Metal Programming to keep our firmware simple and sequential. Figure X. goes into detail of the exact steps that occur throughout the program’s lifetime. A state machine exists in the void loop that loops between seven states: OFF, WELCOME, PROMPT, STREAM, STORE, SESSION_COMPLETE. 

The blue boxes in the flowchart are the only required states that the Muscle Recovery must step through. On power up, the device must display the welcome screen and cycle through the PROMPT, STORE, and SESSION_COMPLETE states, constantly checking for a bluetooth connection, power button press, and start session button press. 

When the start button is pressed, the device begins to sample the raw signal from the EMG sensor 860 times a second, rectifies it, and converts it to an average mV value as a uint8, which is reflected on the TFT screen. Each second of data is stored in a uint8 array and stored into flash memory if the device is not connected to BLE (STORE state). 

On a BLE connection (STREAM state), the program follows a different path of procedures. First, the device receives a datetime from the client side via a BLE characteristic, which only has to happen once whenever the battery is completely drained. This datetime, combined with the RTC (real time clock) is then used to update the ESP32’s internal calendar, capable of keeping track of the datetime even when the device is powered off. From there, the continuously updated calendar keeps track of when sessions are started when BLE is disconnected. The next step from there is sending a BLE characteristic containing the number of sessions stored and offloading all sessions in flash memory. The process of offloading data combined with setting datetimes for every session when there is no BLE connection allows the admin to assign the sessions to a patient on the website. The Software Design portion will go more into detail about this. A BLE connection means that the data no longer needs to be stored onboard the flash memory of the ESP32 and is simply streamed directly to the client side (website) through a BLE characteristic when the session is started from the website. If the device is alerted via a characteristic that the start session button (on the site) has been pressed, the device gathers data the same way as described above. Once the time requirements of a session are met, the device stops streaming and returns to display the prompt screen while looping through the PROMPT, STORE, and SESSION_COMPLETE states.


## Directory Structure:
```
.
|
|───production  # contains files needed for final prototype
|   |
|   |───eCAD    # contains the KiCAD project
|   └───src     # contains the code needed for deployment
|
|───testing     # contains files relevant to testing components
|
└───README.md   # this file!
```

## Preferred Tools
- KiCAD for PCB development
- Visual Studio Code / PlatformIO for code development
- Git CLI / GitHub Desktop for managing Git

## Branches
> To isolate conflicts for schematics and code, we use two branches to actually develop off of depending on discipline
- `develop/ecad` is for schematic development, please create PR's into this branch for schematics and operate in this branch for layout
- `develop/src` is for code development, please create PR's into this branch for features

## References
- Schematics + Components (WIP)

## Practice Good Git Hygiene!
1. Only commit files you intended to change
2. Create branches for each feature, and larger branches for each development effort (i.e. ```develop/*```)
3. Check your branch before starting work
4. Pull frequently to avoid conflicts
5. Make Pull Requests when you are ready to merge into a larger branch

## Git Resources

* [Setting up Git](https://fanatical-colossus-434.notion.site/Git-Installation-and-Setup-d07b7d1ab5544424876f9fd3b4a0b312)
* [Intro to Git Crash Course](https://fanatical-colossus-434.notion.site/Crash-Course-Intro-to-Git-809641611da9478b8f9cca8fd97e49fe)
