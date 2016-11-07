/*
	FlashReader.ino

	This sketch reads data from EPROM chips and  reads/writes data from/to PC flash 
	memory chips (EEPROM) in 24, 28 and 32 pin cases.
	
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

#include "FlashReader.h"

#include "utils.h"
#include "ihex.h"

// EEPROM object
EEprom eeprom;

// data buffer etc.
uint8_t eepromData[kMAX_BLOCK_SIZE];
uint32_t eepromAddr = 0;
//EEprom::eEEPROMtype eepromType = EEprom::eEEPROM_2732;
EEprom::eEEPROMtype eepromType = EEprom::eEEPROM_27020;

/**  */
void setup() {

  /* Initialize serial output at UART_BAUD_RATE bps */
  Serial.begin(UART_BAUD_RATE);
#ifdef DEBUG
  Serial << F("FlashReader.ino starting ...") << endl;
#endif // DEBUG

  eeprom.setType(eepromType);
}


/**  */
void loop() {

#ifdef TEST
  testCode();
#endif // TEST

#if (INTERACTIVE != 0)
  loopInteractive();
#else
  loopNonInteractive();
#endif // INTERACTIVE
}

#if (INTERACTIVE == 0)
 // Calculated based on max input size expected for one command
 #define INPUT_SIZE   64
 // Maximum number of options expected
 #define MAX_OPTIONS   3

char inputLine[INPUT_SIZE+1] = { 0 };
int lineLength = 0;
bool lineComplete = false;

/** loop() function for the non-interactive version. */
void loopNonInteractive() {

  // read characters from serial interface and put them in input buffer until EOL
  int ch = Serial.read();

  if ( ch == -1 ) return;
  
  // check for LF or add character to inputLine
  if ( ch == 0x0a || ch == 0x0d ) {
    lineComplete = true;
  }
  else {
    // Add the final 0 to end the C string
    inputLine[lineLength++] = ch;
    inputLine[lineLength] = 0;
  
    // too many chars entered? clear line buffer and issue warning
    if ( lineLength == INPUT_SIZE ) {
      lineLength = 0;
      inputLine[lineLength] = 0;
      Serial << F("ERR: max. line length exceeded!") << endl;
    }
  }

  //
  // valid commands:
  // 't ?' - get E(E)PROM type
  // 't <t>' - set E(E)PROM type
  // 'r <addr> <len>' - read <len> bytes starting from address <addr>
  // 'w <addr> <bytes>' - write <bytes> (in IHEX format) to address <addr> 
  //
  if ( lineComplete ) {
    
    Serial << strlen(inputLine) << " '" << inputLine << "'" << endl;

    char *command = NULL;
    int nopts = 0;
    char* token = strtok(inputLine, " ");
    char *option[MAX_OPTIONS];
    bool err = false;

    while ( token && !err ) {
      
      // 'command' token must have length of 1
      if ( strlen(token) == 1 && !command ) {
        command = token;
      }
      // 'command' token found, now check for options
      else if ( command ) {
        if ( nopts == MAX_OPTIONS ) {
          err = true;
          Serial << F("ERR: maximum number of options exceeded") << endl;
        }
        option[nopts++] = token;
      }
      else if ( !command ) {
        Serial << F("ERR: no command found!") << endl;
        err = true;
      }

      // find next token
      token = strtok(0, " ");
    }

#if 0
    // final control output after tokenisation
    if ( command && !err ) {
      Serial << F("command= '") << command << "'" << endl;
      for ( int i=0; i<nopts; ++i )
        Serial << F("option[") << i << F("]= '") << option[i] << "'" << endl;
    }
#endif

    if ( !err ) {

      size_t total_length, len, nBytes;
      
      switch ( command[0] ) {

        case 't': 
        case 'T': 
          // requires exactly one parameter
          if ( nopts != 1 ) {
            err = 1;
            Serial << F("ERR: number of parameters mismatch") << endl;
          }
          else if ( *option[0] == '?' ) {
            Serial << F("T ") << (int)eepromType << endl;
          }
          else if (    atoi(option[0]) > (int)EEprom::eEEPROM_2716
                    || atoi(option[0]) <= (int)EEprom::eEEPROM_27040 ) {
            eepromType = (EEprom::eEEPROMtype)atoi(option[0]);
            eeprom.setType( eepromType );
          }
          else {
            err = 1;
            Serial << F("ERR: parameter range mismatch") << endl;
          }
        break;

        case 'r': 
        case 'R': 
          // requires exactly two parameters
          if ( nopts != 2 ) {
            err = 1;
            Serial << F("ERR: number of parameters mismatch") << endl;
          }
          else {
            eepromAddr = atoi(option[0]);
            total_length = atoi(option[1]);

            if ( eepromAddr >= eeprom.getSize() ) {
              err = 1;
              Serial << F("ERR: start address outside E(E)PROM address range") << endl;
            }
            if ( total_length <= 0 ) {
              err = 1;
              Serial << F("ERR: illegal length parameter") << endl;
            }
            if ( !err ) {
              len = total_length;
              while ( len > 0 ) {
                nBytes = min(len, kMAX_BLOCK_SIZE);
                len -= nBytes;

                eeprom.read(eepromAddr, eepromData, nBytes);
                writeIhexData(Serial, eepromData, nBytes, eepromAddr);

                eepromAddr += nBytes;
              }
              writeIhexEOF(Serial);
            }
          }
        break;

        case 'w': 
        case 'W': 
          // requires exactly two parameters
          if ( nopts != 2 ) {
            err = 1;
            Serial << F("ERR: number of parameters mismatch") << endl;
          }
        break;

        case '?': 
          Serial << F("R W T") << endl;
        break;

        default:
          Serial << F("ERR: unknown command entered!") << endl;
          err = true;
      }
    }

    if ( !err ) {
      Serial << F("OK") << endl;
    }

    lineLength = 0;
    inputLine[lineLength] = 0;
    
    lineComplete = false;
  }
}
#endif // INTERACTIVE

