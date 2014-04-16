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
	if (sensorData == NULL) {
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

	int lastDistance = 0; // The last distance calculated

	int extender = 0; // For extending the time between right turns
	
	unsigned char messageCount = 0;

	uint8_t i2cRoverMoveForward[] = {RoverMsgMotorForward, 0x00};
	uint8_t i2cRoverMove90[] = {RoverMsgMotorLeft90, 0x33};
	uint8_t i2cRoverMoveLeft2[] = {RoverMsgMotorLeft2, 0x00};
	uint8_t i2cRoverMoveRight2[] = {RoverMsgMotorRight2, 0x00};

	for(;;)
	{
	// tell what the rover what to do
		
		//printf(" navTopFor ");
		// Wait for a message from conductor
		if (xQueueReceive(param->inQ,(void *) &msgBuffer,portMAX_DELAY) != pdTRUE) {
			printf("  boooom ");
			VT_HANDLE_FATAL_ERROR(0);
		}
		switch(getMsgType(&msgBuffer)) {
		case navI2CMsgTypeRead:
			printf(" navStart\n");
			startNav = 1;
			break;
		case 0x89:
			insertCountDef(RoverMsgMotorLeft90);
			i2cRoverMove90[1] = getMsgCount();
		  	if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMove90),i2cRoverMove90,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			incrementMsgCount();
			break;
		case 0x11:
			printf("NavMessage:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",msgBuffer.buf[0],msgBuffer.buf[1],msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5],msgBuffer.buf[6],msgBuffer.buf[7],msgBuffer.buf[8],msgBuffer.buf[9]);
			printf("Extender: %d\n",extender);
			extender--;
			if ( extender < 0 ) {
			//if(startNav == 1) {
				int i;
				int frontDistance = 0;
				if (msgBuffer.buf[1] >= 4 && msgBuffer.buf[2] != 0xFF && msgBuffer.buf[3] != 0xFF ) {
					frontDistance = msgBuffer.buf[2]+msgBuffer.buf[3];
					frontDistance = frontDistance/2;
					//sideDistance = msgBuffer.buf[4];
					//sideDistance += msgBuffer.buf[5];
					//sideDistance = sideDistance/2;
				}

			/*r	for (i = 0; i < msgBuffer.buf[1] % 10; i++) {
					if ( i < 8)
					distance += msgBuffer.buf[i+2];
				}
				distance = distance / (i+1);	*/
				printf("FrontDistance: %d\n", frontDistance);
			 if (frontDistance < 50 && lastDistance < 50 && frontDistance >1 && lastDistance > 1) {
					printf("----sent90\n"); 

					insertCountDef(RoverMsgMotorLeft90);
					i2cRoverMove90[1] = getMsgCount();
					printf("MessageCount: %d\n", getMsgCount());
					if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMove90),i2cRoverMove90,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
					} 	 
					incrementMsgCount();
					extender = 15;
				}
				else {
					//printf("----sentMove\n");
					/*if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveForward),i2cRoverMoveForward,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
					} */
				}
				lastDistance = frontDistance;
		//	}
		}
			break;
		default: {
			printf("  navDefault\n");
			//VT_HANDLE_FATAL_ERROR(getMsgType(&msgBuffer));
			break;
		}
		}
		

	}
}
