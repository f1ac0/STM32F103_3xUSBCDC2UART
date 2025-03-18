# STM32F103_3xUSBCDC2UART
This is an STM32CubeIDE project for STM32F103 microcontroller / bluepill board using tinyUSB. It configures the USB port as a device with three CDC interfaces and connects them to the three UART peripherals to make a 3 ports USB2TTL adapter.

It supports port configuration change, at least 8n1 at different bitrates, but support for parity and 9 bits data must be broken.

It was tested with TinyUSB from git at the time of this commit and an STM32F103C8T6, with up to 115200 bitrate. Actually the first two interfaces seem to be working but the 3rd one doesnt and I still do not know why (the UART are working and since it does the same on both linux and window$ then this must be a problem on this project, the stm32 USB or tinyUSB side). If you find a clue, please share it.
