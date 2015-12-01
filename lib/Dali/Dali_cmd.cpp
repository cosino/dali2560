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

uint8_t dev_found;

void Dali::remap(readdr_type remap_type)
{
	uint8_t n_dev;
	this->dali_status |= 0x01;		/* Remapping */

	/* Set the Short Address to all devices from 0x00 */
	n_dev = this->setDevAddresses(0x00, remap_type);
	
	for (int i = 0; i < 8; i++)		/* Clean 'slaves' array */
		this->slaves[i] = 0;

	if (n_dev == 0xFF) {
		/* Need a remap operation */
		this->dali_status = (this->dali_status | 0x2) & 0xFE;
		this->dali_cmd &= 0xFE;
		dev_found = 0;
		return;
	}

	/* Prepare 'slaves' array for future detects */
	for (int i = 0; i < dev_found; i++)
		this->slaves[i / 8] |= 1 << (i % 8);
	storeSlaves(this, slaves);

	this->dali_status &= 0xFE;		/* Finish remapping */
}

void Dali::abort_remap(void)
{
	this->dali_cmd |= 0x01;
}

/* TODO */
void Dali::list_dev(void)
{
	/* 
	 * Send the slaves[] array with slaves info
	 * to the dalida with the standard protocol
	 */
}

uint8_t Dali::sendDirect(uint8_t val, addr_type type_addr, uint8_t addr)
{
	uint8_t cmd[2];

	if (type_addr == BROADCAST)
		cmd[0] = 0xFE;
	else if (type_addr == GROUP) {
		if (addr > 15)
			return -1;
		cmd[0] = 0x80 | (addr << 1);
	} else if (type_addr == SINGLE) {
		if (addr > 63)
			return -1;
		cmd[0] = (addr << 1);
	}
	cmd[1] = val;
	this->sendwait(cmd, 2, 100);
}

uint8_t Dali::sendCommand(uint8_t val, addr_type type_addr, uint8_t addr)
{
	uint8_t cmd[2];

	if (type_addr == BROADCAST)
		cmd[0] = 0xFF;
	else if (type_addr == GROUP) {
		if (addr > 15)
			return -1;
		cmd[0] = 0x01 | (addr << 1);
	} else if (type_addr == SINGLE) {
		if (addr > 63)
			return -1;
		cmd[0] = 0x01 | (addr << 1);
	}
	cmd[1] = val;
	if (val >= 224 && val <= 255) {
		sendExtCommand(272, 6);		/* En Dev Type Cmd Response */
	}
	this->sendwait(cmd, 2, 100);
	if (val >= 32 && val <= 128 || val >= 224 && val <= 255)
		this->sendwait(cmd, 2, 100);
}

uint8_t Dali::sendExtCommand(uint16_t com, uint8_t val)
{
	uint8_t cmd[2];
	uint16_t appo;
	if (com < 256 || com > 349)
		return -1;
	if (com > 255 && com < 272) {
		appo = ((com - 256) & 0x00FF) << 1;
		cmd[0] = appo;
		cmd[0] |= 0xA1;
	} else if (com > 271 && com < 276) {
		appo = ((com - 272) & 0x00FF) << 1;
		cmd[0] = appo;
		cmd[0] |= 0xC1;
	}
	cmd[1] = val;
	this->sendwait(cmd, 2, 100);
	if (com >= 258 && com <= 259)
		this->sendwait(cmd, 2, 100);
}

uint8_t *Dali::getReply(void)
{
	return (uint8_t *) this->rx_msg;
}

uint8_t Dali::setDevAddresses(uint8_t start_addr, readdr_type all)
{
	uint32_t addr_dev = 0xFF;
	// Reset all device
	this->sendCommand(32, BROADCAST, 0);	/* RESET */
	delay(400);
	this->sendCommand(0, BROADCAST, 0);	/* Turn off lamps */

	this->sendExtCommand(258, all);		/* INITIALISE devices */
	this->sendExtCommand(259, 0x00);	/* RANDOMISE */
	while (1) {
		/* Find the device */
		addr_dev = this->findDev(0xFFFFFF, 0x800000, 0);
		if (addr_dev == 0xFFFFFFEE)
			return 0xFF;		/* Abort remapping */
		else if (addr_dev == 0xFFFFFFFF)
			break;

		/* Program the short address */
		/* Set the right SEARCHADDR for the found dev */
		this->setSearch(addr_dev);
		/* Program his Short Address */
		this->sendExtCommand(267, (start_addr << 1) | 0x01);
		/* Remove him from the search operation */
		this->sendExtCommand(261, 0x00);
		start_addr++;
		dev_found++;
	}
	this->sendExtCommand(256, 0x00);	/* TERMINATE */
	return start_addr;
}

uint32_t Dali::findDev(uint32_t base_addr, uint32_t delta_addr, uint8_t n)
{
	if (this->dali_cmd & 0x01)
		return 0xFFFFFFEE;
	this->setSearch(base_addr);
	
	/* Control commands on hold during the remap (Abort or finish?) */
	serialDali();
	
	this->sendExtCommand(260, 0x00);	/* COMPARE */
	if (this->rx_msg[0]) {
		if (n == 24) {
			return base_addr;
		} else
			base_addr -= delta_addr;
	} else {
		if (n == 0)
			return 0xFFFFFFFF;
		else if (n > 23) {
			return base_addr + 1;
		} else
			base_addr += delta_addr;
	}
	if (n != 23)
		delta_addr >>= 1;
	n++;

	base_addr = findDev(base_addr, delta_addr, n);
	return base_addr;
}

void Dali::setSearch(uint32_t addr)
{
	/* Control commands on hold during the remap (Abort or finish?) */
	serialDali();
	
	this->sendExtCommand(264, (addr >> 16) & 0xFF);
	
	/* Control commands on hold during the remap (Abort or finish?) */
	serialDali();
	
	this->sendExtCommand(265, (addr >> 8) & 0xFF);
	
	/* Control commands on hold during the remap (Abort or finish?) */
	serialDali();

	this->sendExtCommand(266, addr & 0xFF);
}
