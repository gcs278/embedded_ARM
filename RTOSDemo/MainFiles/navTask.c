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

/* Start the task */
	portBASE_TYPE retval;
	NavData->dev = i2c;
	if ((retval = xTaskCreate( myNavUpdateTask, ( signed char * ) "Navigation", navSTACK_SIZE, (void *) NavData, uxPriority, ( xTaskHandle * ) NULL )) != pdPASS) {
		VT_HANDLE_FATAL_ERROR(retval);
	}
}

portBASE_TYPE SendNavValueMsg(myNavStruct *sensorData, uint8_t msgType, uint8_t value, portTickType ticksToBlock)
{
	myNavMsg tempBuffer;

	if (sensorData == NULL) {
		VT_HANDLE_FATAL_ERROR(0);
	}
	tempBuffer.length = sizeof(value);
	if (tempBuffer.length > myNavMaxLen) {
		// no room for this message
		VT_HANDLE_FATAL_ERROR(tempBuffer.length);
	}
	memcpy(tempBuffer.buf,(char *)&value,sizeof(value));
	tempBuffer.msgType = msgType;
	return(xQueueSend(sensorData->inQ,(void *) (&tempBuffer),ticksToBlock));
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


	const uint8_t i2cRoverMoveForward[] = {0x01, 0x00};

	for(;;)
	{
		printf(&msgBuffer);
		// Wait for a message from conductor
		if (xQueueReceive(param->inQ,(void *) &msgBuffer,portMAX_DELAY) != pdTRUE) {
			VT_HANDLE_FATAL_ERROR(0);
		}
		// tell what the rover what to do
		if (vtI2CEnQ(devPtr,navI2CMsgTypeRead,0x4F,sizeof(i2cRoverMoveForward),i2cRoverMoveForward,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}

	}
}
