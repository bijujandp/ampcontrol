#include "ks0108.h"

#include <avr/pgmspace.h>

const uint8_t *_font;
static uint8_t _cs, _row, _col;
static uint8_t csInv;		/* 0 for WG12864A, 1 for WG12864B */

static fontParams fp;

static inline void setPortCS()
{
	if (csInv) {
		GD_CHIP_PORT |= (GD_CS1 | GD_CS2);
		GD_CHIP_PORT &= ~_cs;
	} else {
		GD_CHIP_PORT &= ~(GD_CS1 | GD_CS2);
		GD_CHIP_PORT |= _cs;
	}
	return;
}

static void writeStrob()
{
	asm("nop");	/* 120ns */
	asm("nop");
	GD_CONTROL_PORT |= GD_E;
	asm("nop");	/* 360ns */
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	GD_CONTROL_PORT &= ~GD_E;

	return;
}

static uint8_t readStrob()
{
	uint8_t pin;

	asm("nop");	/* 120ns */
	asm("nop");
	GD_CONTROL_PORT |= GD_E;
	asm("nop");	/* 300ns */
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	pin = GD_DATA_PIN;
	GD_CONTROL_PORT &= ~GD_E;

	return pin;
}

uint8_t gdReadStatus()
{
	uint8_t status;

	GD_DATA_DDR = 0x00;

	setPortCS();

	GD_CONTROL_PORT |= GD_RW;
	GD_CONTROL_PORT &= ~GD_DI;

	status = readStrob();

	return status;
}

void waitWhile(uint8_t status)
{
	uint8_t i = 0;
	while(gdReadStatus() & status) {
		if (i++ > 200)	/* Avoid endless loop */
			return;
	}
	return;
}

uint8_t gdReadData()
{
	uint8_t data;

	waitWhile(KS0108_STA_BUSY);
	GD_CONTROL_PORT |= GD_DI;

	writeStrob();

	waitWhile(KS0108_STA_BUSY);
	GD_CONTROL_PORT |= GD_DI;

	data = readStrob();

	return data;
}

void gdWriteCommand(uint8_t command)
{
	waitWhile(KS0108_STA_BUSY);

	GD_DATA_DDR = 0xFF;

	setPortCS();

	GD_CONTROL_PORT &= ~GD_RW;
	GD_CONTROL_PORT &= ~GD_DI;

	GD_DATA_PORT = command;

	writeStrob();

	return;
}

void gdWriteData(uint8_t data)
{
	waitWhile(KS0108_STA_BUSY);

	GD_DATA_DDR = 0xFF;

	setPortCS();

	GD_CONTROL_PORT &= ~GD_RW;
	GD_CONTROL_PORT |= GD_DI;

	GD_DATA_PORT = data;

	writeStrob();

	if (++_col == 64) {
		_col = 0;

		if (_cs & GD_CS2)
			_row += fp.height;
		if (_row == GD_ROWS)
			_row = 0;

		if (_cs == GD_CS1)
			_cs = GD_CS2;
		else if (_cs == GD_CS2)
			_cs = GD_CS1;

		gdWriteCommand(KS0108_SET_ADDRESS);
		gdWriteCommand(KS0108_SET_PAGE + _row);
	}
	return;
}

void gdFill(uint8_t data)
{
	uint8_t i, j;
	uint8_t cs = _cs;

	_cs = GD_CS1 | GD_CS2;
	gdWriteCommand(KS0108_SET_ADDRESS + _col);
	gdWriteCommand(KS0108_SET_PAGE + _row);

	for (i = 0; i < GD_ROWS; i++)
		for (j = 0; j < GD_COLS; j++)
			gdWriteData(data);
	_cs = cs;

	return;
}

