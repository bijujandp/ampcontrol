#include "display.h"

#include <avr/pgmspace.h>
#include <avr/eeprom.h>

#include "eeprom.h"
#include "input.h"
#include "tuner.h"
#include "ds18x20.h"
#include "temp.h"

int8_t brStby;											/* Brightness in standby mode */
int8_t brWork;											/* Brightness in working mode */

uint8_t spMode;											/* Spectrum mode */
uint8_t strbuf[STR_BUFSIZE + 1] = "                ";	/* String buffer */

uint8_t defDisplay = MODE_SPECTRUM;						/* Default display */

uint8_t getDefDisplay()
{
	return defDisplay;
}

void setDefDisplay(uint8_t value)
{
	defDisplay = value;

	return;
}

static void writeStringEeprom(const uint8_t *string)
{
	uint8_t i = 0;

	for (i = 0; i < STR_BUFSIZE; i++)
		strbuf[i] = eeprom_read_byte(&string[i]);

	gdWriteString(strbuf);

	return;
}

uint8_t *mkNumString(int16_t number, uint8_t width, uint8_t lead, uint8_t radix)
{
	uint8_t numdiv;
	uint8_t sign = lead;
	int8_t i;

	if (number < 0) {
		sign = '-';
		number = -number;
	}

	for (i = 0; i < width; i++)
		strbuf[i] = lead;
	strbuf[width] = '\0';
	i = width - 1;

	while (number > 0 || i == width - 1) {
		numdiv = number % radix;
		strbuf[i] = numdiv + 0x30;
		if (numdiv >= 10)
			strbuf[i] += 7;
		i--;
		number /= radix;
	}

	if (i >= 0)
		strbuf[i] = sign;

	return strbuf;
}

static void showBar(int16_t min, int16_t max, int16_t value, uint8_t *buf)
{
	uint8_t i, j;
	uint8_t color;

	uint8_t x, xbase;
	uint8_t y, ybase;

	if (min + max) {
		value = (int16_t)91 * (value - min) / (max - min);
	} else {
		value = (int16_t)45 * value / max;
	}

	for (i = 0; i < 91; i++) {
		if (((min + max) && (value <= i)) || (!(min + max) &&
			(((value > 0) && ((i < 45) || (value + 45 < i))) ||
			((value <= 0) && ((i > 45) || (value + 45 > i)))))) {
			color = 0x00;
		} else {
			color = 0x01;
		}
		if (!(i & 0x01)) {
			for (j = 29; j < 40; j++) {
				if (j == 34) {
					gdDrawPixel(i, j, 1);
				} else {
					gdDrawPixel(i, j, color);
				}
			}
		}
	}

	if (buf) {
		for (y = 0; y < GD_SIZE_Y / 8 * 3; y++) {
			for (x = 0; x < GD_SIZE_X / 4 - 1; x++) {
				xbase = x * 3;
				ybase = 63 - y;
				if (buf[x] + buf[x + 32] >= y * 3) {
					gdDrawPixel(xbase + 0, ybase, 1);
					gdDrawPixel(xbase + 1, ybase, 1);
				} else {
					gdDrawPixel(xbase + 0, ybase, 0);
					gdDrawPixel(xbase + 1, ybase, 0);
				}
			}
		}
	}

	return;
}

static void showParValue(int8_t value)
{
	gdLoadFont(font_ks0066_ru_24, 1, FONT_DIR_0);
	gdSetXY(94, 32);
	gdWriteString(mkNumString(value, 3, ' ', 10));
	gdLoadFont(font_ks0066_ru_08, 1, FONT_DIR_0);

	return;
}

static void showParLabel(const uint8_t *parLabel)
{
	gdLoadFont(font_ks0066_ru_24, 1, FONT_DIR_0);
	gdSetXY(0, 0);
	writeStringEeprom(parLabel);
	gdLoadFont(font_ks0066_ru_08, 1, FONT_DIR_0);

	return;
}