#if (INTERACTIVE != 0)
//
// flash strings for the main menu
//
FLASH_STRING(help_main, "Main menu, options:\n"
        "a    - enter <a>address from where to read\n"
        "f    - toggle output <f>ormat\n"
        "l    - enter <l>ength of data to read\n"
        "p    - <p>rint some information\n"
        "r    - <r>ead length bytes\n"
        "t    - set EPROM <t>ype\n"
        "h, ? - display this <h>elp\n"
      /*  "Ctrl-] - leave miniterm.py\n" */ );

/** loop() function for the interactive programm version. */
void loopInteractive() {
  
  static byte output_format = 0;

  uint32_t total_length = 64;
  uint32_t i, len, nBytes;
  
  if ( first ) {
    help_main.print(Serial);
    Serial.print(F("E(E)PROM type: ")); 
    Serial << (int)eeprom.getType() << " (" 
           << EEprom::getTypeString(eeprom.getType()) << ")" << endl;
    first = false;
  }

#if 0
  // test, is working
  uint32_t t = strtol("1000", NULL, 0); Serial.println(t);
  t = strtol("0x1000", NULL, 0); Serial.println(t);
#endif
  
  while (1) {

    int ch = Serial.read();

    // ignore also LFs (from windowish systems)
    if ( ch == -1 || ch == 0x0a ) continue;
    
    switch (ch) {

      case 'a':  // --- set the base address
      case 'A':
          Serial.print(F("\nEnter start address? ")); Serial.flush();
          eepromAddr = readInt32();
          //eepromAddr = Serial.parseInt();
          //Serial.println(eepromAddr);
          Serial.println();
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
        total_length = readInt();
        Serial.println();
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

      case 'p':  // --- print some information
      case 'P':
          Serial.println();
          Serial.print(F("E(E)PROM type: ")); 
          Serial << (int)eeprom.getType() << " (" 
                 << EEprom::getTypeString(eeprom.getType()) << ")" << endl;
          Serial.print(F("Size:          ")); printHex(eeprom.getSize(), 6); 
          Serial << " (" << eeprom.getSize() << ")";
          Serial.println();
          Serial.print(F("Start address: ")); printHex(eepromAddr, 6);
          Serial.println();
          Serial.print(F("Block length:    ")); printHex(total_length, 4);
          Serial.println();
        break;
      
      case 't':
      case 'T':
          Serial.println();
          Serial.println(F("Select any possible E(E)PROM type:"));
          Serial.print(EEprom::eEEPROM_2716); Serial.println(F(" - 2716 (2K * 8)"));
          Serial.print(EEprom::eEEPROM_2732); Serial.println(F(" - 2732 (4K * 8)"));
          Serial.print(EEprom::eEEPROM_2764); Serial.println(F(" - 2764 (8K * 8)"));
          Serial.print(EEprom::eEEPROM_27128); Serial.println(F(" - 27128 (16K * 8)"));
          Serial.print(EEprom::eEEPROM_27256); Serial.println(F(" - 27256 (32K * 8)"));
          Serial.print(EEprom::eEEPROM_27512); Serial.println(F(" - 27512 (64K * 8)"));
          Serial.print(EEprom::eEEPROM_27010); Serial.println(F(" - 271001, 27C010 (128K * 8)"));
          Serial.print(EEprom::eEEPROM_27020); Serial.println(F(" - 272001, 27C020 (256K * 8)"));
          Serial.print(EEprom::eEEPROM_27040); Serial.println(F(" - 274001, 27C040 (512K * 8)"));
          Serial.print(EEprom::eEEPROM_NONE); Serial.println(F(" - Cancel"));
          Serial.print(F("E(E)PROM type? ")); Serial.flush();
          eepromType = (EEprom::eEEPROMtype)readInt((uint16_t)EEprom::eEEPROM_NONE, 
                                                    (uint16_t)EEprom::eEEPROM_27040); // Serial.parseInt();
          if (eepromType) {
            eeprom.setType(eepromType);
            eepromAddr = 0;  // start again with first addr
          }
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
}
#endif // INTERACTIVE

/** Read unsigned integer, 32 bits from console (experimental). */
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

/** Test code frame ... 
  *  
  *  Various code and hardware test sequences, to be activated by pre-processor directives
  */
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

/** Test code to test the address latching unit. 
  *
  * The address bits are switched through subsequently (shift register mode).
  */
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

