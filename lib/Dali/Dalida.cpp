/*
 * Copyright (C) 2015 Luca Zulberti <luca.zulberti@cosino.io>
 * Copyright (C) 2015 HCE Engineering <info@hce-engineering.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "Dali.h"
#include <avr/eeprom.h>

void Dali_rx(Dali * d, uint8_t * data, uint8_t len);
void serialDali_rx(uint8_t errn, uint8_t * data, uint8_t n);
uint8_t exeCmd(char *msg);
uint8_t devCmd(char *msg);
uint8_t grpCmd(char *msg);
uint8_t busCmd(char *msg);
uint8_t rmpCmd(char *msg);
uint8_t action(char *msg);
void storeSlaves(Dali * dali, uint8_t * slaves);
void retrieveSlaves(Dali * dali, uint8_t * slaves);
int checkSlave(Dali * dali, uint8_t dev);
int infoDev(Dali * dali, uint8_t dev_type, uint8_t dev, uint8_t * info_buf);

Dali *Master[2];
uint8_t bytes_rx;
char msg[10];
uint8_t rx_buf[100];	/* First byte used for lenght */
uint8_t rx_code;
enum rx_codes {
	STRING_CODING,
	BIN_CODING
};

void serialDali(void)
{
	uint8_t errn;

	if (Serial.available()) {
		msg[bytes_rx] = (char)Serial.read();
		Serial.write(msg[bytes_rx]);
		if (msg[bytes_rx] == '\n') {
			if (msg[bytes_rx - 1] == '\r') /* Adjustment protocol */
				msg[bytes_rx - 1] = '\n';
			bytes_rx = 0;
			errn = exeCmd(msg);
			if (errn == 0)
				serialDali_rx(0, rx_buf + 1, rx_buf[0]);
			else if (errn != 0xFF)
				/* 
				 * Is not an error
				 * Remap finished do not send anything
				 */
				serialDali_rx(errn, NULL, 0);
		} else if (bytes_rx == 9)
			bytes_rx = 0;
		else
			bytes_rx++;
	}
}

void serialDali_rx(uint8_t errn, uint8_t * data, uint8_t n)
{
	uint8_t buf[100];
	uint8_t n_char = 0;

	if (errn == 0) {
		buf[0] = 'O';
		if (rx_code == STRING_CODING) {
			for (int a = 0; a < n; a++)
				n_char +=
				    sprintf((char *)buf + 1 + n_char, "%d",
					    (char)(data[a]));
		} else if (rx_code == BIN_CODING) {
			for (int a = 0; a < n; a++)
				buf[1 + a] = data[a];
			n_char = n;
		}
		buf[1 + n_char] = '\n';
		buf[2 + n_char] = '\r';
		Serial.write(buf, n_char + 3);
	} else {
		switch (errn) {
		case 0x01:
			Serial.println("E01");
			break;
		case 0x02:
			Serial.println("E02");
			break;
		case 0x03:
			Serial.println("E03");
			break;
		case 0x20:
			Serial.println("E20");
			break;
		case 0x30:
			Serial.println("E30");
			break;
		case 0x90:
			Serial.println("E90");
			break;
		}
	}
}

void Dali_rx(Dali * d, uint8_t * data, uint8_t len)
{
	serialDali_rx(0, data, 1);
}

uint8_t exeCmd(char *msg)
{
	switch (msg[0]) {
	case 'd':
		return devCmd(msg);
		break;
	case 'g':
		return grpCmd(msg);
		break;
	case 'b':
		return busCmd(msg);
		break;
	case 'R':
		return rmpCmd(msg);
		break;
	}
	return 0x01;	/* Syntax error */
}

uint8_t devCmd(char *msg)
{
	uint8_t bus, dev, arc;
	char str[] = "00";

	if (msg[2] == '0')
		bus = 0;
	else if (msg[2] == '1')
		bus = 1;
	else
		return 0x20;
	if ((Master[bus]->dali_status & 0x01) == 1)
		return 0x03;

	str[0] = msg[3];
	str[1] = msg[4];
	dev = (uint8_t) strtol(str, NULL, 16);

	if (checkSlave(Master[bus], dev) < 0)
		return 0x30;

	switch (msg[1]) {
	case '1':
		Master[bus]->sendCommand(5, SINGLE, dev);	/* ON */
		rx_buf[0] = 0;
		return 0x00;
	case '0':
		Master[bus]->sendCommand(0, SINGLE, dev);	/* OFF */
		rx_buf[0] = 0;
		return 0x00;
	case 'a':
		arc = (uint8_t) strtol(msg + 5, NULL, 16);
		Master[bus]->sendDirect(arc, SINGLE, dev);	/* Arc level */
		rx_buf[0] = 0;
		return 0x00;
	case 'i':
		int ret, dev_type;
		Master[bus]->sendCommand(153, SINGLE, dev);	/* Get DEV type */
		dev_type = *(Master[bus]->getReply());
		rx_buf[0] = infoDev(Master[bus], dev_type, dev, rx_buf + 1);
		rx_code = BIN_CODING;
		return 0x00;
	case 'c':
		/* Configure device - to implement */
		return 0x02;
	}

	return 0x01;
}

