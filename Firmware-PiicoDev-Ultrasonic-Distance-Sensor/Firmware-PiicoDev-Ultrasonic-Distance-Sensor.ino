/*
   PiicoDev Ultrasonic Rangefinder
   by Michael Ruppe @ Core Electronics
   Based off the Core Electronics Potentiometer module https://github.com/CoreElectronics/CE-PiicoDev-Potentiometer-MicroPython-Module
   Date: 2023-02-21
   An I2C based module that reads a 3.3V compatible ultrasonic distance sensor
   Feel like supporting PiicoDev? https://core-electronics.com.au/piicodev.html
   PiicoDev Ultrasonic Rangefinder: https://core-electronics.com.au/catalog/product/view/sku/CE09360
*/

#define DEBUG false

#if DEBUG == true
  #define debug(x) Serial.print(x)
  #define debugln(x) Serial.println(x)
#else
  #define debug(x)
  #define debugln(x)
#endif

#include <Wire.h>
#include <EEPROM.h>
#include <stdint.h>
#include <avr/sleep.h> // For sleep_mode
#include <avr/power.h> // For powering-down peripherals such as ADC and Timers

#define FIRMWARE_MAJOR 0x01
#define FIRMWARE_MINOR 0x00
#define DEVICE_ID 578
#define DEFAULT_I2C_ADDRESS 0x35    // The default address when all switches are off
#define I2C_ADDRESS_POOL_START 0x08 // The start of the 'smart module address pool' minus 1 - addresses settable by switches
#define SOFTWARE_ADDRESS true
#define HARDWARE_ADDRESS false
#define I2C_BUFFER_SIZE 32 //For ATmega328 and ATtiny based Arduinos, the I2C buffer is limited to 32 bytes

#define BIT_NEW_SAMPLE_AVAILABLE 0

// Hardware Definitions
#define trigPin PIN_PA6
#define echoPin PIN_PA5
#define powerLedPin PIN_PA3
#define addressPin1 PIN_PA1
#define addressPin2 PIN_PC3
#define addressPin3 PIN_PC2
#define addressPin4 PIN_PC1

// System global variables
uint8_t responseBuffer[I2C_BUFFER_SIZE]; // Used to pass data back to master
volatile uint8_t responseSize = 1; // Defines how many bytes of relevant data is contained in the responseBuffer

#define LOCAL_BUFFER_SIZE 32 // bytes
uint8_t incomingData[LOCAL_BUFFER_SIZE]; // Local buffer to record I2C bytes
volatile uint16_t incomingDataSpot = 0; // Keeps track of where we are in the incoming buffer

struct memoryMapRegs {
  uint8_t id;
  uint8_t firmwareMajor;
  uint8_t firmwareMinor;
  uint8_t i2cAddress;
  uint8_t timeOfFlight_us;
  uint8_t getSamplePeriod;
  uint8_t setSamplePeriod;
  uint8_t getLED;
  uint8_t setLED;
  uint8_t getStatus;
  uint8_t getSelfTestResult;
};

struct memoryMapData {
  uint16_t id;
  uint8_t firmwareMajor;
  uint8_t firmwareMinor;
  uint8_t i2cAddress;
  uint16_t timeOfFlight_us;
  uint16_t samplePeriod;
  uint8_t led;
  uint8_t status;
  uint8_t selfTestResult;
};

// Register addresses.
const memoryMapRegs registerMap = {
  .id = 0x01,
  .firmwareMajor = 0x02,
  .firmwareMinor = 0x03,
  .i2cAddress = 0x04,
  .timeOfFlight_us = 0x05,
  .getSamplePeriod = 0x06,
  .setSamplePeriod = 0x86,
  .getLED = 0x07,
  .setLED = 0x87,
  .getStatus = 0x08,
  .getSelfTestResult = 0x09
};

volatile memoryMapData valueMap = {
  .id = DEVICE_ID,
  .firmwareMajor = FIRMWARE_MAJOR,
  .firmwareMinor = FIRMWARE_MINOR,
  .i2cAddress = DEFAULT_I2C_ADDRESS,
  .timeOfFlight_us = 0,
  .samplePeriod = 20,
  .led = 0x01,
  .status = 0x00,
  .selfTestResult = 0
};

uint8_t currentRegisterNumber;

struct functionMap {
  uint8_t registerNumber;
  void (*handleFunction)(char *myData);
};

void idReturn(char *data);
void firmwareMajorReturn(char *data);
void firmwareMinorReturn(char *data);
void setAddress(char *data);
void readTimeOfFlight(char *data);
void setPowerLED(char *data);
void getPowerLED(char *data);
void setSamplePeriod(char *data);
void getSamplePeriod(char *data);
void getStatus(char *data);
void getSelfTest(char *data);


