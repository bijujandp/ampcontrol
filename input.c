#include <avr/io.h>
#include <avr/interrupt.h>

#include "input.h"

volatile uint8_t cmdBuf = CMD_NOCMD;	/* Command buffer, cleared when read */
volatile uint8_t encCnt = 0;			/* Counter for encoder */
volatile uint8_t rc5Buf = CMD_NOCMD;	/* Buffer for last command from RC */
volatile uint16_t rc5Timer = 0;

volatile uint16_t rc5Cmd; /**/
const uint8_t trans[4] = {0x01, 0x91, 0x9b, 0xfb};
uint8_t rc5Cnt;

volatile uint16_t displayTime;

rc5State state = STATE_BEGIN;

void setDisplayTime(uint16_t value)
{
	displayTime = value;
}

uint16_t getDisplayTime(void)
{
	return displayTime;
}

void rc5Reset()
{
	rc5Cnt = 14;
	rc5Cmd = 0;
	state = STATE_BEGIN;
}

void rc5Init()
{
	MCUCR |= (1<<ISC10);	/* Set INT1 to trigger on any edge */
	BTN_DDR &= ~(1<<PD3);	/* Set PD3 (INT1) to input */
	TCCR1A = 0;				/* Reset Timer1 counter */
	TCCR1B = (1<<CS11);		/* Set Timer1 prescaler to 8 (2MHz) */
	GICR |= (1<<INT1);		/* Enable INT1 interrupt */

	rc5Reset();
}

ISR(INT1_vect)
{
	uint16_t delay = TCNT1;

	static uint8_t togBitNow = 0;
	static uint8_t togBitPrev = 0;

	/* event: 0 / 2 - short space/pulse, 4 / 6 - long space/pulse */
	uint8_t event = (BTN_PIN & RC5_DATA) ? 2 : 0;

	if (delay > RC5_LONG_MIN && delay < RC5_LONG_MAX)
		event += 4;
	else if (delay < RC5_SHORT_MIN || delay > RC5_SHORT_MAX)
		rc5Reset();

	if (state == STATE_BEGIN) {
		rc5Cnt--;
		rc5Cmd |= (1 << rc5Cnt);
		state = STATE_MID1;
		TCNT1 = 0;
		return;
	}

	rc5State newstate = (trans[state] >> event) & 0x03;

	if (newstate == state || state > STATE_START0) {
		rc5Reset();
		return;
	}

	state = newstate;

	if (state == STATE_MID0)
		rc5Cnt--;
	else if (state == STATE_MID1) {
		rc5Cnt--;
		rc5Cmd |= 1 << rc5Cnt;
	}

	if (rc5Cnt == 0 && (state == STATE_START1 || state == STATE_MID0)) {
		/* If RC5 address is correct, send command to IR RC buffer */
		if ((rc5Cmd & RC5_ADDR_MASK) == RC5_ADDR) {
			if (rc5Cmd & RC5_TOGB_MASK)
				togBitNow = 1;
			else
				togBitNow = 0;
			rc5Cmd &= RC5_COMM_MASK;
			if ((togBitNow != togBitPrev) ||
			    ((rc5Timer > 200) &
			     (rc5Cmd == CMD_VOL_UP || rc5Cmd == CMD_VOL_DOWN)) ||
			    (rc5Timer > 800)) {
				rc5Buf = rc5Cmd;
				rc5Timer = 0;
			}
			togBitPrev = togBitNow;
		}
		rc5Reset();
	}

	TCNT1 = 0;
}

void btnInit(void)
{
	/* Setup buttons and encoders as inputs with pull-up resistors */
	BTN_DDR &= ~(BTN_MASK);
	BTN_PORT |= BTN_MASK;

	ENC_DDR &= ~(ENC_AB);
	ENC_PORT |= ENC_AB;

	TCCR2 = 0b101;			/* Set timer prescaller to 128 (125 kHz) */
	OCR2 = 125;				/* 12500/125 => 1000 polls/sec */
	TCCR2 |= (1<<WGM21);	/* Reset counter on match */
	TCNT2 = 0;				/* Reset timer value */
	TIMSK |= (1<<OCIE2);	/* Enable timer compare match interrupt */
}

