// GPIO map of the class board, v0.6 (10-Class-Board-Design-Brief.md §4.2).
// Jinhua ESP32-S3 N16R8 dev board, DevKitC-1 pin order.
//
// Never use: GPIO 0, 45, 46 (strapping) and 35-37 (octal PSRAM).
#pragma once

// ---- Base: buses ----------------------------------------------------------
constexpr int PIN_SPI_SCLK = 12;   // SPI2 on IO_MUX pins (80 MHz capable)
constexpr int PIN_SPI_MOSI = 11;
constexpr int PIN_SPI_MISO = 13;
constexpr int PIN_I2C_SDA  = 1;    // 400 kHz, 4.7 k pull-ups on the base; OLED 0x3C
constexpr int PIN_I2C_SCL  = 2;
constexpr int PIN_RGB_LED  = 48;   // WS2812 on the dev board itself

// ---- B1 Precision inputs (ADS8688) ---------------------------------------
constexpr int PIN_CS_ADC = 10;     // ADS8688 /CS

// ---- B3 Signal generation (DAC8563) --------------------------------------
constexpr int PIN_CS_DAC = 5;      // DAC8563 /SYNC

// ---- B4 Isolated switching -----------------------------------------------
constexpr int PIN_RELAY[4]  = {4, 6, 7, 15};   // ULN2003 IN1..IN4
constexpr int PIN_OPTO_IN[2] = {16, 17};       // 6N137 outputs (3V3 side, active low)

// ---- B5 Digital I/O & timing ---------------------------------------------
constexpr int PIN_DIO[8]     = {41, 42, 47, 40, 8, 9, 14, 3};  // 74AHCT541 A1..A8
constexpr int PIN_FAST_OUT[2] = {18, 21};      // MCPWM/RMT -> 74HCT125 -> terminals
constexpr int PIN_TRIG_IO  = 38;   // 74LVC1T45 A side
constexpr int PIN_TRIG_DIR = 39;   // 74LVC1T45 DIR: 1 = A->B = output to the TRIG SMA

// ---- Base: USB / UART (informational, not driven by firmware) ------------
constexpr int PIN_USB_DN = 19;
constexpr int PIN_USB_DP = 20;
constexpr int PIN_UART0_TX = 43;   // free expansion pins when the console is USB CDC
constexpr int PIN_UART0_RX = 44;
