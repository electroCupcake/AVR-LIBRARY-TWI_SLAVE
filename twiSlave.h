/*
 * twiSlave.h
 *
 *  Created on: Aug 12, 2015
 *      Author: jconvertino
 * 
    Copyright (C) 2015 John Convertino

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef TWISLAVE_H_
#define TWISLAVE_H_

#include <stdlib.h>
#include <util/twi.h>
#include <inttypes.h>

//exit status
#define EXIT_DATA_LRG	-6
#define EXIT_BUF_EMP 	-5
#define EXIT_BUF_FUL 	-4
#define EXIT_INV_DATA  	-3
#define EXIT_BUSY    	-2
#define EXIT_TWI_ERR 	-1
#define EXIT_SUCCESS  	 0

//response status
#define TWI_ACK  1
#define TWI_NACK 0
#define TWI_DISCONNECT -1
#define TWI_FINISHED -2

//max values
#define MAX_BUFFER_SIZE 256

typedef enum {disabled, enabled} flag;

//handler function pointer typedefs
//consumer/producer from twi point of view (transmit consumes, recv produces)
typedef int (*consumer)(volatile uint8_t *);
typedef int (*producer)(volatile uint8_t *);

//setup twi to respond to a address, allows handles to be use fnptr_consumer for receiving data,
//and fnptr_producer for sending data. Use NULL for no handlers.
//disable or enable response to gcall (receive only!)
void twiInit(uint8_t address, flag gcall, consumer fnptr_consumer, producer fnptr_producer);

//create a buffer with data, and send that buffer to the address.
int twiSend(uint8_t *data, size_t size);

//create buffer, send address, copy received data to buffer, then copy buffer data to buffer.
int twiRecv(uint8_t *data, size_t size);

//begin transmission using handler
void twiBeginHandlerTrans();

//end a transmission using handler
void twiStopHandlerTrans();

//check if twi is busy (if interrupt enable is on, twi busy)
uint8_t twiBusyCk();

//get last status when ISR was jumped to
uint8_t twiGetLastStatus();

//get last error ISR branched to.
uint8_t twiGetLastError();



#endif /* TWISLAVE_H_ */
