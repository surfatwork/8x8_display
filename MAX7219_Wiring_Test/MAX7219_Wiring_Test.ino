
/*
  MAX7219 8x8 Matrix Wiring Test
  For Arduino Uno/Nano

  DIN  -> D11
  CLK  -> D13
  CS   -> D10

  Test sequence:
  1. All LEDs ON
  2. All LEDs OFF
  3. Columns sweep left->right
  4. Rows sweep top->bottom
  5. Checkerboard
*/

#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 1
#define CS_PIN 10
#define CLK_PIN   13
#define DATA_PIN  11

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

void clearDisplay()
{
  mx.clear();
  mx.update();
}

void setup()
{
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 1);
  mx.clear();
  mx.update();
  delay(1000);
}

void loop()
{
  // All ON
  for (uint8_t c = 0; c < 8; c++)
    mx.setColumn(c, 0xFF);
  delay(2000);

  // All OFF
  clearDisplay();
  delay(1000);

  // Column sweep
  for (uint8_t c = 0; c < 8; c++)
  {
    clearDisplay();
    mx.setColumn(c, 0xFF);
    delay(200);
  }

  // Row sweep
  for (uint8_t r = 0; r < 8; r++)
  {
    clearDisplay();
    for (uint8_t c = 0; c < 8; c++)
      mx.setPoint(r, c, true);
    delay(200);
  }

  // Checkerboard
  clearDisplay();
  for (uint8_t r = 0; r < 8; r++)
    for (uint8_t c = 0; c < 8; c++)
      if ((r + c) & 1)
        mx.setPoint(r, c, true);

  delay(1000);
}