ISR (TIMER2_COMP_vect) {
	static uint8_t encPrev = ENC_0;		/* Previous encoder state */
	static int16_t btnCnt = 0;

	static uint8_t btnPrev = 0;

	uint8_t btnPin = ~BTN_PIN;
	uint8_t encNow = btnPin & (ENC_A | ENC_B);
	uint8_t btnNow = btnPin & BTN_MASK;


	if (rc5Buf != CMD_NOCMD) {
		cmdBuf = rc5Buf;
		encCnt++;
		rc5Buf = CMD_NOCMD;
	}
	if (rc5Timer < 1000)
		rc5Timer++;

	/* If encoder event has happened, send it to command buffer */
	switch (encNow) {
	case ENC_AB:
		if (encPrev == ENC_B) {
			cmdBuf = CMD_VOL_UP;
			encCnt++;
		}
		if (encPrev == ENC_A) {
			cmdBuf = CMD_VOL_DOWN;
			encCnt++;
		}
		break;
/*	case ENC_A:
		if (encPrev == ENC_AB) {
			cmdBuf = CMD_VOL_UP;
			encCnt++;
		}
		if (encPrev == ENC_0) {
			cmdBuf = CMD_VOL_DOWN;
			encCnt++;
		}
		break;
	case ENC_B:
		if (encPrev == ENC_0) {
			cmdBuf = CMD_VOL_UP;
			encCnt++;
		}
		if (encPrev == ENC_AB) {
			cmdBuf = CMD_VOL_DOWN;
			encCnt++;
		}
		break;
	case ENC_0:
		if (encPrev == ENC_A) {
			cmdBuf = CMD_VOL_UP;
			encCnt++;
		}
		if (encPrev == ENC_B) {
			cmdBuf = CMD_VOL_DOWN;
			encCnt++;
		}
		break;
*/	default:
		break;
	}
	encPrev = encNow;	/* Save current encoder state */

	if (btnNow) {
		if (btnNow == btnPrev)
			btnCnt++;
		else
			btnCnt = 0;
		btnPrev = btnNow;
	} else {
		if (btnCnt > LONG_PRESS) {
			// Place "long" command to buffer
			switch (btnPrev) {
			case BTN_MENU:
				cmdBuf = CMD_MENU;
				break;
			case BTN_STDBY:
				cmdBuf = CMD_STBY;
				break;
			case BTN_MUTE:
				cmdBuf = CMD_DESCR;
				break;
			case BTN_TIME:
				cmdBuf = CMD_STORE;
				break;
			case BTN_INPUT:
				cmdBuf = CMD_SEARCH;
				break;
			default:
				break;
			}
		} else {
			if (btnCnt > SHORT_PRESS) {
				// Place "short" command to buffer
				switch (btnPrev) {
				case BTN_MENU:
					cmdBuf = CMD_MENU;
					break;
				case BTN_STDBY:
					cmdBuf = CMD_STBY;
					break;
				case BTN_MUTE:
					cmdBuf = CMD_MUTE;
					break;
				case BTN_TIME:
					cmdBuf = CMD_TIME;
					break;
				case BTN_INPUT:
					cmdBuf = CMD_SEARCH;
					break;
				default:
					break;
				}
			}
		}
		// Clear button counter
		btnCnt = 0;
	}

	if (displayTime)
		displayTime--;
	return;
};

uint8_t getCommand(void) /* Read command and clear command buffer */
{
	uint8_t ret = cmdBuf;
	cmdBuf = CMD_NOCMD;
	return ret;
}

uint8_t getCmdCount(void)
{
	uint8_t ret = encCnt;
	encCnt = 0;
	return ret;
}
