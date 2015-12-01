/*
 * Copyright (C) 2015 Luca Zulberti <luca.zulberti@cosino.io>
 * Copyright (C) 2015 HCE Engineering <info@hce-engineering.com>
 *
 * This code is derived from pq_Dali.h by http://blog.perquin.com.
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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "Arduino.h"

/*
 * Define
 */

#define DALI_HOOK_COUNT 2

/*
 * Enums
 */

enum addr_type {
	BROADCAST,
	GROUP,
	SINGLE
};
enum readdr_type {
	ALL = 0,
	MISS_SHORT = 255
};
enum dev_type {
	FLUO_LAMP,
	EMERGENCY_LIGHT,
	DISCHARGE_LAMP,
	LV_ALOGEN_LAMP,
	INCANDESCENT_LAMP,
	CONV_DIG_DC,
	LED_MODULE,
	SWITCH,
	COLOUR_CTRL,
	SEQUENCER,
	OPTICAL_CTRL
};

/*
 * Class
 */

class Dali {
public:
	typedef void (*EventHandlerReceivedDataFuncPtr) (Dali * sender,
							 uint8_t * data,
							 uint8_t len);
	EventHandlerReceivedDataFuncPtr EventHandlerReceivedData;

	void begin(uint8_t tx_pin, uint8_t rx_pin);
	uint8_t send(uint8_t * tx_msg, uint8_t tx_len_bytes);
	uint8_t sendwait(uint8_t * tx_msg, uint8_t tx_len_bytes,
			 uint32_t timeout_ms = 500);
	uint8_t sendwait_int(uint16_t tx_msg, uint32_t timeout_ms = 500);
	uint8_t sendwait_byte(uint8_t tx_msg, uint32_t timeout_ms = 500);
	void ISR_timer();
	void ISR_pinchange();

	uint8_t sendDirect(uint8_t val, addr_type type_addr, uint8_t addr);
	uint8_t sendCommand(uint8_t val, addr_type type_addr, uint8_t addr);
	uint8_t sendExtCommand(uint16_t com, uint8_t val);
	uint8_t *getReply(void);

	void readStat(addr_type type_addr, uint8_t addr);

	/* DALIDA functions */
	void remap(readdr_type remap_type);
	void abort_remap(void);
	void list_dev(void);

	/* DALIDA variables */
	uint8_t dali_status;		/* b0 = remapping, b1 = need remap */
	uint8_t dali_cmd;		/* b0 = stop remap */
	uint8_t slaves[8];

private:
	 uint8_t bus_number;

	enum tx_stateEnum {
		IDLE,
		START, START_X,
		BIT,   BIT_X,
		STOP1, STOP1_X,
		STOP2, STOP2_X,
		STOP3
	};
	uint8_t tx_pin;
	uint8_t tx_msg[3];
	uint8_t tx_len;
	volatile uint8_t tx_pos;	/* Position of next bit to send */
	volatile tx_stateEnum tx_state;
	volatile uint8_t tx_bus_low;	/* Current logic state of the bus */
	volatile uint8_t tx_collision;

	enum rx_stateEnum {
		RX_IDLE,
		RX_START,
		RX_BIT
	};
	uint8_t rx_pin;
	volatile uint8_t rx_last_bus_low;    /* Last logic state of the bus */
	volatile uint32_t rx_last_change_ts; /* Last change timestamp */
	volatile rx_stateEnum rx_state;
	volatile uint8_t rx_msg[3];
	volatile int8_t rx_len;
	volatile uint8_t rx_last_halfbit;
	volatile uint8_t rx_int_rq;	     /* Call user interrupt routine? */
	volatile uint8_t bus_idle_te_cnt;    /* N of te(417 us) in IDLE state */

	void push_halfbit(uint8_t bit);

	/* DALIDA */
	uint8_t setDevAddresses(uint8_t start_addr, readdr_type all);
	uint32_t findDev(uint32_t base_addr, uint32_t delta_addr, uint8_t n);
	void setSearch(uint32_t addr);
};

/*
 * Extern 
 */

void serialDali(void);
extern Dali *Master[2];
extern uint8_t bytes_rx;
extern void storeSlaves(Dali * dali, uint8_t * slaves);
extern uint8_t dev_found;