functionMap functions[] = {
  {registerMap.id, idReturn},
  {registerMap.firmwareMajor, firmwareMajorReturn},
  {registerMap.firmwareMinor, firmwareMinorReturn},
  {registerMap.i2cAddress, setAddress},
  {registerMap.timeOfFlight_us, readTimeOfFlight},
  {registerMap.getSamplePeriod, getSamplePeriod},
  {registerMap.setSamplePeriod, setSamplePeriod},
  {registerMap.setLED, setPowerLED},
  {registerMap.getLED, getPowerLED},
  {registerMap.getStatus, getStatus},
  {registerMap.getSelfTestResult, getSelfTest},
};

enum eepromLocations {
  LOCATION_I2C_ADDRESS = 0x00,  // Device's address
  LOCATION_ADDRESS_TYPE = 0x01, // Address type can be either hardware defined (jumpers/switches), or software defined by user.
};

uint8_t oldAddress;

void setup() {
  pinMode(powerLedPin, OUTPUT);
  digitalWrite(powerLedPin, HIGH);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an OUTPUT
  pinMode(echoPin, INPUT);  // Sets the echoPin as an INPUT
  pinMode(addressPin1, INPUT_PULLUP);
  pinMode(addressPin2, INPUT_PULLUP);
  pinMode(addressPin3, INPUT_PULLUP);
  pinMode(addressPin4, INPUT_PULLUP);
  
  #if DEBUG == true
  Serial.begin(9600); // // Serial Communication is starting with 9600 of baudrate speed
  #endif
  
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  readSystemSettings(); //Load all system settings from EEPROM
  startI2C();          //Determine the I2C address we should be using and begin listening on I2C bus
  oldAddress = valueMap.i2cAddress;
}


void loop() {
  bool enableRanger = (valueMap.samplePeriod > 0);
  if (!enableRanger) {
    sleep_mode();
  }

  static uint32_t lastRead = 0;
  uint32_t now = millis();
  if (enableRanger && now - lastRead >= uint32_t(valueMap.samplePeriod) ){
    lastRead = now;
    sampleRange();
  }

  if ( I2CaddressWasUpdated() ) {
    startI2C();
    oldAddress = valueMap.i2cAddress;
  }

  delay(1);
}

void sampleRange() {
  // Clears the trigPin condition
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin HIGH (ACTIVE) for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Reads the echoPin, returns the sound wave travel time in microseconds
  uint16_t duration = pulseIn(echoPin, HIGH);
  valueMap.timeOfFlight_us = duration;
  valueMap.status |= 1 << BIT_NEW_SAMPLE_AVAILABLE; // new sample available
}

bool I2CaddressWasUpdated() {
  return ( valueMap.i2cAddress != oldAddress );
}


// Begin listening on I2C bus as I2C target using the global variable valueMap.i2cAddress
void startI2C()
{
  uint8_t address;
  uint8_t addressType;
  EEPROM.get(LOCATION_ADDRESS_TYPE, addressType);
  if (addressType == 0xFF) {
    EEPROM.put(LOCATION_ADDRESS_TYPE, SOFTWARE_ADDRESS);
  }
  
  // Add hardware address jumper values to the default address
  uint8_t IOaddress = DEFAULT_I2C_ADDRESS;
  uint8_t switchPositions = 0;
  bitWrite(switchPositions, 0, !digitalRead(addressPin1));
  bitWrite(switchPositions, 1, !digitalRead(addressPin2));
  bitWrite(switchPositions, 2, !digitalRead(addressPin3));
  bitWrite(switchPositions, 3, !digitalRead(addressPin4));
#if DEBUG
  Serial.print("switchPositions");
  Serial.println(switchPositions);
#endif
  if (switchPositions != 0) IOaddress = I2C_ADDRESS_POOL_START + switchPositions; // use the "smart-module address pool" when any hardware address is set

  // If any of the address jumpers are set, we use jumpers
  if ((IOaddress != DEFAULT_I2C_ADDRESS) || (addressType == HARDWARE_ADDRESS))
  {
    address = IOaddress;
    EEPROM.put(LOCATION_ADDRESS_TYPE, HARDWARE_ADDRESS);
  }
  // If none of the address jumpers are set, we use registerMap (but check to make sure that the value is legal first)
  else
  {
    // if the value is legal, then set it
    if (valueMap.i2cAddress > 0x07 && valueMap.i2cAddress < 0x78)
      address = valueMap.i2cAddress;

    // if the value is illegal, default to the default I2C address for our platform
    else
      address = DEFAULT_I2C_ADDRESS;
  }

  // save new address to the register map
  valueMap.i2cAddress = address;
#if DEBUG
  Serial.print("I2C Address:");
  Serial.println(address);
#endif
  recordSystemSettings(); // save the new address to EEPROM

  // reconfigure Wire instance
  Wire.end();          //stop I2C on old address
  Wire.begin(address); //rejoin the I2C bus on new address

  // The connections to the interrupts are severed when a Wire.begin occurs, so here we reattach them
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}