uint8_t grpCmd(char *msg)
{
	uint8_t bus, grp, arc;
	char str[] = "0";

	if (msg[2] == '0')
		bus = 0;
	else if (msg[2] == '1')
		bus = 1;
	else
		return 0x20;
	if ((Master[bus]->dali_status & 0x01) == 1)
		return 0x03;

	str[0] = msg[3];
	grp = (uint8_t) strtol(str, NULL, 16);

	switch (msg[1]) {
	case '1':
		Master[bus]->sendCommand(5, GROUP, grp);	/* ON */
		rx_buf[0] = 0;
		return 0x00;
	case '0':
		Master[bus]->sendCommand(0, GROUP, grp);	/* OFF */
		rx_buf[0] = 0;
		return 0x00;
	case 'a':
		arc = (uint8_t) strtol(msg + 4, NULL, 16);
		Master[bus]->sendDirect(arc, GROUP, grp);	/* Arc level */
		rx_buf[0] = 0;
		return 0x00;
	}

	return 0x01;
}

uint8_t busCmd(char *msg)
{
	uint8_t bus, grp, arc;
	char str[] = "00";

	if (msg[2] == '0')
		bus = 0;
	else if (msg[2] == '1')
		bus = 1;
	else
		return 0x20;
	if ((Master[bus]->dali_status & 0x01) == 1)
		return 0x03;

	switch (msg[1]) {
	case '1':
		Master[bus]->sendCommand(5, BROADCAST, grp);	/* ON */
		rx_buf[0] = 0;
		return 0x00;
	case '0':
		Master[bus]->sendCommand(0, BROADCAST, grp);	/* OFF */
		rx_buf[0] = 0;
		return 0x00;
	case 'a':
		arc = (uint8_t) strtol(msg + 3, NULL, 16);
		Master[bus]->sendDirect(arc, BROADCAST, grp);	/* Arc level */
		rx_buf[0] = 0;
		return 0x00;
	case 'd':
		int i, j;
		uint8_t _slaves[8];
		retrieveSlaves(Master[bus], _slaves);		/* Detect */
		rx_buf[0] = 64;
		for (i = 0; i < 8; i++)
			for (j = 0; j < 8; j++)
				rx_buf[1 + i * 8 + j] =
				    ((_slaves[i] & (1 << j)) ? 1 : 0);
		rx_code = STRING_CODING;
		return 0x00;
	}

	return 0x01;
}

uint8_t rmpCmd(char *msg)
{
	switch (msg[1]) {
	case '\n':	/* Remap All */
		if ((Master[0]->dali_status & 0x01) == 1
		    || (Master[1]->dali_status & 0x01) == 1)
			return 0x03;
		serialDali_rx(0, NULL, 0);
		dev_found = 0;
		for (int i = 0; i < 2; i++)
			if (Master[i] != NULL)
				Master[i]->remap(ALL);
		return 0xFF;	/* Remap finished. Do not send anything. */

	case 'u':	/* Remap unknown Dev */
		if ((Master[0]->dali_status & 0x01) == 1
		    || (Master[1]->dali_status & 0x01) == 1)
			return 0x03;
		serialDali_rx(0, NULL, 0);
		dev_found = 0;
		for (int i = 0; i < 2; i++)
			if (Master[i] != NULL)
				Master[i]->remap(MISS_SHORT);
		return 0xFF;	/* Remap finished. Do not send anything. */

	case 'f':	/* Remap finished? */
		if ((Master[0]->dali_status & 0x01) == 1
		    || (Master[1]->dali_status & 0x01) == 1)
			rx_buf[1] = (dev_found * 100) / 64;
		else
			rx_buf[1] = 100;
		rx_buf[0] = 1;
		rx_code = STRING_CODING;
		return 0x00;

	case 'A':	/* Remap Abort */
		for (int i = 0; i < 2; i++)
			if (Master[i] != NULL)
				Master[i]->abort_remap();
		rx_buf[0] = 0;
		return 0x00;
	}

	return 0x01;
}

void storeSlaves(Dali * dali, uint8_t * slaves)
{
	int base_addr;

	if (dali == Master[0])
		base_addr = 0x0000;
	else if (dali == Master[1])
		base_addr = 0x0100;
	else
		return;

	for (int i = 0; i < 8; i++)
		eeprom_write_byte((unsigned char *)(base_addr + i), slaves[i]);
}

void retrieveSlaves(Dali * dali, uint8_t * slaves)
{
	int base_addr;

	if (dali == Master[0])
		base_addr = 0x0000;
	else if (dali == Master[1])
		base_addr = 0x0100;
	else
		return;

	for (int i = 0; i < 8; i++) {
		slaves[i] = eeprom_read_byte((unsigned char *)(base_addr + i));
		dali->slaves[i] = slaves[i];
	}
}

int checkSlave(Dali * dali, uint8_t dev)
{
	uint8_t i, j, slaves[8];

	i = dev / 8;
	j = dev % 8;
	retrieveSlaves(dali, slaves);

	if ((slaves[i] & (1 << j)) == 1)
		return 0;
	else
		return -1;
}

int infoDev(Dali * dali, uint8_t dev_type, uint8_t dev, uint8_t * info_buf)
{
	switch (dev_type) {
	case LED_MODULE:
		info_buf[0] = dev_type;
		dali->sendCommand(162, SINGLE, dev);	/* Query MIN LEVEL */
		info_buf[1] = *(dali->getReply());
		dali->sendCommand(161, SINGLE, dev);	/* Query MAX LEVEL */
		info_buf[2] = *(dali->getReply());
		dali->sendCommand(160, SINGLE, dev);	/* Query actual LEVEL */
		info_buf[3] = *(dali->getReply());
		return 4;
	default:
		info_buf[0] = dev_type;
		return 1;
	}
}
