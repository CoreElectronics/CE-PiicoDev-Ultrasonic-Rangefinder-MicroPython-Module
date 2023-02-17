/*
  User accessible functions
*/

void getStatus(char *data) {
  loadArray(valueMap.status);
}

void readTimeOfFlight(char *data) {
  loadArray(valueMap.timeOfFlight_us);
  valueMap.status &= ~(1 << BIT_NEW_SAMPLE_AVAILABLE);
}

void idReturn(char *data) {
  loadArray(valueMap.id);
}

void firmwareMajorReturn(char *data) {
  loadArray(valueMap.firmwareMajor);
}

void firmwareMinorReturn(char *data) {
  loadArray(valueMap.firmwareMinor);
}

void getPowerLED(char *data) {
  loadArray(valueMap.led);
  debug("  Led status: ");debugln(valueMap.led);
}

void setPowerLED(char *data) {
  debugln("Set the LED");
  valueMap.led = (data[0] == 1) ;
  digitalWrite(powerLedPin, valueMap.led);
}

void getSamplePeriod(char *data) {
  loadArray(valueMap.samplePeriod);
}

void setSamplePeriod(char *data) {
  valueMap.samplePeriod = (uint8_t(data[0]) << 8) | uint8_t(data[1]);
}

void setAddress(char *data) {
  uint8_t tempAddress = data[0];

  if (tempAddress < 0x08 || tempAddress > 0x77)
    return; // Command failed. This address is out of bounds.
  valueMap.i2cAddress = tempAddress;

  EEPROM.put(LOCATION_ADDRESS_TYPE, SOFTWARE_ADDRESS);
}


void getSelfTest(char *data) {
  runSelfTest();
  loadArray(valueMap.selfTestResult);
}

void runSelfTest(void){
  const bool FAIL = false;
  const bool PASS = true;
  bool state = PASS;
  
  // Check for shorts between address pins
  uint8_t numPins = 4;
  uint16_t pins[] = {addressPin1, addressPin2, addressPin3, addressPin4};

  // Set all pins as input with pullup
  for (uint8_t i=0; i<numPins; i++){
    pinMode(pins[i], INPUT_PULLUP);
  }
  // Test Address Pin 1 against all others
  pinMode(pins[0], OUTPUT);
  digitalWrite(pins[0], LOW);
  if (digitalRead(pins[1]) == 0) state = FAIL;
  if (digitalRead(pins[2]) == 0) state = FAIL;
  if (digitalRead(pins[3]) == 0) state = FAIL;
  pinMode(pins[0], INPUT_PULLUP);
  
  // Test Address Pin 2 against remaining pins
  pinMode(pins[1], OUTPUT);
  digitalWrite(pins[1], LOW);
  if (digitalRead(pins[2]) == 0) state = FAIL;
  if (digitalRead(pins[3]) == 0) state = FAIL;
  pinMode(pins[1], INPUT_PULLUP);

  // Test Address Pin 3 against remaining pins
  pinMode(pins[2], OUTPUT);
  digitalWrite(pins[2], LOW);
  if (digitalRead(pins[3]) == 0) state = FAIL;
  pinMode(pins[2], INPUT_PULLUP);

  // Every address pin has now been tested against every other pin

  // Check LED pin is not shorted to adjacent GND pin.
  pinMode(powerLedPin, INPUT_PULLUP); // enter test state
  digitalRead(powerLedPin);
  if (digitalRead(powerLedPin) == 0) state = FAIL;
  pinMode(powerLedPin, OUTPUT);
  digitalWrite(powerLedPin, valueMap.led); // return the LED to the desired state

  valueMap.selfTestResult = state;
}

// Functions to load data into the response buffer
void loadArray(uint8_t myNumber)
{
  for (uint8_t x = 0 ; x < sizeof(myNumber) ; x++)
    responseBuffer[x] = (myNumber >> (((sizeof(myNumber) - 1) - x) * 8)) & 0xFF;
  responseSize = sizeof(myNumber);
}

void loadArray(uint16_t myNumber)
{
  for (uint8_t x = 0 ; x < sizeof(myNumber) ; x++)
    responseBuffer[x] = (myNumber >> (((sizeof(myNumber) - 1) - x) * 8)) & 0xFF;
  responseSize = sizeof(myNumber);
}
