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

#include <Dali.h>

/*
 * Objects
 */
 
Dali dali0, dali1;

/*
 * Setup
 */
 
void setup()
{
	delay(500);
	Serial.begin(115200);
	
	delay(500);
	Serial.println("DALI testing program - ver. 1.00");
	Serial.println("Cosino Mega 2560 extension");
	Serial.println("Luca Zulberti <@cosino.io>");
	
	delay(500);
	dali0.begin(18, 10);	/* Overwrite OC1A and OC1B */
	dali1.begin(A14, A15);	/* Overwrite ADCch 14 and 15 */
}

/*
 * Loop
 */

void loop()
{
	serialDali();
}
