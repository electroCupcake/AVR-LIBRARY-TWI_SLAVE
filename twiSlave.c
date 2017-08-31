/*
 * twiSlave.c
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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

#include "twiSlave.h"

//idea for state machine
//typedef enum {idle, sla_r, sla_w, send, recv, error} twiStates;

//struct for containing all data members for twi driver
struct
{
	uint8_t lastStatus;
	uint8_t lastError;
	uint8_t tries;
	uint8_t bufferSize;
	uint8_t bufferIndex;
	uint8_t bufferData[MAX_BUFFER_SIZE];
	flag handlerEnable;
	//function pointers
	consumer fnptr_consumer;
	producer fnptr_producer;
} twi;

//functions for twi control register setup and clearing interrupt
//connect to bus and listen
static inline void twiEnable()
{
	TWCR = (1 << TWEA) | (1 << TWEN);
}

//can be used for pulse stretching to give slave more time
static inline void twiStretch()
{
	TWCR = (1 << TWEA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE) | (1 << TWSTO);
}

static inline void twiConnect()
{
	TWCR =  (1 << TWEA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
}

//disconnect from bus
static inline void twiDisconnect()
{
	TWCR = (1 << TWEA) | (1 << TWINT) | (1 << TWEN);
}

//done but stay connected
static inline void twiFinished()
{
	TWCR = (1 << TWEA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
}


//send ack pulse
static inline void twiACK()
{
	TWCR = (1 << TWEA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
}

//send nack pulse
static inline void twiNACK()
{
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
}

static inline void twiAction(int value)
{
	switch(value)
	{
	case TWI_NACK:
		twiNACK();
		break;
	case TWI_FINISHED:
		twiFinished();
		break;
	case TWI_DISCONNECT:
		twiDisconnect();
		break;
	default:
		twiACK();
		break;
	}
}


//init twi info, calculate scl_speed for TWBR, also enable interrupts
void twiInit(uint8_t address, flag gcall, consumer fnptr_consumer, producer fnptr_producer)
{
	TWSR = 0;
	TWCR = 0;
	TWAR = 0;

	switch(gcall)
	{
		case enabled:
			TWAR = 1;
		case disabled:
		default:
			TWAR |= address << 1;
			break;
	}

	twi.tries = 0;
	twi.lastStatus = 0;
	twi.bufferIndex = 0;
	twi.bufferSize = 0;
	twi.handlerEnable = disabled;
	twi.fnptr_consumer = fnptr_consumer;
	twi.fnptr_producer = fnptr_producer;

	memset(twi.bufferData, 0, MAX_BUFFER_SIZE);

	twiEnable();

	sei();
}

//write to buffer for twi data send
int twiSend(uint8_t *data, size_t size)
{

	if(twiBusyCk())
	{
		return EXIT_BUSY;
	}

	if(data == NULL)
	{
		return EXIT_INV_DATA;
	}

	if(size > MAX_BUFFER_SIZE)
	{
		return EXIT_DATA_LRG;
	}

	//deep copy data
	memcpy(twi.bufferData, data, size);

	twi.bufferSize = size;

	twi.bufferIndex = 0;

	twiConnect();

	return EXIT_SUCCESS;
}

//read from buffer after twi has sent address and built up data
int twiRecv(uint8_t *data, size_t size)
{
	if(twiBusyCk())
	{
		return EXIT_BUSY;
	}

	if(data == NULL)
	{
		return EXIT_INV_DATA;
	}

	if(size > MAX_BUFFER_SIZE)
	{
		return EXIT_DATA_LRG;
	}

	twi.bufferSize = size;

	twiConnect();

	while(twiBusyCk());

	if(twi.bufferIndex != twi.bufferSize)
	{
		return EXIT_TWI_ERR;
	}

	//deep copy
	memcpy(data, twi.bufferData, size);

	return EXIT_SUCCESS;
}

//begin transmission using handler
void twiBeginHandlerTrans()
{
	twi.handlerEnable = enabled;
	twiConnect();
}

//stop handler, allow ISR to end transmission correctly
void twiStopHandlerTrans()
{
	twi.handlerEnable = disabled;
	twiDisconnect();
}

//check if the the interrupt is enabled, if so, ISR in progress and were busy
uint8_t twiBusyCk()
{
	return (TWCR & (1 << TWIE));
}

//return last status from ISR vector jump
uint8_t twiGetLastStatus()
{
	return twi.lastStatus;
}

//return last error caught by ISR
uint8_t twiGetLastError()
{
	return twi.lastError;
}

//ISR will activate when TWINT is 1, must be reset manually
ISR(TWI_vect)
{
	twi.lastStatus = TW_STATUS;
	twi.lastError = 0;

	switch(TW_STATUS)
	{
	case TW_SR_SLA_ACK:
	case TW_SR_GCALL_ACK:
		twi.bufferIndex = 0;
		twiACK();
		break;
	case TW_SR_DATA_ACK:
	case TW_SR_GCALL_DATA_ACK:
		//handler enabled, and there is a valid handler use it.
		if((twi.fnptr_consumer != NULL) && (twi.handlerEnable == enabled))
		{
			twiAction(twi.fnptr_consumer(&TWDR));
		}
		//other wise use twiRecv routine if there is room and a valid buffer is available
		else if(twi.bufferIndex < twi.bufferSize)
		{
			twi.bufferData[twi.bufferIndex] = TWDR;
			twi.bufferIndex++;
			twiACK();
		}
		//nothing else to do but stop
		else
		{
			twiDisconnect();
		}
		break;
	case TW_ST_SLA_ACK:
		twi.bufferIndex = 0;
	case TW_ST_DATA_ACK:
		//valid handler and its enabled, use it
		if((twi.fnptr_producer != NULL) && (twi.handlerEnable == enabled))
		{
			//decides on either a nack, ack, or stop
			twiAction(twi.fnptr_producer(&TWDR));
		}
		//else use twiSend, if buffer is allocated and there is room
		else if(twi.bufferIndex < twi.bufferSize)
		{
			TWDR = twi.bufferData[twi.bufferIndex];
			twi.bufferIndex++;
			twiAction(twi.bufferSize - twi.bufferIndex);
		}
		//stop transmission if all other conditions fail and we have for some reason gotten to this state.
		else
		{
			twiDisconnect();
		}
		break;
	case TW_ST_LAST_DATA:
	case TW_ST_DATA_NACK:
		twiDisconnect();
		break;
	case TW_SR_DATA_NACK:
	case TW_SR_GCALL_DATA_NACK:
	case TW_SR_STOP:
		twiDisconnect();
		break;
	//error states
	case TW_ST_ARB_LOST_SLA_ACK:
	case TW_SR_ARB_LOST_SLA_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		twi.lastError = twi.lastStatus;
		twiDisconnect();
		break;
	}
}
