#include "audioproc.h"

#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "../eeprom.h"
#include "../display.h"
#include "../pins.h"

static const sndGrid grid_0_0_0             PROGMEM = {  0,  0, 0.00 * 8};	/* Not implemented */
static const sndGrid grid_n79_0_1           PROGMEM = {-79,  0, 1.00 * 8};	/* -79..0dB with 1dB step */
static const sndGrid grid_n14_14_2          PROGMEM = { -7,  7, 2.00 * 8};	/* -14..14dB with 2dB step */
static const sndGrid grid_n47_0_1           PROGMEM = {-47,  0, 1.00 * 8};	/* -47..0dB with 1dB step */
static const sndGrid grid_n15_15_1          PROGMEM = {-15, 15, 1.00 * 8};	/* -15..15dB with 1dB step */
static const sndGrid grid_0_30_2            PROGMEM = {  0, 15, 2.00 * 8};	/* 0..30dB with 2dB step */
static const sndGrid grid_n78d75_0_1d25     PROGMEM = {-63,  0, 1.25 * 8};	/* -78.75..0dB with 1.25dB step*/
static const sndGrid grid_n18d75_18d75_1d25 PROGMEM = {-15, 15, 1.25 * 8};	/* -18.75..18.75dB with 1.25dB step */
static const sndGrid grid_0_11d25_3d75      PROGMEM = {  0,  3, 3.75 * 8};	/* 0..11.25dB with 3.75dB step */
static const sndGrid grid_0_18d75_6d25      PROGMEM = {  0,  3, 6.25 * 8};	/* 0..18.75dB with 6.25dB step */
static const sndGrid grid_n15_0_1           PROGMEM = {-15,  0, 1.00 * 8};	/* -15..0dB with 1dB step */
static const sndGrid grid_n96_31_1          PROGMEM = {-96, 31, 1.00 * 8};	/* -96..31dB with 1dB step */
static const sndGrid grid_n7_7_1            PROGMEM = { -7,  7, 1.00 * 8};	/* -7..7dB with 1dB step */
static const sndGrid grid_0_6_6             PROGMEM = {  0,  1, 6.00 * 8};	/* 0..6dB with 6dB step */
static const sndGrid grid_n66_20_2          PROGMEM = {-33, 10, 2.00 * 8};	/* -66..20dB with 2dB step */
static const sndGrid grid_n12_15_3          PROGMEM = { -4,  5, 3.00 * 8};	/* -12..15dB with 3dB step */
static const sndGrid grid_n12_12_3          PROGMEM = { -4,  4, 3.00 * 8};	/* -12..12dB with 3dB step */

sndParam sndPar[MODE_SND_END];
static audioProc _aproc;

static uint8_t _inCnt;
static uint8_t _input;
static uint8_t _mute;
static uint8_t _loudness;
static uint8_t _surround;
static uint8_t _effect3d;
static uint8_t _toneDefeat;

static void setNothing(void)
{
	return;
}

