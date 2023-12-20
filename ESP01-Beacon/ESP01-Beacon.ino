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

// compare 2 unsigned values
// true if X > Y while for all possible (X, Y), X - Y < Z
#define TIME_CMP(X, Y, Z) (((X) - (Y)) < (Z))

// Configurable from 0 to 15; 16 not available
// Please refer to your breakout board datasheet for pin mapping
// WARNING: some pins are used internally for connecting the ESP to ROM chip;
// DO NOT USE THEM or your ESP will be bricked
const int cCarrierPin = 0;                         // GPIO for 38 kHz carrier

const int cSerialPin = 2;                          // GPIO for TX1
const char cTXData = 0x55;                         // U
const int cNumBytes = 1;                           // number of bytes to transmit in a burst
const int cBurstSpacing = 5000;                    // time between bursts

double freq;                                       // carrier frequency in Hz
double offset;                                     // percent (0.0 to 1.0)
double width;                                      // percent (0.0 to 1.0)

unsigned int TXDelay;                              //

// unit: microsecond
unsigned long cycleTime;                           // carrier cycle time in microseconds
unsigned long risingEdge;                          // 
unsigned long fallingEdge;
unsigned long prevMicros;

ADC_MODE(ADC_VCC);                                 // set up ADC to read VCC pin

inline void setHigh() {
  GPOS = (1 << cCarrierPin);
}

inline void setLow() {
  GPOC = (1 << cCarrierPin);
}

void setup() {
  // disable hardware watchdog
  CLEAR_PERI_REG_MASK(WDT_CTL, WDT_CTL_ENABLE);
  // disable software watchdog
  ESP.wdtDisable();
  // set IO pin mode for carrier signal
  pinMode(cCarrierPin, OUTPUT);

  //set up  Serial1
  //UART_FIFO(1) = {0xff, 0xFF, 0xFF, 0x55, 0xFF, 0xFF};
  // Set up UART1 pins
  // GPIO2 is TX, GPIO3 is RX
  pinMode(cSerialPin, FUNCTION_2);  // Configure for TX1
  //pinMode(3, FUNCTION_2); // RX

  // Configure UART1 with desired parameters
  // For example, 2400 baud rate, 8 data bits, no parity, 1 stop bit
  Serial1.begin(1200, SERIAL_8N1);

  // calculate arguments
  freq = 38000;  
  width = 0.5;
  offset = 0.0;

  cycleTime = 1000000 / freq;
  risingEdge = (unsigned long)(offset * cycleTime) % cycleTime;
  fallingEdge = (unsigned long)((offset + width) * cycleTime) % cycleTime;

  prevMicros = micros() + cycleTime;
}

void loop() {
// do pinout shifting for carrier
  if (width + offset < 1) {
    // rising edge should appear earlier
    while (TIME_CMP(micros(), prevMicros + risingEdge, cycleTime)) {
      setHigh();
    }
    while (TIME_CMP(micros(), prevMicros + fallingEdge, cycleTime)) {
      setLow();
    }
  } 
  else {
    // falling edge should appear earlier
    while (TIME_CMP(micros(), prevMicros + fallingEdge, cycleTime)) {
      setLow();
    }
    while (TIME_CMP(micros(), prevMicros + risingEdge, cycleTime)) {
      setHigh();
    }
  }
  prevMicros += cycleTime;
  // Check if the UART1 TX buffer is empty
  if (!(READ_PERI_REG(UART_STATUS(UART1)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S))) {
    // Send the byte if the buffer is empty
    // Fill the UART1 TX FIFO with the provided data
    TXDelay++;
    if (TXDelay > cBurstSpacing) {                 // delay between sent bursts
      for (int i = 0; i < cNumBytes; i++) { 
        WRITE_PERI_REG(UART_FIFO(UART1), cTXData); //Tx_data[TX_Buffer_index]);
      }
      TXDelay = 0;                                 // reset TXDelay
      if (ESP.getVcc() <= 255) {                   // if battery voltage is low
        ESP.deepSleep(0);                          // put device to sleep -> unplug and recharge
      }
    }
  }
}