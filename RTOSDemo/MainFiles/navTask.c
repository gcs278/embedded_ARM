#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "projdefs.h"
#include "semphr.h"

/* include files. */
#include "vtUtilities.h"
#include "vtI2C.h"
#include "LCDtask.h"
#include "i2cTemp.h"
#include "I2CTaskMsgTypes.h"
#include "navtask.h"
#include "conductor.h"
/* *********************************************** */
// definitions and data structures that are private to this file

// I have set this to a large stack size because of (a) using printf() and (b) the depth of function calls
//   for some of the i2c operations	-- almost certainly too large, see LCDTask.c for details on how to check the size
#define INSPECT_STACK 1
#define baseStack 2
#if PRINTF_VERSION == 1
#define navSTACK_SIZE		((baseStack+5)*configMINIMAL_STACK_SIZE)
#else
#define navSTACK_SIZE		(baseStack*configMINIMAL_STACK_SIZE)
#endif
// end of defs
/* *********************************************** */
#define myNavQLen 10 

/*typedef struct __myNav {

}myNav*/

typedef struct __myNavMsg
{
	uint8_t msgType;
	uint8_t	length;	 // Length of the message to be printed
	uint8_t buf[myNavMaxLen+1]; // On the way in, message to be sent, on the way out, message received (if any) ** dont have

}myNavMsg;

// The Nav task
static portTASK_FUNCTION_PROTO( myNavUpdateTask, pvParameters );

void myStartNavTask(myNavStruct *NavData, unsigned portBASE_TYPE uxPriority, vtI2CStruct *i2c)
{
 	if ((NavData->inQ = xQueueCreate(myNavQLen,sizeof(myNavMsg))) == NULL) {
		VT_HANDLE_FATAL_ERROR(0);
	}
/* Start the task */
	portBASE_TYPE retval;
	NavData->dev = i2c;
	if ((retval = xTaskCreate( myNavUpdateTask, ( signed char * ) "Navigation", navSTACK_SIZE, (void *) NavData, uxPriority, ( xTaskHandle * ) NULL )) != pdPASS) {
		VT_HANDLE_FATAL_ERROR(retval);
	}
}

portBASE_TYPE SendNavValueMsg(myNavStruct *sensorData, uint8_t msgType, uint8_t* value, portTickType ticksToBlock)
{
	myNavMsg tempBuffer;
	printf("  1 ");
	if (sensorData == NULL) {
		printf("  2 ");
		VT_HANDLE_FATAL_ERROR(0);
	}
	tempBuffer.length = 10;
	if (tempBuffer.length > vtTempMaxLen) {
		// no room for this message
	//	VT_HANDLE_FATAL_ERROR(tempBuffer.length);
	}
	int i;
	for (i=0; i<10; i++) {
	 	tempBuffer.buf[i] = value[i];
	}
	//memcpy(tempBuffer.buf,(char *)&values,sizeof(values));
	tempBuffer.msgType = msgType;
	printf("  for the heck of it");
	return(xQueueSend(sensorData->inQ,(void *) (&tempBuffer),ticksToBlock));
}

int getMsgType(myNavMsg *Buffer)
{
	return(Buffer->msgType);
}

static portTASK_FUNCTION( myNavUpdateTask, pvParameters) {
	//seems like a good idea
//	uint8_t rxLen, status;
	uint8_t Buffer[vtI2CMLen];
	uint8_t *valPtr = &(Buffer[0]);
	// Get the parameters
	myNavStruct *param = (myNavStruct *) pvParameters;
	// Get the I2C device pointer
	vtI2CStruct *devPtr = param->dev;
	// Get the sensor data
	myNavMsg msgBuffer;
	//flag
	uint8_t startNav = 0;


	const uint8_t i2cRoverMoveForward[] = {0x01, 0x00};
	const uint8_t i2cRoverMove90[] = {0x1f, 0x00};

	for(;;)
	{
	// tell what the rover what to do
		
		printf("IAM HERE");
		// Wait for a message from conductor
		if (xQueueReceive(param->inQ,(void *) &msgBuffer,portMAX_DELAY) != pdTRUE) {
			printf("  boooom ");
			VT_HANDLE_FATAL_ERROR(0);
		}
		switch(getMsgType(&msgBuffer)) {
		case navI2CMsgTypeRead:
			startNav = 1;
			break;
		case 0x11:
			if(startNav == 1) {
				int i;
				int distance = 0;
				printf(" %d,%d,%d,%d,%d,%d,%d,%d,%d,%d",msgBuffer.buf[0],msgBuffer.buf[1],msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5],msgBuffer.buf[6],msgBuffer.buf[7],msgBuffer.buf[8],msgBuffer.buf[9]);
				for (i = 0; i < msgBuffer.buf[1] % 10; i++) {
					if ( i < 8)
					distance += msgBuffer.buf[i+2];
				}
				distance = distance / (i+1);
				printf(" hi%dhi ", distance);
				if (distance < 30 && distance >1) {
					printf("  90  "); 
					if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMove90),i2cRoverMove90,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
					} 
				}
				else {
					printf(" Move ");
					/*if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveForward),i2cRoverMoveForward,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
					} */
				}
			}
			break;
		default: {
			//VT_HANDLE_FATAL_ERROR(getMsgType(&msgBuffer));
			break;
		}
		}
		

	}
}
