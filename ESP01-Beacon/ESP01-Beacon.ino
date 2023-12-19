//MSE 2202 esp01s Beacon code

//EJ Porter Dec 2023 Ver 1


//Modified code from:
// High-accuracy square wave generator
// by James Swineson <github@public.swineson.me>, 2017-10
// https://gist.github.com/Jamesits/92394675c0fe786467b26f90e95d3904
// See https://blog.swineson.me/implementation-of-6mbps-high-speed-io-on-esp8266-arduino-ide/


#include <Esp.h>
#include <esp8266_peri.h>
#include <eagle_soc.h>
#include <uart_register.h>
// #include <ESP8266WiFi.h>

#define TX_DATA 0x55

#define WDT_CTL 0x60000900
#define WDT_CTL_ENABLE (BIT(0))

// Configurable from 0 to 15; 16 not available
// Please refer to your breakout board datasheet for pin mapping
// WARNING: some pins are used internally for connecting the ESP to ROM chip;
// DO NOT USE THEM or your ESP will be bricked
#define PINOUT 0  //38 kHz out

#define NUM_CHARS 5

double freq;    // Hz
double offset;  // percent (0.0 to 1.0)
double width;   // percent (0.0 to 1.0)

// unit: microsecond
unsigned long cycle_time;
unsigned long raising_edge;
unsigned long falling_edge;
unsigned long prev_micros;

// compare 2 unsigned value
// true if X > Y while for all possible (X, Y), X - Y < Z
#define TIME_CMP(X, Y, Z) (((X) - (Y)) < (Z))

inline void setHigh() {
  GPOS = (1 << PINOUT);
}

inline void setLow() {
  GPOC = (1 << PINOUT);
}

unsigned int ui_Tx_Delay;

ADC_MODE(ADC_VCC);  // set up adc to read vcc pin

void setup() {
  // disable hardware watchdog
  CLEAR_PERI_REG_MASK(WDT_CTL, WDT_CTL_ENABLE);
  // disable software watchdog
  ESP.wdtDisable();
  // set IO pin mode
  pinMode(PINOUT, OUTPUT);

  //set up  Serial1
  //UART_FIFO(1) = {0xff, 0xFF, 0xFF, 0x55, 0xFF, 0xFF};
  // Set up UART1 pins
  // GPIO2 is TX, GPIO3 is RX
  pinMode(2, FUNCTION_2);  // Configure for TX1
  //pinMode(3, FUNCTION_2); // RX

  // Configure UART1 with desired parameters
  // For example, 2400 baud rate, 8 data bits, no parity, 1 stop bit
  Serial1.begin(2400, SERIAL_8N2);

  // calculate arguments
  freq = 38000;
  width = 0.5;
  offset = 0.0;

  cycle_time = 1000000 / freq;
  raising_edge = (unsigned long)(offset * cycle_time) % cycle_time;
  falling_edge = (unsigned long)((offset + width) * cycle_time) % cycle_time;

  prev_micros = micros();
}

void loop() {
// do pinout shifting
  if (width + offset < 1) {
    // raising edge should appear earlier
    while (TIME_CMP(micros(), prev_micros + raising_edge, cycle_time))
      ;
    setHigh();
    while (TIME_CMP(micros(), prev_micros + falling_edge, cycle_time))
      ;
    setLow();
  } 
  else {
    // falling edge should appear earlier
    while (TIME_CMP(micros(), prev_micros + falling_edge, cycle_time))
      ;
    setLow();
    while (TIME_CMP(micros(), prev_micros + raising_edge, cycle_time))
      ;
    setHigh();
  }
  prev_micros += cycle_time;
  // Check if the UART1 TX buffer is empty
  if (!(READ_PERI_REG(UART_STATUS(UART1)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S))) {
    // Send the byte if the buffer is empty
    // Fill the UART1 TX FIFO with the provided data
    ui_Tx_Delay++;
    if (ui_Tx_Delay > 5000)  //delay between sent bytes
    {
      for (int i = 0; i < NUM_CHARS; i++) {
        WRITE_PERI_REG(UART_FIFO(UART1), TX_DATA);  //Tx_data[TX_Buffer_index]);
      }
      ui_Tx_Delay = 0;
      if (ESP.getVcc() <= 255) {
        ESP.deepSleep(0);  //goes to sleep because battery is low. unplug and recharge
      }
    }
  }
}