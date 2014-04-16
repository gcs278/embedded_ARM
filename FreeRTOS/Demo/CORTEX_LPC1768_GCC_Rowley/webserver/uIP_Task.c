/*
    FreeRTOS V7.0.1 - Copyright (C) 2011 Real Time Engineers Ltd.
	

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS tutorial books are available in pdf and paperback.        *
     *    Complete, revised, and edited pdf reference manuals are also       *
     *    available.                                                         *
     *                                                                       *
     *    Purchasing FreeRTOS documentation will not only help you, by       *
     *    ensuring you get running as quickly as possible and with an        *
     *    in-depth knowledge of how to use FreeRTOS, it will also help       *
     *    the FreeRTOS project to continue with its mission of providing     *
     *    professional grade, cross platform, de facto standard solutions    *
     *    for microcontrollers - completely free of charge!                  *
     *                                                                       *
     *    >>> See http://www.FreeRTOS.org/Documentation for details. <<<     *
     *                                                                       *
     *    Thank you for using FreeRTOS, and thank you for your support!      *
     *                                                                       *
    ***************************************************************************


    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    >>>NOTE<<< The modification to the GPL is included to allow you to
    distribute a combined work that includes FreeRTOS without being obliged to
    provide the source code for proprietary components outside of the FreeRTOS
    kernel.  FreeRTOS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
*/

/* Standard includes. */
#include <string.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* uip includes. */
#include "uip.h"
#include "uip_arp.h"
#include "httpd.h"
#include "timer.h"
#include "clock-arch.h"

/* Demo includes. */
#include "EthDev_LPC17xx.h"
#include "EthDev.h"
#include "ParTest.h"

#include "i2ctemp.h"
#include "I2CTaskMsgTypes.h"
#include "Messages.h"

/*-----------------------------------------------------------*/

/* How long to wait before attempting to connect the MAC again. */
#define uipINIT_WAIT    ( 100 / portTICK_RATE_MS )

/* Shortcut to the header within the Rx buffer. */
#define xHeader ((struct uip_eth_hdr *) &uip_buf[ 0 ])

/* Standard constant. */
#define uipTOTAL_FRAME_HEADER_SIZE	54

/*-----------------------------------------------------------*/

/*
 * Setup the MAC address in the MAC itself, and in the uIP stack.
 */
static void prvSetMACAddress( void );

/*
 * Port functions required by the uIP stack.
 */
void clock_init( void );
clock_time_t clock_time( void );

/*-----------------------------------------------------------*/

/* The semaphore used by the ISR to wake the uIP task. */
xSemaphoreHandle xEMACSemaphore = NULL;

/*-----------------------------------------------------------*/

void clock_init(void)
{
	/* This is done when the scheduler starts. */
}
/*-----------------------------------------------------------*/

clock_time_t clock_time( void )
{
	return xTaskGetTickCount();
}
/*-----------------------------------------------------------*/

static RoverCommStruct* roverComm;

void vuIP_Task( void *pvParameters )
{
portBASE_TYPE i;
uip_ipaddr_t xIPAddr;
struct timer periodic_timer, arp_timer;
extern void ( vEMAC_ISR_Wrapper )( void );

	roverComm = ( RoverCommStruct*) pvParameters;

	/* Initialise the uIP stack. */
	timer_set( &periodic_timer, configTICK_RATE_HZ / 2 );
	timer_set( &arp_timer, configTICK_RATE_HZ * 10 );
	uip_init();
	uip_ipaddr( xIPAddr, configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 );
	uip_sethostaddr( xIPAddr );
	uip_ipaddr( xIPAddr, configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 );
	uip_setnetmask( xIPAddr );
	httpd_init();

	/* Create the semaphore used to wake the uIP task. */
	vSemaphoreCreateBinary( xEMACSemaphore );

	/* Initialise the MAC. */
	while( lEMACInit() != pdPASS )
    {
        vTaskDelay( uipINIT_WAIT );
    }

	portENTER_CRITICAL();
	{
		EMAC->IntEnable = ( INT_RX_DONE | INT_TX_DONE );

		/* Set the interrupt priority to the max permissible to cause some
		interrupt nesting. */
		NVIC_SetPriority( ENET_IRQn, configEMAC_INTERRUPT_PRIORITY );

		/* Enable the interrupt. */
		NVIC_EnableIRQ( ENET_IRQn );
		prvSetMACAddress();
	}
	portEXIT_CRITICAL();


	for( ;; )
	{
		/* Is there received data ready to be processed? */
		uip_len = ulGetEMACRxData();

		if( ( uip_len > 0 ) && ( uip_buf != NULL ) )
		{
			/* Standard uIP loop taken from the uIP manual. */
			if( xHeader->type == htons( UIP_ETHTYPE_IP ) )
			{
				uip_arp_ipin();
				uip_input();

				/* If the above function invocation resulted in data that
				should be sent out on the network, the global variable
				uip_len is set to a value > 0. */
				if( uip_len > 0 )
				{
					uip_arp_out();
					vSendEMACTxData( uip_len );
				}
			}
			else if( xHeader->type == htons( UIP_ETHTYPE_ARP ) )
			{
				uip_arp_arpin();

				/* If the above function invocation resulted in data that
				should be sent out on the network, the global variable
				uip_len is set to a value > 0. */
				if( uip_len > 0 )
				{
					vSendEMACTxData( uip_len );
				}
			}
		}
		else
		{
			if( timer_expired( &periodic_timer ) && ( uip_buf != NULL ) )
			{
				timer_reset( &periodic_timer );
				for( i = 0; i < UIP_CONNS; i++ )
				{
					uip_periodic( i );

					/* If the above function invocation resulted in data that
					should be sent out on the network, the global variable
					uip_len is set to a value > 0. */
					if( uip_len > 0 )
					{
						uip_arp_out();
						vSendEMACTxData( uip_len );
					}
				}

				/* Call the ARP timer function every 10 seconds. */
				if( timer_expired( &arp_timer ) )
				{
					timer_reset( &arp_timer );
					uip_arp_timer();
				}
			}
			else
			{
				/* We did not receive a packet, and there was no periodic
				processing to perform.  Block for a fixed period.  If a packet
				is received during this period we will be woken by the ISR
				giving us the Semaphore. */
				xSemaphoreTake( xEMACSemaphore, configTICK_RATE_HZ / 2 );
			}
		}
	}
}
/*-----------------------------------------------------------*/

