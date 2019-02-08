/*
             LUFA Library
     Copyright (C) Dean Camera, 2011.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2011  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Header file for Adelino.h
 */

#ifndef _CDC_H_
#define _CDC_H_

/** Version major of the CDC bootloader. */
#define BOOTLOADER_VERSION_MAJOR     0x01

/** Version minor of the CDC bootloader. */
#define BOOTLOADER_VERSION_MINOR     0x00

/** Hardware version major of the CDC bootloader. */
#define BOOTLOADER_HWVERSION_MAJOR   0x01

/** Hardware version minor of the CDC bootloader. */
#define BOOTLOADER_HWVERSION_MINOR   0x00

/** Eight character bootloader firmware identifier reported to the host when requested */
#define SOFTWARE_IDENTIFIER          "ADELINO"

/** Setup D13 (PC7) and ACT (PD5) */
#define LED_SETUP()     DDRC |= (1<<7); DDRD |= (1<<5);
#define L_LED_OFF()     PORTC &= ~(1<<7)
#define L_LED_ON()      PORTC |= (1<<7)
#define L_LED_TOGGLE()  PORTC ^= (1<<7)

/** We use just 1 led (ACT) for comm I/O signaling */
#define ACT_LED_OFF()     PORTD &= ~(1<<5)
#define ACT_LED_ON()      PORTD |= (1<<5)
#define ACT_LED_TOGGLE()  PORTD ^= (1<<5)

/** Simply plug a LED between A0,A1 for basic debug */
#define DEBUG_SETUP()   DDRF |= _BV(6) | _BV(7)
#define DEBUG_ON()      PORTF |= _BV(7)
#define DEBUG_OFF()     PORTF &= ~_BV(7);

#endif

// EOF