void testDisplay() {
	const uint8_t wrData[] = {0x55, 0xAA};
	uint8_t rdData[] = {0, 0};
	uint8_t i;

	csInv = 0;

	for (i = 0; i < 2; i++) {
		gdWriteCommand(KS0108_SET_ADDRESS);
		gdWriteCommand(KS0108_SET_PAGE);
		gdWriteData(wrData[i]);

		gdWriteCommand(KS0108_SET_ADDRESS);
		gdWriteCommand(KS0108_SET_PAGE);
		rdData[i] = gdReadData();
	}

	if (wrData[0] != rdData[0] || wrData[1] != rdData[1])
		csInv = 1;

	return;
}

void gdInit(void)
{
	/* Set control lines as outputs */
	GD_CONTROL_DDR |= GD_DI | GD_RW | GD_E;
	GD_CHIP_DDR |= GD_CS1 | GD_CS2 | GD_RES;

	GD_BACKLIGHT_DDR |= GD_BCKL;
	GD_BACKLIGHT_PORT &= ~GD_BCKL; /* Turn off backlight */

	/* Reset */
	GD_CHIP_PORT &= ~(GD_RES);
	asm("nop");
	asm("nop");
	GD_CHIP_PORT |= GD_RES;
	asm("nop");
	asm("nop");

	/* Clear display  and reset addresses */
	_cs = GD_CS1 | GD_CS2;

	testDisplay();

	gdWriteCommand(KS0108_DISPLAY_START_LINE);
	gdWriteCommand(KS0108_SET_ADDRESS);
	gdWriteCommand(KS0108_SET_PAGE);

	fp.height = 1;
	gdFill(0x00);

	gdWriteCommand(KS0108_DISPLAY_ON);

	_row = 0;
	_col = 0;
	_cs = GD_CS2;

	return;
}

void gdSetXY(uint8_t x, uint8_t y)
{
	if (x > ((GD_COLS << 1) - 1))
		y += fp.height;

	if ((x & ((GD_COLS << 1) - 1)) < GD_COLS)
		_cs = GD_CS1;
	else
		_cs = GD_CS2;

	_col = x & (GD_COLS - 1);
	_row = y & (GD_ROWS - 1);

	gdWriteCommand(KS0108_SET_ADDRESS + _col);
	gdWriteCommand(KS0108_SET_PAGE + _row);

}

void gdLoadFont(const uint8_t *font, uint8_t color)
{
	_font = font + 5;
	fp.height = pgm_read_byte(font);
	fp.ltsppos = pgm_read_byte(font + 1);
	fp.ccnt = pgm_read_byte(font + 2);
	fp.ofta = pgm_read_byte(font + 3);
	fp.oftna = pgm_read_byte(font + 4);
	fp.color = color;
}

void gdWriteChar(uint8_t code)
{
	/* Store current position before writing to display */
	uint8_t cs = _cs;
	uint8_t row = _row;
	uint8_t col = _col;
	if (cs == GD_CS2)
		col += GD_COLS;

	uint8_t i, j;

	uint8_t pgmData;

	uint8_t spos = code - ((code >= 128) ? fp.oftna : fp.ofta);

	uint16_t oft = 0;	/* Current symbol offset in array*/
	uint8_t swd = 0;	/* Current symbol width */

	for (i = 0; i < spos; i++) {
		swd = pgm_read_byte(_font + i);
		oft += swd;
	}
	swd = pgm_read_byte(_font + spos);
	oft *= fp.height;
	oft += fp.ccnt;

	for (j = 0; j < fp.height; j++) {
		gdSetXY(col, row + j);
		for (i = 0; i < swd; i++) {
			pgmData = pgm_read_byte(_font + oft + (swd * j) + i);
			if (fp.color)
				gdWriteData(pgmData);
			else
				gdWriteData(~pgmData);
		}
	}

	gdSetXY(col + swd, row);

	return;
}

void gdWriteString(uint8_t *string)
{
	if (*string)
		gdWriteChar(*string++);
	while(*string) {
		gdWriteChar(fp.ltsppos);
		gdWriteChar(*string++);
	}

	return;
}