static void showParIcon(const uint8_t *icon)
{
	gdSetXY(104, 2);
	gdWriteIcon24(icon);

	return;
}

static uint8_t rc5CmdInd = CMD_RC5_STBY;
static uint8_t rc5Cmd;
static uint8_t rc5Addr;

void nextRC5Cmd(void)
{
	eeprom_update_byte(eepromRC5Cmd + rc5CmdInd, rc5Cmd);
	eeprom_update_byte(eepromRC5Addr, rc5Addr);

	rc5CmdInd++;
	if (rc5CmdInd >= CMD_BTN_1)
		rc5CmdInd = CMD_RC5_STBY;

	rc5Addr = eeprom_read_byte(eepromRC5Addr);
	rc5Cmd = eeprom_read_byte(eepromRC5Cmd + rc5CmdInd);

	setRC5Buf(rc5Addr, rc5Cmd);

	return;
}

void startTestMode(void)
{
	rc5CmdInd = CMD_RC5_STBY;
	rc5Addr = eeprom_read_byte(eepromRC5Addr);
	rc5Cmd = eeprom_read_byte(eepromRC5Cmd + rc5CmdInd);

	setRC5Buf(rc5Addr, rc5Cmd);

	return;
}

void showRC5Info(uint8_t **txtLabels)
{
	uint16_t rc5Buf = getRC5Buf();
	uint8_t btnBuf = getBtnBuf();
	uint8_t encBuf = getEncBuf();

	gdLoadFont(font_ks0066_ru_08, 1, FONT_DIR_0);
	gdSetXY(0, 0);
	writeStringEeprom(txtLabels[LABEL_IN_STATUS]);
	gdSetXY(4, 9);
	writeStringEeprom(txtLabels[LABEL_REMOTE]);
	gdSetXY(45, 9);
	gdWriteString(mkNumString(rc5Buf, 14, '0', 2));

	gdSetXY(4, 18);
	writeStringEeprom(txtLabels[LABEL_BUTTONS]);
	gdSetXY(75, 18);
	gdWriteString(mkNumString(btnBuf, 5, '0', 2));
	gdSetXY(108, 18);
	gdWriteString((uint8_t*)"/");
	gdSetXY(117, 18);
	gdWriteString(mkNumString(encBuf, 2, '0', 2));

	gdSetXY(0, 30);
	writeStringEeprom(txtLabels[LABEL_LEARN_MODE]);
	gdSetXY(4, 39);
	writeStringEeprom(txtLabels[LABEL_BUTTON]);
	gdSetXY(48, 39);
	writeStringEeprom(txtLabels[LABEL_RC5_STBY + rc5CmdInd]);

	gdSetXY(8, 48);
	writeStringEeprom(txtLabels[LABEL_ADDRESS]);
	gdSetXY(64, 48);
	rc5Addr = (rc5Buf & 0x07C0)>>6;
	gdWriteString(mkNumString(rc5Addr, 2, '0', 16));
	gdWriteString((uint8_t*)" => ");
	gdWriteString(mkNumString(eeprom_read_byte(eepromRC5Addr), 2, '0', 16));

	gdSetXY(8, 56);
	writeStringEeprom(txtLabels[LABEL_COMMAND]);
	gdSetXY(64, 56);
	rc5Cmd = rc5Buf & 0x003F;
	gdWriteString(mkNumString(rc5Cmd, 2, '0', 16));
	gdWriteString((uint8_t*)" => ");
	gdWriteString(mkNumString(eeprom_read_byte(eepromRC5Cmd + rc5CmdInd), 2, '0', 16));

	return;
}

