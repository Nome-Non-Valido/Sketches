/*
	FlashReader.ino

	This sketch reads/writes data from PC flash memory chips in 28 to 32 pin
	chips.
	
	The circuit:
	* low nibble of data lines is connected to PC0 .. PC3 (A0..A3)
	* high nibble of data lines is connected to PD4 .. PD7 (D4..D7)
	* CE is connected to PB5 (D13)
	* OE is connected to PB4 (D12)
	* WE is connected to PB3 (D11)
	* STR1 (Strobe 1) latches LSB of address bus, connected to PB2 (D10)
	* STR2 (Strobe 2) latches MSB of address bus, connected to PB1 (D9)
	* STR3 (Strobe 3) latches HSB of address bus, connected to PB0 (D8)
	* reserved lines:
	  - PC4/PC5 for SDA/SCL (A4/A5)
	  - PD0/PD1 for RxD/TxD (D0/D1)
	  - PD2/PD3 not used

	Created 30 Aug 2016
	 Hermann-Josef Mathes <dc2ip@darc.de>
	Modified day month year
	By author's name

  $Id$

  Port manipulation, for the Arduino Nano we have:
  - PB0 .. PB5 = D8 .. D13
  - PC0 .. PC5 = A0 .. A5
  - PD0 .. PD7 = D0 .. D7
  See also:
	https://www.arduino.cc/en/Reference/PortManipulation
  
  https://www.arduino.cc/en/Hacking/Atmega168Hardware

*/

#define DEBUG
// execute test code (if any)
#undef TEST
#define INTERACTIVE   1
#if (INTERACTIVE == 0)
 #undef DEBUG
#endif

// http://arduiniana.org/libraries/streaming/
// has to be included BEFORE Flash
#include <Streaming.h>

// http://arduiniana.org/libraries/flash/
// https://github.com/mikalhart/Flash/releases
//
// all 'prog_char' types in this library (Flash.h/Flash.cpp) must be replaced by the 'char' type
//
// my 'local copy':
// https://svn.ikp.kit.edu/svn/users/mathes/sketchbook/libraries/Flash
//
#include <Flash.h>

// check version of Flash library, V4 isn't compatible with Arduino 1.5.x and higher
#if (defined FLASH_LIBRARY_VERSION) && (FLASH_LIBRARY_VERSION < 5)
 #error "Flash version >= 5 is required!"
#endif // FLASH_LIBRARY_VERSION

#include "EEprom.h"
#include "utils.h"

// UART baud rate
#define UART_BAUD_RATE  9600

EEprom eeprom;

/**  */
void setup() {

  /* Initialize serial output at UART_BAUD_RATE bps */
  Serial.begin(UART_BAUD_RATE);
  Serial << F("Flasher.ino starting ...") << endl;
}

//
// flash strings for the main menu
//
FLASH_STRING(help_main, "Main menu, options:\n"
        "a    - enter <a>address from where to read\n"
        "f    - toggle output <f>ormat\n"
        "l    - enter <l>ength of data to read\n"
        "r    - <r>ead length bytes\n"
        "h, ? - display this <h>elp\n"
        "Ctrl-] - leave miniterm.py\n");

static const int kMAX_BLOCK_SIZE = 256;

// data buffer
uint8_t eepromData[kMAX_BLOCK_SIZE];
uint32_t eepromAddr = 0;

/**  */
void loop() {

  static bool first = true;
  static byte output_format = 0;

#ifdef TEST
  testCode();
#endif // TEST

#if (INTERACTIVE != 0)
  uint32_t total_length = 64;
  uint32_t i, len, nBytes;
  
  if ( first ) {
    help_main.print(Serial);
    first = false;
  }

  while (1) {

    int ch = Serial.read();

    // ignore also LFs (from windowish systems)
    if ( ch == -1 || ch == 0x0a ) continue;
    
    switch (ch) {

      case 'a':  // --- set the base address
      case 'A':
          Serial.print(F("\nEnter start address? ")); Serial.flush();
          //eepromAddr = readInt(); // readUInt32();
          eepromAddr = Serial.parseInt();
          Serial.println(eepromAddr);
        break;

      case 'f':  // --- toggle output format
      case 'F':
          if ( output_format == 0 ) {
	          output_format = 1;
	          Serial.println(F("Switched to IHEX output format!"));
	        }
	        else {
	          output_format = 0;
	          Serial.println(F("Switched to DUMP output format!"));
      	  }
        break;

      case 'l':  // --- set the length
      case 'L':
        Serial.print(F("\nEnter block length? ")); Serial.flush();
        //total_length = readInt(); // readUInt32();
        total_length = Serial.parseInt();
        Serial.println(total_length);
        break;

      case 'r':  // --- read length bytes starting from address
      case 'R':
          len = total_length;
          while ( len > 0 ) {
            nBytes = min(len, kMAX_BLOCK_SIZE);
            len -= nBytes;
            
            eeprom.read(eepromAddr, eepromData, nBytes);
	          if ( output_format == 0 )
              dumpHex(eepromData, nBytes, eepromAddr);
            else
              writeIhexData(Serial, eepromData, nBytes, eepromAddr);
            
            eepromAddr += nBytes;
          }
          if ( output_format == 1 )
            writeIhexEOF(Serial);
        break;

      case 's':  // --- show information
      case 'S':
          Serial.println();
          Serial.print(F("Start address: ")); printHex(eepromAddr, 6);
          Serial.println();
          Serial.print(F("Block length:    ")); printHex(total_length, 4);
          Serial.println();
        break;
      
      case 'h':
      case 'H':
      case '?':
          help_main.print(Serial);
        break;

      default:
          Serial << "?" << endl;
    }
  }
#endif // INTERACTIVE
}

