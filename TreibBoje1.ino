//
// File   : TreibBoje1.ino
//
// Purpose: Frame for fun project 'Treibboje1' (experimental)
//
// $Id$
//

// debug via serial interface
#define DEBUG
// scan sensors (to inquire addresses)
#define SCAN_SENSORS
// read temperature sensors
#define READ_SENSORS
// send the data via 433MHZ Tx
#define SEND_DATA
// read photo resistor (LDR)
#define USE_LDR

#ifdef DEBUG
 // http://arduiniana.org/libraries/streaming/
 // has to be included BEFORE Flash
 #include <Streaming.h>
 #include <Flash.h>

 // UART baud rate
 #define UART_BAUD_RATE  9600
#endif // DEBUG

#ifdef SEND_DATA
 #include <RCSwitch.h>
#endif // SEND_DATA

// http://www.pjrc.com/teensy/td_libs_OneWire.html
#include <OneWire.h>

// https://github.com/milesburton/Arduino-Temperature-Control-Library
// http://milesburton.com/Main_Page?title=Dallas_Temperature_Control_Library
// Note:
// The library archive is not well formated and has to be renamed into DallasTemperature
//
// DS18B20       +-------+
//               |       |
//               | 18B20 |
//               |       |
//               ++--+--++
//                |  |  |
//              GND Dq  Vdd
//
#include <DallasTemperature.h>

#ifdef SEND_DATA
 #define TX_PIN     10

RCSwitch theSender = RCSwitch();

#endif // SEND_DATA

#ifdef USE_LDR
 // define a pin for Photo resistor (ADC input!)
 #define LDR_PIN    14
#endif // USE_LDR

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 10

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses of known 1-wire temp. sensors
// sensor #1
DeviceAddress gSensor1 = { 0x28, 0xCE, 0x15, 0x46, 0x06, 0x00, 0x00, 0x04 };
// sensor #2
DeviceAddress gSensor2 = { 0x28, 0x65, 0x5D, 0x47, 0x06, 0x00, 0x00, 0xF8 };
// sensor #3
DeviceAddress gSensor3 = { 0x28, 0x76, 0xC1, 0xE0, 0x06, 0x00, 0x00, 0x43 };
// sensor #4
DeviceAddress gSensor4 = { 0x28, 0x7A, 0x31, 0xE0, 0x06, 0x00, 0x00, 0x98 };
// sensor #5
DeviceAddress gSensor5 = { 0x28, 0x6B, 0x84, 0xDF, 0x06, 0x00, 0x00, 0xBB };

// a global cycle counter
static uint16_t gCycleCounter = 0;

/**
 * Initialization
 */
void setup()
{
#ifdef DEBUG
  /* Initialize serial output at UART_BAUD_RATE bps */
  Serial.begin(UART_BAUD_RATE);
  Serial << F("Starting ...");

  Serial << F("Free SRAM: ");
  Serial.println(availableMemory());
#endif // DEBUG

  // OneWire setup etc.
  // Start up the library
  sensors.begin();

#ifdef SEND_DATA
  // Transmitter is connected to Arduino Pin #10 (TX_PIN)
  theSender.enableTransmit(TX_PIN);
  
  // default in RCSwitch library is 10
  //theSender.setRepeatTransmit(10);

  // Optional set pulse length.
  theSender.setPulseLength(200);
  
  // Optional set protocol (default is 1, will work for most outlets)
  //theSender.setProtocol(1);  // -> setPulseLength(350); '0' = 1,3
#endif // SEND_DATA

#if (defined SCAN_SENSORS) && (defined DEBUG)
  // locate devices on the bus
  Serial.print(F("Locating devices..."));
  Serial.print(F("Found "));
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(F(" devices."));

  // report parasite power requirements
  Serial.print(F("Parasite power is: ")); 
  Serial.println((sensors.isParasitePowerMode() ? "ON" : "OFF"));

  byte addr[8];
  for ( byte nSensor=0; nSensor<sensors.getDeviceCount(); ++nSensor) {
    if ( sensors.getAddress(addr, nSensor) ) {
      sensors.setResolution(addr, TEMPERATURE_PRECISION);
      Serial << "Device " << (int)nSensor << ": ";
      printAddress( addr );
//      for ( byte i=0; i<8; ++i ) {
//        Serial.print(addr[i], HEX);
//        Serial.print(" ");
//      }
      Serial.println();
    }
  }
#else
  // set desired precision of the temperature sensors
  byte addr[8];
  for ( byte nSensor=0; nSensor<sensors.getDeviceCount(); ++nSensor) {
    if ( sensors.getAddress(addr, nSensor) ) {
      sensors.setResolution(addr, TEMPERATURE_PRECISION);
    }
  }
#endif // SCAN_SENSORS && DEBUG

#ifdef USE_LDR
  //           PhotoR     10K
  // +5    o---/\/\/--.--/\/\/---o GND
  //                  |
  // Pin 0 o----------+
  //
  pinMode( LDR_PIN, INPUT );
  
  // change range and resolution of analog pin, default
  analogReference(DEFAULT);
#endif // USE_LDR
}

