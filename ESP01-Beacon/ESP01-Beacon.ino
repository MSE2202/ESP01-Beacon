// 
// MSE 2202 ESP-01S Beacon Code
// 
//  Language: Arduino (C++)
//  Target:   ESP-01S
//  Authors:  Eugen Porter and Michael Naish
//  Date:     2023 12 19 
//
//  Modified code from:
//    High-accuracy square wave generator
//    by James Swineson <github@public.swineson.me>, 2017-10
//    https://gist.github.com/Jamesits/92394675c0fe786467b26f90e95d3904
//    See https://blog.swineson.me/implementation-of-6mbps-high-speed-io-on-esp8266-arduino-ide/
//

#include <Esp.h>
#include <esp8266_peri.h>
#include <eagle_soc.h>
#include <uart_register.h>

#define WDT_CTL 0x60000900
#define WDT_CTL_ENABLE (BIT(0))

ADC_MODE(ADC_VCC);                                 // set up ADC to read VCC pin

// Constants
const int cCarrierPin   = 0;                       // GPIO for 38 kHz carrier
const int cSerialPin    = 2;                       // GPIO for TX1
const char cTXData      = 0x55;                    // = U = 0b01010101
const int cNumBytes     = 1;                       // number of bytes to transmit in a burst
const int cBurstSpacing = 100;                     // milliseconds between bursts
const double freq       = 38000;                   // carrier frequency in Hz
const double width      = 0.5;                     // percent (0.0 to 1.0)

// Variables
unsigned long cycleTime;                           // carrier cycle time (period) in microseconds
unsigned long highTime;                            // microseconds that carrier is high
unsigned long prevCycle;                           // time of previous cycle in microseconds
unsigned long prevBurst;                           // time of previous burst in microseconds

void setup() {
  CLEAR_PERI_REG_MASK(WDT_CTL, WDT_CTL_ENABLE);    // disable hardware watchdog
  ESP.wdtDisable();                                // disable software watchdog
  pinMode(cCarrierPin, OUTPUT);                    // Configure carrier pin as output
  pinMode(cSerialPin, FUNCTION_2);                 // Configure GPIO2 for TX1 (UART1)

  // Configure UART1 with desired parameters: 1200 baud rate, 8 data bits, no parity, 1 stop bit
  Serial1.begin(1200, SERIAL_8N1);

  cycleTime = 1000000 / freq;                      // calculate number of microseconds per period
  // calcuate number of microseconds that signal is high in cycle (time to falling edge)
  highTime = (unsigned long)(width * cycleTime) % cycleTime; 
  prevCycle = micros();                            // capture current time in microseconds
  prevBurst = millis();                            // capture current time in milliseconds
}

void loop() {
  // implement high frequency carrier signal
  while (micros() - prevCycle < highTime)          // wait number of microseconds to falling edge
  ;
  GPOC = (1 << cCarrierPin);                       // set carrier GPIO low (clear bit)
  while (micros() - prevCycle < cycleTime)         // wait remaining microseconds in cycle
  ;
  GPOS = (1 << cCarrierPin);                       // set carrier GPIO high (set bit)
  prevCycle += cycleTime;                          // update for next cycle

  // transmit character(s)
  // check whether UART1 TX buffer is empty
  if (!(READ_PERI_REG(UART_STATUS(UART1)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S))) {
    // if so, check whether sufficient time has elapsed
    if (millis() - prevBurst > cBurstSpacing) {
      for (int i = 0; i < cNumBytes; i++) {        // if so, load UART1 TX FIFO buffer with data
        WRITE_PERI_REG(UART_FIFO(UART1), cTXData); 
      }
      prevBurst += cBurstSpacing;                  // update for next burst                              
      
      if (ESP.getVcc() <= 255) {                   // check whether battery voltage is low
        ESP.deepSleep(0);                          // if so, put device to sleep -> unplug and recharge
      }
    }
  }
}