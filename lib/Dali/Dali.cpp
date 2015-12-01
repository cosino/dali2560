/*
 * Copyright (C) 2015 Luca Zulberti <luca.zulberti@cosino.io>
 * Copyright (C) 2015 HCE Engineering <info@hce-engineering.com>
 *
 * This code is derived from pq_Dali.cpp by http://blog.perquin.com.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Dali.h"

/*
 * Define
 */
 
#define DALI_BUS_LOW() digitalWrite(this->tx_pin,LOW); this->tx_bus_low=1
#define DALI_BUS_HIGH() digitalWrite(this->tx_pin,HIGH); this->tx_bus_low=0
#define DALI_IS_BUS_LOW() (digitalRead(this->rx_pin)==LOW)
#define DALI_BAUD 1200
#define DALI_TE ((1000000+(DALI_BAUD))/(2*(DALI_BAUD)))	/* 417us */
#define DALI_TE_MIN (80*DALI_TE)/100
#define DALI_TE_MAX (120*DALI_TE)/100
#define DALI_IS_TE(x) ((DALI_TE_MIN)<=(x) && (x)<=(DALI_TE_MAX))
#define DALI_IS_2TE(x) ((2*(DALI_TE_MIN))<=(x) && (x)<=(2*(DALI_TE_MAX)))

/*
 * ISR TX
 */
 
static Dali *IsrTimerHooks[DALI_HOOK_COUNT + 1];

ISR(TIMER1_COMPA_vect)
{
	for (uint8_t i = 0; i < DALI_HOOK_COUNT; i++) {
		if (IsrTimerHooks[i] == NULL) {
			return;
		}
		IsrTimerHooks[i]->ISR_timer();
	}
}

void Dali::ISR_timer()
{
	if (this->bus_idle_te_cnt < 0xff)
		this->bus_idle_te_cnt++;

	/* Send sequence: start bit, message, 2 stop bits */
	switch (this->tx_state) {
	case IDLE:
		break;
	case START:
		/* Waiting 22 * te */
		if (this->bus_idle_te_cnt >= 22) {
			DALI_BUS_LOW();
			this->tx_state = START_X;
		}
		break;
	case START_X:
		DALI_BUS_HIGH();
		this->tx_pos = 0;
		this->tx_state = BIT;
		break;
	case BIT:
		if (this->
		    tx_msg[this->tx_pos >> 3] & 1 << (7 -
						      (this->tx_pos & 0x7))) {
			DALI_BUS_LOW();
		} else {
			DALI_BUS_HIGH();
		}
		this->tx_state = BIT_X;
		break;
	case BIT_X:
		if (this->
		    tx_msg[this->tx_pos >> 3] & 1 << (7 -
						      (this->tx_pos & 0x7))) {
			DALI_BUS_HIGH();
		} else {
			DALI_BUS_LOW();
		}
		this->tx_state = BIT_X;
		this->tx_pos++;
		if (this->tx_pos < this->tx_len) {
			this->tx_state = BIT;
		} else {
			this->tx_state = STOP1;
		}
		break;
	case STOP1:
		DALI_BUS_HIGH();
		this->tx_state = STOP1_X;
		break;
	case STOP1_X:
		this->tx_state = STOP2;
		break;
	case STOP2:
		this->tx_state = STOP2_X;
		break;
	case STOP2_X:
		this->tx_state = STOP3;
		break;
	case STOP3:
		this->bus_idle_te_cnt = 0;
		this->tx_state = IDLE;
		this->rx_state = RX_IDLE;
		break;
	}

	/* Analyzing receiving stop bits (4 times logical '1') */
	if (this->rx_state == RX_BIT && this->bus_idle_te_cnt > 4) {
		this->rx_state = RX_IDLE;
		/* 2 stop bits received, rx message to the user */
		uint8_t bitlen = (this->rx_len + 1) >> 1;
		if ((bitlen & 0x7) == 0) {
			uint8_t len = bitlen >> 3;
			this->rx_int_rq = 0;
			if (this->EventHandlerReceivedData != NULL)
				this->EventHandlerReceivedData(this,
							       (uint8_t *)
							       this->rx_msg,
							       len);
		}
	}
}

/*
 * ISR RX
 */

static Dali *IsrPCINT0Hook;
static Dali *IsrPCINT1Hook;
static Dali *IsrPCINT2Hook;

ISR(PCINT0_vect)
{
	if (IsrPCINT0Hook != NULL)
		IsrPCINT0Hook->ISR_pinchange();
}

ISR(PCINT1_vect)
{
	if (IsrPCINT1Hook != NULL)
		IsrPCINT1Hook->ISR_pinchange();
}

ISR(PCINT2_vect)
{
	if (IsrPCINT2Hook != NULL)
		IsrPCINT2Hook->ISR_pinchange();
}

void Dali::ISR_pinchange()
{
	uint32_t ts = micros();			/* Timestamp pinchange */
	this->bus_idle_te_cnt = 0;		/* Reset IDLE counter */
	uint8_t bus_low = DALI_IS_BUS_LOW();

	/* This instance is transmitting? */
	if (this->tx_state != IDLE) {
		/* Collision occurred */
		if (bus_low && !this->tx_bus_low) {
			this->tx_state = IDLE;	/* Stop transmission */
			this->tx_collision = 1;	/* Check collision */
		}
		return;
	}
	/* Logical bus level unchanged */
	if (bus_low == this->rx_last_bus_low)
		return;

	/* Logical bus level changed -> store values for analyze the content */
	uint32_t dt = ts - this->rx_last_change_ts;	/* Period of transmission protocol */
	this->rx_last_change_ts = ts;			/* Store last Timestamp */
	this->rx_last_bus_low = bus_low;		/* Store new logical bus level */

	switch (this->rx_state) {
	case RX_IDLE:
		if (bus_low) {
			this->rx_state = RX_START;
		}
		break;
	case RX_START:
		if (bus_low || !DALI_IS_TE(dt)) {
			this->rx_state = RX_IDLE;
		} else {
			this->rx_len = -1;
			for (uint8_t i = 0; i < 7; i++)
				this->rx_msg[0] = 0;
			this->rx_state = RX_BIT;
		}
		break;
	case RX_BIT:
		if (DALI_IS_TE(dt)) {
			/* Add half-bit */
			this->push_halfbit(bus_low);
		} else if (DALI_IS_2TE(dt)) {
			/* Add 2 half-bit */
			this->push_halfbit(bus_low);
			this->push_halfbit(bus_low);
		} else {
			/* some error... */
			this->rx_state = RX_IDLE;
			/* TODO rx error handler */
			return;
		}
		break;
	}
}