static void writeByteHex(Stream& stream,uint8_t data) {
  if ( data < 0x10 )
    stream.print("0");
  stream.print(data, HEX);
}

// https://en.wikipedia.org/wiki/Intel_HEX
void writeIhexData(Stream& stream,const uint8_t *data,const size_t data_length,const uint32_t eprom_addr) {

  static const size_t eIhexBlocksize = 16;  // 32
  uint32_t addr = eprom_addr;
  size_t cur_length = data_length;
  size_t offset = 0;

  while ( cur_length > 0 ) {

    size_t len = min(cur_length, eIhexBlocksize);
    
    uint8_t addr_lo = (uint8_t)(addr & 0xff);
    uint8_t addr_hi = (uint8_t)((addr & 0xff00) >> 8);

    byte checksum = 0;
  
    checksum += len;
    checksum += addr_hi;
    checksum += addr_lo;
  
    stream.print(":");
    writeByteHex(stream, eIhexBlocksize);
    writeByteHex(stream, addr_hi);
    writeByteHex(stream, addr_lo);
    stream.print("00");   // block type 'data'

    for ( size_t i=0; i<len; ++i ) {
      writeByteHex(stream, data[offset + i]);
      checksum += data[offset + i];
    }

    writeByteHex(stream, checksum);
    stream.println("");

    cur_length -= len;
    addr += len;
    offset += len;
  }
}

void writeIhexEOF(Stream& stream) {
  
  stream.println(":000001FF");  // block type 'EOF'
}

uint32_t readUInt32(void) {

  byte line[20];

  while (Serial.available() > 0)       /* clear old garbage */
    Serial.read();

  String l = Serial.readString();

  Serial.print(l);
  
  l.getBytes(line, sizeof(line));
  
  uint32_t data = strtol((const char *)line, NULL, 0);
  
  return data;
}

/** Test code frame ... */
void testCode() {
  
  static bool first = true;
#if 0
  if ( first ) {

    Serial.println("Fake data:");
    
    for ( int i=0; i<sizeof(eepromData); ++i )
      eepromData[i] = (uint8_t)(i & 0xff);
    
    dumpHex(eepromData, sizeof(eepromData), eepromAddr);

    first = false;
  }
#endif

  //testAddressLatches();

  int nBytes = kMAX_BLOCK_SIZE;

#if 0
  Serial.println("EEPROM data (single read):");
  
  // read 'nBytes' bytes from (E)EPROM
  for ( int i=0; i<nBytes; ++i ) {

    eeprom.setAddress( (uint32_t)(i + eepromAddr) );
    
    eepromData[i] = eeprom.read();
  }

  dumpHex(eepromData, nBytes, eepromAddr);
#endif

#if 1
  Serial.println("EEPROM data (block read):");

  eeprom.read(eepromAddr, eepromData, nBytes);
  dumpHex(eepromData, nBytes, eepromAddr);
#endif

  delay(5000);
}

/** Test code to test the address latching unit. */
void testAddressLatches() {

  static uint8_t addr = 0x01;

#ifdef DEBUG
  Serial.print("0x");
  if ( addr < 0x0f )
    Serial.print("0");
  Serial.println(addr, HEX);
#endif // DEBUG

  eeprom.setAddressLSB(addr);

  delay(100);

  eeprom.setAddressMSB(addr);

  delay(100);

  eeprom.setAddressHSB(addr);

  addr <<= 1;
  //addr += 1;

  if ( !addr ) addr = 0x01;

  delay(2500);
}
