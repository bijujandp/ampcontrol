#ifndef DISPLAY_H
#define DISPLAY_H

#include <inttypes.h>
#include "ds1307.h"
#include "audio/audioproc.h"

#ifdef KS0066_16X2
#define KS0066
#endif

#ifdef KS0066

#else
#include "display/gdfb.h"
#endif

/* Backlight state */
#define BACKLIGHT_ON			1
#define BACKLIGHT_OFF			0

/* Radio tuning mode */
#define MODE_RADIO_TUNE			1
#define MODE_RADIO_CHAN			0

/* String buffer */
#define STR_BUFSIZE				16

/* Spectrum output mode */
enum {
	SP_MODE_STEREO = 0,
	SP_MODE_METER,
	SP_MODE_MIXED,
	SP_MODE_END
};

enum {
	FALL_SPEED_LOW = 0,
	FALL_SPEED_MIDDLE,
	FALL_SPEED_FAST
};

void displayInit(void);
void displayClear(void);

void writeString(uint8_t *string);
void writeStringEeprom(const uint8_t *string);
void writeNum(int16_t number, uint8_t width, uint8_t lead, uint8_t radix);

uint8_t **getTxtLabels(void);

void setDefDisplay(uint8_t value);
uint8_t getDefDisplay();

void nextRC5Cmd(void);
void startTestMode(void);
void showRC5Info(void);

void showTemp(void);

void showRadio(uint8_t tune);

void showMute(void);
void showLoudness(void);

void showBrWork(void);
void changeBrWork(int8_t diff);

void showSndParam(sndMode mode);

void showTime(void);
void showAlarm(void);
void showTimer(int16_t timer);

void switchSpMode(void);
void switchFallSpeed(void);
void showSpectrum(void);

void setWorkBrightness(void);
void setStbyBrightness(void);

void displayPowerOff(void);

#endif /* DISPLAY_H */
