// Executes when data is received on I2C
// this function is registered as an event, see setup() and/or startI2C()
void receiveEvent(int numberOfBytesReceived)
{
  if (Wire.available() == 0) return; // Prevents double-firing the interrupt
  incomingDataSpot = 0;  
  memset(incomingData, 0, sizeof(incomingData));
  
  while (Wire.available())
  {
    currentRegisterNumber = Wire.read();
    #if DEBUG
    Serial.print("# Incoming data: ");
    //incomingData is 32 bytes. We shouldn't spill over because receiveEvent can't receive more than 32 bytes
    #endif
    while (Wire.available())
    {
      incomingData[incomingDataSpot++] = Wire.read();
      #if DEBUG
      Serial.print(char(incomingData[incomingDataSpot-1]), HEX);
      //incomingData is 32 bytes. We shouldn't spill over because receiveEvent can't receive more than 32 bytes
      #endif
    }
  }

  
  for (uint16_t regNum = 0; regNum < (sizeof(memoryMapRegs) / sizeof(uint8_t)); regNum++)
  {
    if (functions[regNum].registerNumber == currentRegisterNumber)
    {
      functions[regNum].handleFunction((char *)incomingData);
    }
  }
}


/*   All bus transactions start as a receiveEvent, and the relevant command will queue data into
 *   the responseBuffer.
 *   The requestEvent simple dequeues the response buffer to the bus.
 */
void requestEvent() {
  Wire.write(responseBuffer, responseSize);
}
