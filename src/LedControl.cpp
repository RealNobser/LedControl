/*
 *    LedControl.cpp - A library for controling Leds with a MAX7219/MAX7221
 *    Copyright (c) 2007 Eberhard Fahle
 *
 *    Permission is hereby granted, free of charge, to any person
 *    obtaining a copy of this software and associated documentation
 *    files (the "Software"), to deal in the Software without
 *    restriction, including without limitation the rights to use,
 *    copy, modify, merge, publish, distribute, sublicense, and/or sell
 *    copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following
 *    conditions:
 *
 *    This permission notice shall be included in all copies or
 *    substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *    OTHER DEALINGS IN THE SOFTWARE.
 */

#include "LedControl.h"

// the opcodes for the MAX7221 and MAX7219
#define OP_NOOP 0
#define OP_DIGIT0 1
#define OP_DIGIT1 2
#define OP_DIGIT2 3
#define OP_DIGIT3 4
#define OP_DIGIT4 5
#define OP_DIGIT5 6
#define OP_DIGIT6 7
#define OP_DIGIT7 8
#define OP_DECODEMODE 9
#define OP_INTENSITY 10
#define OP_SCANLIMIT 11
#define OP_SHUTDOWN 12
#define OP_DISPLAYTEST 15

LedControl::LedControl(const uint8_t dataPin, const uint8_t clkPin, const uint8_t csPin, const uint8_t numDevices)
{
	SPI_MOSI = dataPin;
	SPI_CLK = clkPin;
	SPI_CS = csPin;

	maxDevices = numDevices;

	if (maxDevices > 8)
		maxDevices = 8;

	maxDevices = numDevices;

	pinMode(SPI_MOSI, OUTPUT);
	pinMode(SPI_CLK, OUTPUT);
	pinMode(SPI_CS, OUTPUT);

	digitalWrite(SPI_CS, HIGH);
	SPI_MOSI = dataPin;

	for (uint8_t i = 0; i < 64; i++)
		status[i] = 0x00;

	for (uint8_t i = 0; i < maxDevices; i++)
	{
		spiTransfer(i, OP_DISPLAYTEST, 0);
		// scanlimit is set to max on startup
		setScanLimit(i, 7);
		// decode is done in source
		spiTransfer(i, OP_DECODEMODE, 0);
		clearDisplay(i);
		// we go into shutdown-mode on startup
		shutdown(i, true);
	}
}

uint8_t LedControl::getDeviceCount()
{
	return maxDevices;
}

void LedControl::shutdown(const uint8_t addr, const bool b)
{
	if (addr >= maxDevices)
		return;

	if (b)
		spiTransfer(addr, OP_SHUTDOWN, 0);
	else
		spiTransfer(addr, OP_SHUTDOWN, 1);
}

void LedControl::setScanLimit(const uint8_t addr, const uint8_t limit)
{
	if (addr >= maxDevices)
		return;

	if (limit >= 0 && limit < 8)
		spiTransfer(addr, OP_SCANLIMIT, limit);
}

void LedControl::setIntensity(const uint8_t addr, const uint8_t intensity)
{
	if (addr >= maxDevices)
		return;

	if (intensity >= 0 && intensity < 16)
		spiTransfer(addr, OP_INTENSITY, intensity);
}

void LedControl::clearDisplay(const uint8_t addr)
{
	uint8_t offset;

	if (addr >= maxDevices)
		return;

	offset = addr * 8;
	for (uint8_t i = 0; i < 8; i++)
	{
		status[offset + i] = 0;
		spiTransfer(addr, i + 1, status[offset + i]);
	}
}

void LedControl::setLed(const uint8_t addr, const uint8_t row, const uint8_t column, const bool state)
{
	uint8_t offset;
	uint8_t val = 0x00;

	if (addr >= maxDevices)
		return;

	if (row > 7 || column > 7)
		return;

	offset = addr * 8;
	val = B10000000 >> column;
	if (state)
		status[offset + row] = status[offset + row] | val;
	else
	{
		val = ~val;
		status[offset + row] = status[offset + row] & val;
	}
	spiTransfer(addr, row + 1, status[offset + row]);
}

void LedControl::setRow(const uint8_t addr, const uint8_t row, const uint8_t value)
{
	uint8_t offset;
	if (addr >= maxDevices)
		return;

	if (row > 7)
		return;

	offset = addr * 8;
	status[offset + row] = value;
	spiTransfer(addr, row + 1, status[offset + row]);
}

void LedControl::setColumn(const uint8_t addr, const uint8_t col, const uint8_t value)
{
	uint8_t val;

	if (addr >= maxDevices)
		return;

	if (col > 7)
		return;

	for (uint8_t row = 0; row < 8; row++)
	{
		val = value >> (7 - row);
		val = val & 0x01;
		setLed(addr, row, col, val);
	}
}

#ifdef INLCUDE_DIGITS
void LedControl::setDigit(const uint8_t addr, const uint8_t digit, const uint8_t value, const bool dp)
{
	uint8_t offset;
	uint8_t v;

	if (addr >= maxDevices)
		return;

	if (digit > 7 || value > 15)
		return;

	offset = addr * 8;
	v = pgm_read_byte_near(charTable + value);
	if (dp)
		v |= B10000000;
	status[offset + digit] = v;
	spiTransfer(addr, digit + 1, v);
}

void LedControl::setChar(const uint8_t addr, const uint8_t digit, const char value, const bool dp)
{
	uint8_t offset;
	uint8_t index, v;

	if (addr >= maxDevices)
		return;

	if (digit > 7)
		return;

	offset = addr * 8;
	index = (uint8_t)value;
	if (index > 127)
	{
		// no defined beyond index 127, so we use the space char
		index = 32;
	}
	v = pgm_read_byte_near(charTable + index);
	if (dp)
		v |= B10000000;
	status[offset + digit] = v;
	spiTransfer(addr, digit + 1, v);
}
#endif

void LedControl::spiTransfer(const uint8_t addr, const volatile uint8_t opcode, const volatile uint8_t data)
{
	// Create an array with the data to shift out
	uint8_t offset = addr * 2;
	uint8_t maxbytes = maxDevices * 2;

	for (uint8_t i = 0; i < maxbytes; i++)
		spidata[i] = 0;

	// put our device data into the array
	spidata[offset + 1] = opcode;
	spidata[offset] = data;
	// enable the line
	digitalWrite(SPI_CS, LOW);
	// Now shift out the data
	for (uint8_t i = maxbytes; i > 0; i--)
		shiftOut(SPI_MOSI, SPI_CLK, MSBFIRST, spidata[i - 1]);
	// latch the data onto the display
	digitalWrite(SPI_CS, HIGH);
}