int getRawTemperature(float temp,int precision=12) {
  switch (precision) {
    case 12:
      return ((int)(temp / 0.0625)) & 0x0fff;
    case 11:
      return ((int)(temp / 0.125)) & 0x07ff;
    case 10:
      return ((int)(temp / 0.25)) & 0x03ff;
    case 9:
      return ((int)(temp / 0.5)) & 0x01ff;
  }
  
  return 0;
}

//
// This is how the data is encoded:
//
// - the ASK message consist of 12 bits
// - 3 bits (a2..a0) are used to specify the sensor
// - the sensor data are put in the
//   remaining 9 bits (d8..d0)
//
//  D11 D10 D9 D8 | D7 D6 D5 D4 | D3 D2 D1 D0
//   a2  a1 a0 d8   d7 d6 d5 d4   d3 d2 d1 d0
//
// - a2a1a0 = 0 specifies the cycle counter
// - a2a1a0 = 1..5 specifies the temperature sensors
//            which therefor operate in 9 bit mode
// - a2a1a0 = 6 specifies the LDR message
// - a2a1a0 = 7 specifies sync messages
//  
#ifdef SEND_DATA
 #define SEND_SYNC(_x) theSender.send((_x & 0x1ff) | 0x0e00, 12)
 #define SEND_CYCLE_COUNTER() theSender.send((gCycleCounter & 0x1ff), 12)
 #define SEND_LDR(_x) theSender.send((_x & 0x1ff) | (0x0600 << 1), 12)
 #define SEND_T1(_x) theSender.send((_x & 0x1ff) | (0x0100 << 1), 12)
 #define SEND_T2(_x) theSender.send((_x & 0x1ff) | (0x0200 << 1), 12)
 #define SEND_T3(_x) theSender.send((_x & 0x1ff) | (0x0300 << 1), 12)
 #define SEND_T4(_x) theSender.send((_x & 0x1ff) | (0x0400 << 1), 12)
 #define SEND_T5(_x) theSender.send((_x & 0x1ff) | (0x0500 << 1), 12)
#else
 #define SEND_SYNC(_x) {}
 #define SEND_CYCLE_COUNTER() {}
 #define SEND_LDR(_x) {}
 #define SEND_T1(_x) {}
 #define SEND_T2(_x) {}
 #define SEND_T3(_x) {}
 #define SEND_T4(_x) {}
 #define SEND_T5(_x) {}
#endif // SEND_DATA

/**
 * Main loop
 */