void sndInit(uint8_t extFunc)
{
	uint8_t i;

	uint8_t **txtLabels = getTxtLabels();

	/* Load audio parameters stored in eeprom */
	for (i = 0; i < MODE_SND_END; i++) {
		sndPar[i].value = eeprom_read_byte((uint8_t*)EEPROM_VOLUME + i);
		sndPar[i].label = txtLabels[MODE_SND_VOLUME + i];
	}
	_loudness = eeprom_read_byte((uint8_t*)EEPROM_LOUDNESS);
	_surround = eeprom_read_byte((uint8_t*)EEPROM_SURROUND);
	_effect3d = eeprom_read_byte((uint8_t*)EEPROM_EFFECT3D);
	_toneDefeat = eeprom_read_byte((uint8_t*)EEPROM_TONE_DEFEAT);
	_inCnt = eeprom_read_byte((uint8_t*)EEPROM_MAX_INPUT_CNT);
	_aproc = eeprom_read_byte((uint8_t*)EEPROM_AUDIOPROC);
	_input = eeprom_read_byte((uint8_t*)EEPROM_INPUT);
	if (_aproc >= AUDIOPROC_END)
		_aproc = AUDIOPROC_TDA7439;

#ifdef EXTFUNC
	if (_aproc == AUDIOPROC_PGA2310 && extFunc == USE_PGA2310)
		pga2310Init(sndPar);
#endif

	/* Init grid and functions with empty values */
	for (i = 0; i < MODE_SND_END; i++) {
		sndPar[i].grid = &grid_0_0_0;
		sndPar[i].set = setNothing;
	}

#ifndef KS0066
	uint8_t ic;

	/* Setup icons */
	for (i = 0; i < MODE_SND_END; i++)
		sndPar[i].icon = i;
	/* Update input icons */
	for (i = 0; i < MODE_SND_END - MODE_SND_GAIN0; i++) {
		ic = eeprom_read_byte((uint8_t*)(EEPROM_INPUT_ICONS + i));
		if (ic < ICON24_END)
			sndPar[MODE_SND_GAIN0 + i].icon = ic;
	}
#endif

	// Setup inputs
	static uint8_t inCnt;
	switch (_aproc) {
	case AUDIOPROC_TDA7439:
		inCnt = TDA7439_IN_CNT;
		break;
	case AUDIOPROC_TDA7312:
		inCnt = TDA7312_IN_CNT;
		break;
	case AUDIOPROC_TDA7313:
		inCnt = TDA7313_IN_CNT;
		break;
	case AUDIOPROC_TDA7314:
		inCnt = TDA7314_IN_CNT;
		break;
	case AUDIOPROC_TDA7315:
		inCnt = TDA7315_IN_CNT;
		break;
	case AUDIOPROC_TDA7318:
		inCnt = TDA7318_IN_CNT;
		break;
	case AUDIOPROC_PT2314:
		inCnt = PT2314_IN_CNT;
		break;
	case AUDIOPROC_TDA7448:
		inCnt = TDA7448_IN_CNT;
		break;
	case AUDIOPROC_PT232X:
		inCnt = PT2323_IN_CNT;
		break;
	case AUDIOPROC_TEA6330:
		inCnt = TEA6330_IN_CNT;
		break;
#ifdef EXTFUNC
	case AUDIOPROC_PGA2310:
		inCnt = PGA2310_IN_CNT;
		break;
#endif
	default:
		inCnt = 1;
		break;
	}
	// Limit global input count
	if (_inCnt > inCnt || _inCnt == 0)
		_inCnt = inCnt;
	// Limit current input
	if (_input >= inCnt)
		_input = 0;

	// Setup gain grid and functions
	const sndGrid *grid = &grid_0_0_0;
	void (*set)(void) = setNothing;
	switch (_aproc) {
	case AUDIOPROC_TDA7439:
		grid = &grid_0_30_2;
		set = tda7439SetGain;
		break;
	case AUDIOPROC_TDA7313:
	case AUDIOPROC_PT2314:
		grid = &grid_0_11d25_3d75;
		set = tda731xSetGain;
		break;
	case AUDIOPROC_TDA7314:
	case AUDIOPROC_TDA7318:
		grid = &grid_0_18d75_6d25;
		set = tda731xSetGain;
		break;
	case AUDIOPROC_PT232X:
		grid = &grid_0_6_6;
		set = pt2323SetGain;
		break;
	default:
		break;
	}
	for (i = MODE_SND_GAIN0; i < MODE_SND_END; i++) {
		sndPar[i].grid = grid;
		sndPar[i].set = set;
	}

	/* Setup audio parameters grid and functions */
	switch (_aproc) {
	case AUDIOPROC_TDA7439:
		sndPar[MODE_SND_VOLUME].grid = &grid_n79_0_1;
		sndPar[MODE_SND_VOLUME].set = tda7439SetSpeakers;
		sndPar[MODE_SND_BASS].grid = &grid_n14_14_2;
		sndPar[MODE_SND_BASS].set = tda7439SetBass;
		sndPar[MODE_SND_MIDDLE].grid = &grid_n14_14_2;
		sndPar[MODE_SND_MIDDLE].set = tda7439SetMiddle;
		sndPar[MODE_SND_TREBLE].grid = &grid_n14_14_2;
		sndPar[MODE_SND_TREBLE].set = tda7439SetTreble;
		sndPar[MODE_SND_PREAMP].grid = &grid_n47_0_1;
		sndPar[MODE_SND_PREAMP].set = tda7439SetPreamp;
		sndPar[MODE_SND_BALANCE].grid = &grid_n15_15_1;
		sndPar[MODE_SND_BALANCE].set= tda7439SetSpeakers;
		break;
	case AUDIOPROC_TDA7312:
	case AUDIOPROC_TDA7313:
	case AUDIOPROC_TDA7314:
	case AUDIOPROC_TDA7315:
	case AUDIOPROC_TDA7318:
	case AUDIOPROC_PT2314:
		sndPar[MODE_SND_VOLUME].grid = &grid_n78d75_0_1d25;
		sndPar[MODE_SND_VOLUME].set = tda731xSetVolume;
		sndPar[MODE_SND_BASS].grid = &grid_n14_14_2;
		sndPar[MODE_SND_BASS].set = tda731xSetBass;
		sndPar[MODE_SND_TREBLE].grid = &grid_n14_14_2;
		sndPar[MODE_SND_TREBLE].set = tda731xSetTreble;
		sndPar[MODE_SND_BALANCE].grid = &grid_n18d75_18d75_1d25;
		sndPar[MODE_SND_BALANCE].set = tda731xSetSpeakers;
		switch (_aproc) {
		case AUDIOPROC_TDA7313:
		case AUDIOPROC_TDA7314:
		case AUDIOPROC_TDA7318:
			sndPar[MODE_SND_FRONTREAR].grid = &grid_n18d75_18d75_1d25;
			sndPar[MODE_SND_TREBLE].set = tda731xSetTreble;
			break;
		default:
			break;
		}
		break;
	case AUDIOPROC_TDA7448:
		sndPar[MODE_SND_VOLUME].grid = &grid_n79_0_1;
		sndPar[MODE_SND_VOLUME].set = tda7448SetSpeakers;
		sndPar[MODE_SND_FRONTREAR].grid = &grid_n7_7_1;
		sndPar[MODE_SND_FRONTREAR].set = tda7448SetSpeakers;
		sndPar[MODE_SND_BALANCE].grid = &grid_n7_7_1;
		sndPar[MODE_SND_BALANCE].set= tda7448SetSpeakers;
		sndPar[MODE_SND_CENTER].grid = &grid_n15_0_1;
		sndPar[MODE_SND_CENTER].set = tda7448SetSpeakers;
		sndPar[MODE_SND_SUBWOOFER].grid = &grid_n15_0_1;
		sndPar[MODE_SND_SUBWOOFER].set = tda7448SetSpeakers;
		break;
	case AUDIOPROC_PT232X:
		sndPar[MODE_SND_VOLUME].grid = &grid_n79_0_1;
		sndPar[MODE_SND_VOLUME].set = pt2322SetVolume;
		sndPar[MODE_SND_BASS].grid = &grid_n14_14_2;
		sndPar[MODE_SND_BASS].set = pt2322SetBass;
		sndPar[MODE_SND_MIDDLE].grid = &grid_n14_14_2;
		sndPar[MODE_SND_MIDDLE].set = pt2322SetMiddle;
		sndPar[MODE_SND_TREBLE].grid = &grid_n14_14_2;
		sndPar[MODE_SND_TREBLE].set = pt2322SetTreble;
		sndPar[MODE_SND_FRONTREAR].grid = &grid_n7_7_1;
		sndPar[MODE_SND_FRONTREAR].set = pt2322SetSpeakers;
		sndPar[MODE_SND_BALANCE].grid = &grid_n7_7_1;
		sndPar[MODE_SND_BALANCE].set = pt2322SetSpeakers;
		sndPar[MODE_SND_CENTER].grid = &grid_n15_0_1;
		sndPar[MODE_SND_CENTER].set = pt2322SetSpeakers;
		sndPar[MODE_SND_SUBWOOFER].grid = &grid_n15_0_1;
		sndPar[MODE_SND_SUBWOOFER].set = pt2322SetSpeakers;
		break;
	case AUDIOPROC_TEA6330:
		sndPar[MODE_SND_VOLUME].grid = &grid_n66_20_2;
		sndPar[MODE_SND_VOLUME].set = tea6330SetVolume;
		sndPar[MODE_SND_BASS].grid = &grid_n12_15_3;
		sndPar[MODE_SND_BASS].set = tea6330SetBass;
		sndPar[MODE_SND_TREBLE].grid = &grid_n12_12_3;
		sndPar[MODE_SND_TREBLE].set = tea6330SetTreble;
		sndPar[MODE_SND_FRONTREAR].grid = &grid_n14_14_2;
		sndPar[MODE_SND_FRONTREAR].set = tea6330SetFrontRear;
		sndPar[MODE_SND_BALANCE].grid = &grid_n14_14_2;
		sndPar[MODE_SND_BALANCE].set = tea6330SetVolume;
		break;
#ifdef EXTFUNC
	case AUDIOPROC_PGA2310:
		sndPar[MODE_SND_VOLUME].grid = &grid_n96_31_1;
		sndPar[MODE_SND_VOLUME].set = pga2310SetSpeakers;
		sndPar[MODE_SND_BALANCE].grid = &grid_n15_15_1;
		sndPar[MODE_SND_BALANCE].set = pga2310SetSpeakers;
		break;
#endif
	default:
		break;
	}

	return;
}