#if !defined(LM7001)
void showTemp(uint8_t **txtLabels)
{
	int8_t tempTH;

	tempTH = getTempTH();

	gdLoadFont(font_ks0066_ru_08, 1, FONT_DIR_0);

	gdSetXY(0, 48);
	writeStringEeprom(txtLabels[LABEL_SENSOR]);
	gdWriteString((uint8_t*)" 1: ");
	gdWriteString(mkNumString(ds18x20GetTemp(0), 3, ' ', 10));
	gdWriteString((uint8_t*)" \xDF""C");

	gdSetXY(0, 56);
	writeStringEeprom(txtLabels[LABEL_SENSOR]);
	gdWriteString((uint8_t*)" 2: ");
	gdWriteString(mkNumString(ds18x20GetTemp(1), 3, ' ', 10));
	gdWriteString((uint8_t*)" \xDF""C");

	showParValue(tempTH);
	showBar(MIN_TEMP, MAX_TEMP, tempTH, 0);
	showParLabel(txtLabels[LABEL_THRESHOLD]);
	showParIcon(icons_24_threshold);

	gdSetXY(118, 56);
	gdWriteString((uint8_t*)"\xDF""C");

	return;
}
#endif

#if !defined(NOTUNER)
void showRadio(uint8_t *buf, uint8_t tune)
{
	uint16_t freq = tunerGetFreq();
	uint8_t level = tunerLevel();
	uint8_t num = stationNum(freq);

	uint8_t i;

	/* Frequency value */
	gdLoadFont(font_ks0066_ru_24, 1, FONT_DIR_0);
	gdSetXY(0, 0);
	gdWriteString((uint8_t*)"FM ");
	gdWriteString(mkNumString(freq / 100, 3, ' ', 10));
	gdWriteString((uint8_t*)"\x7F.\x7F");
	gdWriteString(mkNumString(freq % 100, 2, '0', 10));
	gdLoadFont(font_ks0066_ru_08, 1, FONT_DIR_0);

	/* Signal level */
	gdSetXY (112, 0);
	for (i = 0; i < 16; i+=2) {
		if (i <= level) {
			gdDrawLine(112 + i, 7, 112 + i, 7 - i / 2, 0x01);
		} else {
			gdDrawLine(112 + i, 6, 112 + i, 0, 0x00);
			gdDrawPixel(112 + i, 7, 0x01);
		}
	}

	/* Stereo indicator */
	gdSetXY(114, 16);
	if (tunerStereo())
		gdWriteString((uint8_t*)"ST");
	else
		gdWriteString((uint8_t*)"  ");

	/* Station number */
	if (num) {
		showParValue(num);
	} else {
		gdLoadFont(font_ks0066_ru_24, 1, FONT_DIR_0);
		gdSetXY(94, 32);
		gdWriteString((uint8_t*)" --");
		gdLoadFont(font_ks0066_ru_08, 1, FONT_DIR_0);
	}

	/* Frequency scale */
	showBar(FM_FREQ_MIN>>4, FM_FREQ_MAX>>4, freq>>4, buf);

	if (tune) {
		gdSetXY(103, 56);
		gdWriteString((uint8_t*)"\xDB\xDB\xD0\xDC\xDC");
	}

	return;
}
#endif

static void drawMiniSpectrum(uint8_t *buf)
{
	uint8_t x, xbase;
	uint8_t y, ybase;

	if (buf) {
		for (y = 0; y < GD_SIZE_Y / 8 * 4; y++) {
			for (x = 0; x < GD_SIZE_X / 4 - 1; x++) {
				xbase = x * 3;
				ybase = 63 - y;
				if (buf[x] + buf[x + 32] >= y * 2) {
					gdDrawPixel(xbase + 0, ybase, 1);
					gdDrawPixel(xbase + 1, ybase, 1);
				} else {
					gdDrawPixel(xbase + 0, ybase, 0);
					gdDrawPixel(xbase + 1, ybase, 0);
				}
			}
		}
	}

	return;
}

void showMute(uint8_t **txtLabels, uint8_t *buf)
{
	showParLabel(txtLabels[LABEL_MUTE]);

	gdSetXY(96, 32);

	if (getMute()) {
		gdWriteIcon32(icons_32_mute_on);
	} else {
		gdWriteIcon32(icons_32_mute_off);
	}

	drawMiniSpectrum(buf);

	return;
}