void Dali::push_halfbit(uint8_t bit)
{
	bit = (~bit) & 1;
	if ((this->rx_len & 1) == 0) {
		uint8_t i = this->rx_len >> 4;
		if (i < 3) {
			this->rx_msg[i] = (this->rx_msg[i] << 1) | bit;
		}
	}
	this->rx_len++;
}

/*
 * Dali Class
 */

void Dali::begin(uint8_t tx_pin, uint8_t rx_pin)
{
	this->tx_pin = tx_pin;
	this->rx_pin = rx_pin;
	this->tx_state = IDLE;
	this->rx_state = RX_IDLE;

	if (!Serial)
		Serial.begin(115200);

	/* setup tx */
	if (this->tx_pin >= 0) {
		/* setup tx pin */
		pinMode(this->tx_pin, OUTPUT);
		DALI_BUS_HIGH();

		/* setup tx timer interrupt */
		TCCR1A = 0;
		TCCR1B = 0;
		TCNT1 = 0;

		/* compare match register 16MHz/256/2Hz */
		OCR1A = (F_CPU + DALI_BAUD) / (2 * DALI_BAUD);
		TCCR1B |= (1 << WGM12);			/* CTC mode */
		TCCR1B |= (1 << CS10);			/* 1:1 prescaler */
		TIMSK1 |= (1 << OCIE1A);		/* Interrupt enabled */

		/* setup timer interrupt hooks */
		for (uint8_t i = 0; i < DALI_HOOK_COUNT; i++) {
			if (IsrTimerHooks[i] == NULL) {
				IsrTimerHooks[i] = this;
				break;
			}
		}
	}
	/* setup rx */
	if (this->rx_pin >= 0) {
		/* setup rx pin */
		pinMode(this->rx_pin, INPUT);

		/* setup rx pinchange interrupt */
		/* 10-13  PCINT0_vect PCINT4-7 */
		/* 14-15  PCINT1_vect PCINT9-10 */
		/* A8-A15 PCINT2_vect PCINT16-23 */
		if (this->rx_pin <= 13 && this->rx_pin >= 10) {
			PCICR |= (1 << PCIE0);
			PCMSK0 |= (1 << (this->rx_pin - 6));
			/* setup pinchange interrupt hook */
			IsrPCINT0Hook = this;
		} else if (this->rx_pin == 14 && this->rx_pin == 15) {
			PCICR |= (1 << PCIE1);
			PCMSK1 |= (1 << (this->rx_pin - 13));
			/* setup pinchange interrupt hook */
			IsrPCINT1Hook = this;
		} else if (this->rx_pin <= A15 && this->rx_pin >= A8) {
			PCICR |= (1 << PCIE2);
			PCMSK2 |= (1 << (this->rx_pin - 62));
			/* setup pinchange interrupt hook */
			IsrPCINT2Hook = this;
		}
	}

	uint8_t i;
	for (i = 0; i < 2; i++) {
		if (Master[i] == NULL) {
			Master[i] = this;
			break;
		}
	}
	bytes_rx = 0;
}

uint8_t Dali::send(uint8_t * tx_msg, uint8_t tx_len_bytes)
{
	if (tx_len_bytes > 3)
		return -1;
	if (this->tx_state != IDLE)
		return -1;
	for (uint8_t i = 0; i < tx_len_bytes; i++)
		this->tx_msg[i] = tx_msg[i];
	this->tx_len = tx_len_bytes << 3;
	this->tx_collision = 0;
	this->tx_state = START;
	return 1;
}

uint8_t Dali::sendwait(uint8_t * tx_msg, uint8_t tx_len_bytes,
		       uint32_t timeout_ms)
{
	if (tx_len_bytes > 3)
		return -1;
	uint32_t ts = millis();
	/* wait for idle */
	while (this->tx_state != IDLE)
		if (millis() - ts > timeout_ms)
			return -1;
	/* start transmit */
	if (this->send(tx_msg, tx_len_bytes) < 0)
		return -1;
	this->rx_int_rq = 1;
	/* waiting for complete transmission */
	while (this->tx_state != IDLE)
		if (millis() - ts > timeout_ms)	/* timeout? */
			return -1;
	while (this->rx_int_rq)
		if (millis() - ts > timeout_ms) {	/* timeout? */
			this->rx_msg[0] = 0x00;
			return -1;
		}
	return 1;
}

uint8_t Dali::sendwait_int(uint16_t tx_msg, uint32_t timeout_ms)
{
	uint8_t m[3];
	m[0] = tx_msg >> 8;
	m[1] = tx_msg & 0xff;
	return sendwait(m, 2, timeout_ms);
}

uint8_t Dali::sendwait_byte(uint8_t tx_msg, uint32_t timeout_ms)
{
	uint8_t m[3];
	m[0] = tx_msg;
	return sendwait(m, 1, timeout_ms);
}