sndParam *sndParAddr(uint8_t index)
{
	return &sndPar[index];
}

uint8_t sndInputCnt(void)
{
	return _inCnt;
}

void sndSetInput(uint8_t input)
{
	if (input >= _inCnt)
		input = 0;
	_input = input;

	switch (_aproc) {
	case AUDIOPROC_TDA7439:
		tda7439SetInput(_input);
		break;
	case AUDIOPROC_TDA7312:
	case AUDIOPROC_TDA7313:
	case AUDIOPROC_TDA7314:
	case AUDIOPROC_TDA7315:
	case AUDIOPROC_TDA7318:
	case AUDIOPROC_PT2314:
		tda731xSetInput(_input);
		break;
	case AUDIOPROC_PT232X:
		pt2323SetInput(_input);
		break;
	default:
		break;
	}

	return;
}

uint8_t sndGetInput(void)
{
	return _input;
}


void sndSetMute(uint8_t value)
{
	_mute = value;

	if (_mute)
		PORT(STMU_MUTE) &= ~STMU_MUTE_LINE;
	else
		PORT(STMU_MUTE) |= STMU_MUTE_LINE;

	switch (_aproc) {
	case AUDIOPROC_TDA7439:
		tda7439SetMute(_mute);
		break;
	case AUDIOPROC_TDA7312:
	case AUDIOPROC_TDA7313:
	case AUDIOPROC_TDA7314:
	case AUDIOPROC_TDA7315:
	case AUDIOPROC_TDA7318:
	case AUDIOPROC_PT2314:
		tda731xSetMute(_mute);
		break;
	case AUDIOPROC_TDA7448:
		tda7448SetMute(_mute);
		break;
	case AUDIOPROC_PT232X:
		pt232xSetMute(_mute);
		break;
	case AUDIOPROC_TEA6330:
		tea6330SetMute(_mute);
		break;
#ifdef EXTFUNC
	case AUDIOPROC_PGA2310:
		pga2310SetMute(_mute);
		break;
#endif
	default:
		break;
	}

	return;
}