void showLoudness(uint8_t **txtLabels, uint8_t *buf)
{
	showParLabel(txtLabels[LABEL_LOUDNESS]);

	gdSetXY(96, 32);

	if (getLoudness()) {
		gdWriteIcon32(icons_32_loud_on);
	} else {
		gdWriteIcon32(icons_32_loud_off);
	}

	drawMiniSpectrum(buf);

	return;
}

/* Show brightness control */
void showBrWork(uint8_t **txtLabels, uint8_t *buf)
{
	showParValue(brWork);
	showBar(GD_MIN_BRIGHTNESS, GD_MAX_BRIGTHNESS, brWork, buf);
	showParLabel(txtLabels[LABEL_BR_WORK]);
	showParIcon(icons_24_brightness);

	return;
}

void changeBrWork(int8_t diff)
{
	brWork += diff;
	if (brWork > GD_MAX_BRIGTHNESS)
		brWork = GD_MAX_BRIGTHNESS;
	if (brWork < GD_MIN_BRIGHTNESS)
		brWork = GD_MIN_BRIGHTNESS;
	gdSetBrightness(brWork);

	return;
}


/* Show audio parameter */
void showSndParam(sndParam *param, uint8_t **txtLabels, uint8_t *buf)
{
	showParValue(((int16_t)(param->value) * param->step + 4) >> 3);
	showBar(param->min, param->max, param->value, buf);
	showParLabel(param->label);
	showParIcon(param->icon);
	gdSetXY(116, 56);
	writeStringEeprom(txtLabels[LABEL_DB]);

	return;
}

static void drawTm(uint8_t tm, const uint8_t *font)
{
	if (getEtm() == tm)
		gdLoadFont(font, 0, FONT_DIR_0);
	else
		gdLoadFont(font, 1, FONT_DIR_0);
	gdWriteString(mkNumString(getTime(tm), 2, '0', 10));
	gdLoadFont(font, 1, FONT_DIR_0);

	return;
}

void showTime(uint8_t **txtLabels)
{
	gdSetXY(4, 0);

	drawTm(DS1307_HOUR, font_digits_32);
	gdWriteString((uint8_t*)"\x7F:\x7F");
	drawTm(DS1307_MIN, font_digits_32);
	gdWriteString((uint8_t*)"\x7F:\x7F");
	drawTm(DS1307_SEC, font_digits_32);

	gdSetXY(9, 32);

	drawTm(DS1307_DATE, font_ks0066_ru_24);
	gdWriteString((uint8_t*)"\x7F.\x7F");
	drawTm(DS1307_MONTH, font_ks0066_ru_24);
	gdWriteString((uint8_t*)"\x7F.\x7F");
	if (getEtm() == DS1307_YEAR)
		gdLoadFont(font_ks0066_ru_24, 0, FONT_DIR_0);
	gdWriteString((uint8_t*)"20");
	gdWriteString((uint8_t*)"\x7F");
	drawTm(DS1307_YEAR, font_ks0066_ru_24);

	gdLoadFont(font_ks0066_ru_08, 1, FONT_DIR_0);
	gdSetXY(32, 56);

	writeStringEeprom(txtLabels[LABEL_SUNDAY + (getTime(DS1307_WDAY) - 1) % 7]);

	return;
}

static void drawAm(uint8_t am, const uint8_t *font)
{
	if (getEam() == am)
		gdLoadFont(font, 0, FONT_DIR_0);
	else
		gdLoadFont(font, 1, FONT_DIR_0);
	gdWriteString(mkNumString(getAlarm(am), 2, '0', 10));
	gdLoadFont(font, 1, FONT_DIR_0);

	return;
}