static void prvSetMACAddress( void )
{
struct uip_eth_addr xAddr;

	/* Configure the MAC address in the uIP stack. */
	xAddr.addr[ 0 ] = configMAC_ADDR0;
	xAddr.addr[ 1 ] = configMAC_ADDR1;
	xAddr.addr[ 2 ] = configMAC_ADDR2;
	xAddr.addr[ 3 ] = configMAC_ADDR3;
	xAddr.addr[ 4 ] = configMAC_ADDR4;
	xAddr.addr[ 5 ] = configMAC_ADDR5;
	uip_setethaddr( xAddr );
}
/*-----------------------------------------------------------*/

void vApplicationProcessFormInput( char *pcInputString )
{


char *c;
extern void vParTestSetLEDState( long lState );

	/* Process the form input sent by the IO page of the served HTML. */

	c = strstr( pcInputString, "?" );
    if( c )
    {
		/* Turn the FIO1 LED's on or off in accordance with the check box status. */
		if( !strcmp( c, "?run=1" ) )
		{
			uint8_t i2cRoverMoveForward[] = {RoverMsgMotorForward, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveForward),i2cRoverMoveForward,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			vParTestSetLEDState( pdTRUE );
		}
		else if ( !strcmp( c, "?run=0" ) ) {
			uint8_t i2cRoverMoveStop[] = {0x05, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveStop),i2cRoverMoveStop,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			vParTestSetLEDState( pdTRUE );
		}
		else if ( !strcmp( c, "?run=2" ) ) {
			uint8_t i2cRoverMoveRight[] = {RoverMsgMotorRight, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveRight),i2cRoverMoveRight,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			vParTestSetLEDState( pdTRUE );
		}
		else if ( !strcmp( c, "?run=3" ) ) {
			uint8_t i2cRoverMoveLeft[] = {RoverMsgMotorLeft, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveLeft),i2cRoverMoveLeft,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			vParTestSetLEDState( pdTRUE );
		}
		else if (!strcmp( c, "?run=4" ) ) {
			startGettingMotor("DATASTART");
		}
		else if ( !strcmp( c, "?run=5" ) ) {
			stopGettingMotor("DATASTOP");
		}
		else if ( !strcmp( c, "?run=6" ) ) {
		 	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorBack, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?run=7" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorLeft2, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?run=8" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorForwardCMDelim, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?run=9" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorLeft90, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?run=10" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorRight90, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?run=11" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorLeft5, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?run=12" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorRight5, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?run=13" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorLeft10, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?run=14" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorRight10, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?speed=0" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorSpeedCreepin, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?speed=1" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorSpeedSlow, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?speed=2" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorSpeedMedium, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?speed=3" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorSpeedMediumFast, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?speed=4" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgMotorSpeedFastBRAH, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else if ( !strcmp( c, "?wall=0" )) {
		   	uint8_t i2cRoverMoveBack[] = {RoverMsgTurnOffWallTracking, 0x00};
			if (vtI2CEnQ(roverComm->i2cStruct,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
		}
		else
		{
			vParTestSetLEDState( pdFALSE );
		}
    }
}