uint8_t sndGetMute(void)
{
	return _mute;
}

void sndSetLoudness(uint8_t value)
{
	_loudness = value;

	if (_aproc == AUDIOPROC_TDA7313 || _aproc == AUDIOPROC_TDA7314 ||
			_aproc == AUDIOPROC_TDA7315 || _aproc == AUDIOPROC_PT2314)
		tda731xSetLoudness(_loudness);

	return;
}

uint8_t sndGetLoudness(void)
{
	return _loudness;
}

void sndSetSurround(uint8_t value)
{
	_surround = value;

	if (_aproc == AUDIOPROC_PT232X)
		pt2323SetSurround(_surround);

	return;
}

uint8_t sndGetSurround(void)
{
	return _surround;
}

void sndSetEffect3d(uint8_t value)
{
	_effect3d = value;

	if (_aproc == AUDIOPROC_PT232X)
		pt2322SetEffect3d(_effect3d);

	return;
}

uint8_t sndGetEffect3d(void)
{
	return _effect3d;
}

void sndSetToneDefeat(uint8_t value)
{
	_toneDefeat = value;

	if (_aproc == AUDIOPROC_PT232X)
		pt2322SetToneDefeat(_toneDefeat);

	return;
}

uint8_t sndGetToneDefeat(void)
{
	return _toneDefeat;
}

void sndNextParam(uint8_t *mode)
{
	do {					/* Skip unused params (with step = 0) */
		(*mode)++;
		if (*mode >= MODE_SND_GAIN0)
			*mode = MODE_SND_VOLUME;
	} while((pgm_read_byte(&sndPar[*mode].grid->step) == 0) &&
			(*mode < MODE_SND_GAIN0) && (*mode != MODE_SND_VOLUME));

	return;
}

void sndChangeParam(uint8_t mode, int8_t diff)
{
	sndParam *param = sndParAddr(mode);
	param->value += diff;
	if (param->value > (int8_t)pgm_read_byte(&param->grid->max))
		param->value = (int8_t)pgm_read_byte(&param->grid->max);
	if (param->value < (int8_t)pgm_read_byte(&param->grid->min))
		param->value = (int8_t)pgm_read_byte(&param->grid->min);
	param->set();

	return;
}

void sndPowerOn(void)
{
	int8_t i;

	if (_aproc == AUDIOPROC_PT232X)
		pt232xReset();

	sndSetMute(1);
	sndSetInput(_input);

	sndSetLoudness(_loudness);
	sndSetSurround(_surround);
	sndSetEffect3d(_effect3d);
	sndSetToneDefeat(_toneDefeat);

	for (i = MODE_SND_GAIN0 - 1; i >= MODE_SND_VOLUME; i--)
		sndPar[i].set();

	sndSetMute(0);

	return;
}

void sndPowerOff(void)
{
	uint8_t i;

	for (i = 0; i < MODE_SND_END; i++)
		eeprom_update_byte((uint8_t*)EEPROM_VOLUME + i, sndPar[i].value);

	eeprom_update_byte((uint8_t*)EEPROM_LOUDNESS, _loudness);
	eeprom_update_byte((uint8_t*)EEPROM_SURROUND, _surround);
	eeprom_update_byte((uint8_t*)EEPROM_EFFECT3D, _effect3d);
	eeprom_update_byte((uint8_t*)EEPROM_TONE_DEFEAT, _toneDefeat);
	eeprom_update_byte((uint8_t*)EEPROM_INPUT, _input);

	return;
}