void showAlarm(uint8_t **txtLabels)
{
	uint8_t i, j;
	uint8_t *label;

	gdSetXY(4, 0);

	drawAm(DS1307_A0_HOUR, font_digits_32);
	gdWriteString((uint8_t*)"\x7F:\x7F");
	drawAm(DS1307_A0_MIN, font_digits_32);

	/* Draw input icon selection rectangle */
	if (getEam() == DS1307_A0_INPUT) {
		gdDrawRect(96, 0, 32, 32, 1);
		gdDrawRect(97, 1, 30, 30, 1);
	} else {
		gdDrawRect(96, 0, 32, 32, 0);
		gdDrawRect(97, 1, 30, 30, 0);
	}

	gdSetXY(100, 4);
	/* Check that input number less than CHAN_CNT */
	i = getAlarm(DS1307_A0_INPUT);
	if (i >= CHAN_CNT)
		i = 0;
	gdWriteIcon24(sndParAddr(SND_GAIN0 + i)->icon);

	/* Draw weekdays selection rectangle */
	if (getEam() == DS1307_A0_WDAY) {
		gdDrawRect(0, 34, 128, 30, 1);
		gdDrawRect(1, 35, 126, 28, 1);
	} else {
		gdDrawRect(0, 34, 128, 30, 0);
		gdDrawRect(1, 35, 126, 28, 0);
	}

	/* Draw weekdays */
	gdLoadFont(font_ks0066_ru_08, 1, FONT_DIR_0);
	for (i = 0; i < 7; i++) {
		gdSetXY(5 + 18 * i, 38);
		j = 0;
		label = txtLabels[LABEL_SUNDAY + (i + 1) % 7];
		strbuf[0] = eeprom_read_byte(&label[j++]);
		while (strbuf[0] == ' ')
			strbuf[0] = eeprom_read_byte(&label[j++]);
		strbuf[1] = eeprom_read_byte(&label[j++]);
		strbuf[2] = '\0';
		gdWriteString(strbuf);

		gdDrawRect(3 + 18 * i, 47, 14, 14, 1);
		if (getAlarm(DS1307_A0_WDAY) & (0x40 >> i))
			gdDrawFilledRect(5 + 18 * i, 49, 10, 10, 1);
		else
			gdDrawFilledRect(5 + 18 * i, 49, 10, 10, 0);
	}

	return;
}

void showTimer(uint8_t *buf)
{
	uint8_t x, xbase;
	uint8_t y, ybase;

	int16_t stbyTimer = getStbyTimer();

	gdSetXY(4, 0);

	gdLoadFont(font_digits_32, 1, FONT_DIR_0);
	if (stbyTimer >= 0) {
		gdWriteString(mkNumString(stbyTimer / 3600, 2, '0', 10));
		gdWriteString((uint8_t*)"\x7F:\x7F");
		gdWriteString(mkNumString(stbyTimer / 60 % 60, 2, '0', 10));
		gdWriteString((uint8_t*)"\x7F:\x7F");
		gdWriteString(mkNumString(stbyTimer % 60, 2, '0', 10));
	} else {
		gdWriteString((uint8_t*)"--");
		gdWriteString((uint8_t*)"\x7F:\x7F");
		gdWriteString((uint8_t*)"--");
		gdWriteString((uint8_t*)"\x7F:\x7F");
		gdWriteString((uint8_t*)"--");
	}
	gdLoadFont(font_ks0066_ru_08, 1, FONT_DIR_0);

	for (y = 0; y < GD_SIZE_Y / 2; y++) {
		for (x = 0; x < GD_SIZE_X / 4; x++) {
			xbase = x << 2;
			ybase = 63 - y;
			if (buf[x] + buf[x + 32] >= y * 2) {
				gdDrawPixel(xbase + 0, ybase, 1);
				gdDrawPixel(xbase + 1, ybase, 1);
				gdDrawPixel(xbase + 2, ybase, 1);
			} else {
				gdDrawPixel(xbase + 0, ybase, 0);
				gdDrawPixel(xbase + 1, ybase, 0);
				gdDrawPixel(xbase + 2, ybase, 0);
			}
		}
	}
	return;
}

