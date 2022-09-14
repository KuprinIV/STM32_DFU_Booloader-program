# STM32_DFU_Booloader-program
Program for updating STM32 chips firmware via USB DFU interface. Developed on Qt 6.2.3 platform and use libusb 1.0 USB driver
Use this program with DFU boolloader firmware based on https://github.com/KuprinIV/STM32F3_DFU_Test/tree/master repository
After build release copy libusb-1.0.dll into release_output build directory depending of compiler type (MinGW or MSVC2019, both 64-bit)