void loop() {

#ifdef SEND_DATA
  // send some dummy telegrams, cannot set 0!
  //theSender.send(0x0e01, 12);
  //theSender.send(0x0e02, 12);
  //theSender.send(0x0fff, 12);
#endif // SEND_DATA

  SEND_SYNC(1);
  
  SEND_CYCLE_COUNTER();
#ifdef DEBUG
  Serial << "# " << gCycleCounter << endl;
#endif // DEBUG

#ifdef READ_SENSORS
  if ( sensors.getDeviceCount() > 0 ) {
    sensors.requestTemperatures(); // Send the command to get temperatures

    float temp;
    
    // printout of temperature sensor values in the order they are enumerated at the 1-wire bus
    // reading will be prefixed via T1: .. T2: ... etc.
    // alternative printout, only known sensors correctly prefixed
    if ( sensors.isConnected( gSensor1 ) ) {
      temp = sensors.getTempC( gSensor1 );
#ifdef DEBUG
      Serial << "T1: " << temp << " " 
             << getRawTemperature(temp, TEMPERATURE_PRECISION)
             << " ";
#endif // DEBUG
      SEND_T1(getRawTemperature(temp, TEMPERATURE_PRECISION));
    }
    if ( sensors.isConnected( gSensor2 ) ) {
      temp = sensors.getTempC( gSensor2 );
#ifdef DEBUG
      Serial << "T2: " << temp << " " 
             << getRawTemperature(temp, TEMPERATURE_PRECISION)
             << " ";
#endif // DEBUG
      SEND_T2(getRawTemperature(temp, TEMPERATURE_PRECISION));
    }
    if ( sensors.isConnected( gSensor3 ) ) {
      temp = sensors.getTempC( gSensor3 );
#ifdef DEBUG
      Serial << "T3: " << temp << " " 
             << getRawTemperature(temp, TEMPERATURE_PRECISION)
             << " ";
#endif // DEBUG
      SEND_T3(getRawTemperature(temp, TEMPERATURE_PRECISION));
    }
    if ( sensors.isConnected( gSensor4 ) ) {
      temp = sensors.getTempC( gSensor4 );
#ifdef DEBUG
      Serial << "T4: " << temp << " " 
             << getRawTemperature(temp, TEMPERATURE_PRECISION)
             << " ";
#endif // DEBUG
      SEND_T4(getRawTemperature(temp, TEMPERATURE_PRECISION));
    }
    if ( sensors.isConnected( gSensor5 ) ) {
      temp = sensors.getTempC( gSensor5 );
#ifdef DEBUG
      Serial << "T5: " << temp << " " 
             << getRawTemperature(temp, TEMPERATURE_PRECISION)
             << " ";
#endif // DEBUG
      SEND_T5(getRawTemperature(temp, TEMPERATURE_PRECISION));
    }
#ifdef DEBUG
    Serial.println();
#endif // DEBUG
  } // if ( getDeviceCount() > 0 )
#endif // READ_SENSORS

#ifdef USE_LDR
  // divide the resulting value:
  // - with a 10k resistor divide the value by 2
  // - for 100k resistor divide by 4.
  // - this is a 10bit (0..1023) ADC
  int ldr_value = analogRead(LDR_PIN)/4;
#ifdef DEBUG
  Serial << "LDR: " << ldr_value << endl;
#endif // DEBUG
  SEND_LDR(ldr_value);
#endif // USE_LDR

  SEND_SYNC(2);
  
  gCycleCounter = ( gCycleCounter == 0x1ff ) ? 0 : (gCycleCounter+1);
}

/**
 * Try to inquire max. available memory by allocating until heap is exhausted.
 *
 * from: https://devel-ik.fzk.de/wsvn/fd-online/Diverse/Oktokopter/Arduino2Flasher/trunk/Arduino2Flasher.ino
 */
int availableMemory() {

  int size = 10000;
  byte *buf;
  while ((buf = (byte *) malloc(--size)) == NULL);
  free(buf);
  return size;
}

/** function to print a device address */
void printAddress(DeviceAddress deviceAddress) {

  for (uint8_t i = 0; i < 8; i++) {
    Serial.print("0x");
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    Serial.print(" ");
  }
}

/** function to print a device's resolution */
void printResolution(DeviceAddress deviceAddress) {

  Serial.print("Resolution: ");
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();    
}