void drawSpectrum(uint8_t *buf, uint8_t **txtLabels)
{
	uint8_t x, xbase;
	uint8_t y, ybase;
	uint16_t left, right;

	switch (spMode) {
	case SP_MODE_STEREO:
		for (y = 0; y < GD_SIZE_Y / 2; y++) {
			for (x = 0; x < GD_SIZE_X / 4; x++) {
				xbase = x << 2;
				ybase = 31 - y;
				if (buf[x] >= y) {
					gdDrawPixel(xbase + 0, ybase, 1);
					gdDrawPixel(xbase + 1, ybase, 1);
					gdDrawPixel(xbase + 2, ybase, 1);
				} else {
					gdDrawPixel(xbase + 0, ybase, 0);
					gdDrawPixel(xbase + 1, ybase, 0);
					gdDrawPixel(xbase + 2, ybase, 0);
				}
				ybase = 63 - y;
				if (buf[x + 32] >= y) {
					gdDrawPixel(xbase + 0, ybase, 1);
					gdDrawPixel(xbase + 1, ybase, 1);
					gdDrawPixel(xbase + 2, ybase, 1);
				} else {
					gdDrawPixel(xbase + 0, ybase, 0);
					gdDrawPixel(xbase + 1, ybase, 0);
					gdDrawPixel(xbase + 2, ybase, 0);
				}
			}
		}
		break;
	case SP_MODE_METER:
		gdLoadFont(font_ks0066_ru_08, 1, FONT_DIR_0);
		gdSetXY(0, 0);
		writeStringEeprom(txtLabels[LABEL_LEFT_CHANNEL]);
		gdSetXY(0, 36);
		writeStringEeprom(txtLabels[LABEL_RIGHT_CHANNEL]);
		left = 0;
		right = 0;
		for (x = 0; x < GD_SIZE_X / 4; x++) {
			left += buf[x];
			right += buf[x + 32];
		}
		left >>= 3;
		right >>= 3;
		for (x = 0; x < GD_SIZE_X; x++) {
			if (x % 3 != 2) {
				for (y = 12; y < 27; y++) {
					if (x < left || y == 19) {
						gdDrawPixel(x, y, 1);
					} else {
						gdDrawPixel(x, y, 0);
					}
				}
				for (y = 48; y < 63; y++) {
					if (x < right || y == 55) {
						gdDrawPixel(x, y, 1);
					} else {
						gdDrawPixel(x, y, 0);
					}
				}
			}
		}
		break;
	default:
		for (y = 0; y < GD_SIZE_Y; y++) {
			for (x = 0; x < GD_SIZE_X / 4; x++) {
				xbase = x << 2;
				ybase = 63 - y;
				if (buf[x] + buf[x + 32] >= y) {
					gdDrawPixel(xbase + 0, ybase, 1);
					gdDrawPixel(xbase + 1, ybase, 1);
					gdDrawPixel(xbase + 2, ybase, 1);
				} else {
					gdDrawPixel(xbase + 0, ybase, 0);
					gdDrawPixel(xbase + 1, ybase, 0);
					gdDrawPixel(xbase + 2, ybase, 0);
				}
			}
		}
		break;
	}

	return;
}

void setWorkBrightness(void)
{
	gdSetBrightness(brWork);

	return;
}

void setStbyBrightness(void)
{
	gdSetBrightness(brStby);

	return;
}

void loadDispParams(void)
{
	brStby = eeprom_read_byte(eepromBrStby);
	brWork = eeprom_read_byte(eepromBrWork);
	spMode  = eeprom_read_byte(eepromSpMode);
	defDisplay = eeprom_read_byte(eepromDisplay);

	return;
}

void saveDisplayParams(void)
{
	eeprom_update_byte(eepromBrStby, brStby);
	eeprom_update_byte(eepromBrWork, brWork);
	eeprom_update_byte(eepromSpMode, spMode);
	eeprom_update_byte(eepromDisplay, defDisplay);

	return;
}

/* Change spectrum mode */
void switchSpMode()
{
	if (++spMode > SP_MODE_MIXED)
		spMode = SP_MODE_STEREO;

	return;
}
