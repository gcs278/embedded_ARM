#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "projdefs.h"
#include "semphr.h"
#include "lpc17xx_gpio.h"

/* include files. */
#include "vtUtilities.h"
#include "vtI2C.h"
#include "LCDtask.h"
#include "i2cTemp.h"
#include "I2CTaskMsgTypes.h"
#include "maptask.h"
#include "conductor.h"
#include "messages.h"
#include "mywebmap.h"
/* *********************************************** */
// definitions and data structures that are private to this file

// I have set this to a large stack size because of (a) using printf() and (b) the depth of function calls
//   for some of the i2c operations	-- almost certainly too large, see LCDTask.c for details on how to check the size
#define INSPECT_STACK 1
#define baseStack 2
#if PRINTF_VERSION == 1
#define mapSTACK_SIZE		((baseStack+3)*configMINIMAL_STACK_SIZE)
#else
#define mapSTACK_SIZE		(baseStack*configMINIMAL_STACK_SIZE)
#endif
// end of defs
/* *********************************************** */

/*typedef struct __myNav {

}myNav*/

#define myMapQLen 10

typedef struct __myMapMsg
{
	uint8_t msgType;
	uint8_t	length;	 // Length of the message to be printed
	uint8_t buf[myMapMaxLen+1]; // On the way in, message to be sent, on the way out, message received (if any) ** dont have

} myMapMsg;

// The Nav task
static portTASK_FUNCTION_PROTO( myMapUpdateTask, pvParameters );

void myStartMapTask(myMapStruct *MapData, unsigned portBASE_TYPE uxPriority, vtI2CStruct *i2c)
{
 	if ((MapData->inQ = xQueueCreate(myMapQLen,sizeof(myMapMsg))) == NULL) {
		VT_HANDLE_FATAL_ERROR(0);
	}
/* Start the task */
	printf("Starting map task!\n");
	portBASE_TYPE retval;
	MapData->dev = i2c;
	if ((retval = xTaskCreate( myMapUpdateTask, ( signed char * ) "Mapping", mapSTACK_SIZE, (void *) MapData, uxPriority, ( xTaskHandle * ) NULL )) != pdPASS) {
		VT_HANDLE_FATAL_ERROR(retval);
	}
}

portBASE_TYPE SendMapValueMsg(myMapStruct *Data, uint8_t msgType, uint8_t* value, portTickType ticksToBlock)
{
	myMapMsg tempBuffer;

	if (Data == NULL) {
		VT_HANDLE_FATAL_ERROR(0);
	}
	tempBuffer.length = sizeof(value);
	if (tempBuffer.length > myMapMaxLen) {
		// no room for this message
		VT_HANDLE_FATAL_ERROR(tempBuffer.length);
	}
	int i;
	for (i=0; i<10; i++) {
	 	tempBuffer.buf[i] = value[i];
	}
	//memcpy(tempBuffer.buf,(char *)&value,sizeof(value));
	tempBuffer.msgType = msgType;
	return(xQueueSend(Data->inQ,(void *) (&tempBuffer),ticksToBlock));
}

int local_len = 0;
int num_walls = 1;
wall mapping_walls[50];
static portTASK_FUNCTION( myMapUpdateTask, pvParameters) {
	//seems like a good idea
	uint8_t rxLen, status;
	uint8_t Buffer[vtI2CMLen];
	uint8_t *valPtr = &(Buffer[0]);
	// Get the parameters
	myMapStruct *param = (myMapStruct *) pvParameters;
	// Get the I2C device pointer
	vtI2CStruct *devPtr = param->dev;
	// Get the sensor data
	myMapMsg msgBuffer;
	uint8_t i2cRoverMoveForward[] = {0x01, 0x00};
	printf("Starting map task for loop!\n");
	int total_len = 0;
	int saw_left=0, saw_right=0;
				   
	// ASSERTION: THE NUMBER OF WALLS WILL NOT EXCEED 100.
	// NOTE: in order for this to break, there would need to be 
	// more than 32 obstacles added to the course!
	
	int k;
	for (k = 0; k < 100; k++)
	{
		mapping_walls[k].length = 0;
	}

	mapping_walls[0].direction = 270;
	mapping_walls[0].length = 0;
	for(;;)
	{
		
		//printf("Saw Left: %d\n", saw_left);
		//printf("Saw Right: %d\n", saw_right);
		for (k = 0; k < num_walls; k++)
			printf("wall#: %d, orientation: %d, length: %d\n", k, mapping_walls[k].direction, mapping_walls[k].length);
		//printf("Orientation: %d\n", mapping_walls[num_walls-1].direction);
		///if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveForward),i2cRoverMoveForward,10) != pdTRUE) {
		//	VT_HANDLE_FATAL_ERROR(0);
		//} 
		// Wait for a message from conductor
		if (xQueueReceive(param->inQ,(void *) &msgBuffer,portMAX_DELAY) != pdTRUE) {
			VT_HANDLE_FATAL_ERROR(0);
		}
		//printf("Received an XQUEUE message!\n");
		
		switch(getMsgType(&msgBuffer)) {
		case RoverMsgSensorAllData:
		{
			//printf("SensorData:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",msgBuffer.buf[0],msgBuffer.buf[1],msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5],msgBuffer.buf[6],msgBuffer.buf[7],msgBuffer.buf[8],msgBuffer.buf[9]);
			//printf("Received sensor data!\n");
			break;
		}
		case RoverMsgMotorLeftData:
		{
			
			printf("MotorMessageMap:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",msgBuffer.buf[0],msgBuffer.buf[1],msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5],msgBuffer.buf[6],msgBuffer.buf[7],msgBuffer.buf[8],msgBuffer.buf[9]);
			// Update the length that the rover has traveled.
			// local_len = local_len + msgBuffer.buf[2];
			mapping_walls[num_walls - 1].length += msgBuffer.buf[2];
			static int i = 0;
			if (i++ >= 10)
			{
				i = 0;		
				update_walls(mapping_walls, num_walls);
			}
			else
				i++;				
										  
			break;
		}
		case RoverMsgMotorRight90:
		{
			num_walls++;
			int new_dir = mapping_walls[num_walls-2].direction - 90;
			mapping_walls[num_walls-1].direction = (new_dir != -90) ? new_dir :	270;
			mapping_walls[num_walls-1].length = 0;
			printf("Received right turn!\n");
			saw_right++;
			break;
		}
		case RoverMsgMotorLeft90:
		{
			num_walls++;
			int new_dir = mapping_walls[num_walls-2].direction + 90;
			mapping_walls[num_walls-1].direction = (new_dir != 360) ? new_dir :	0;
			mapping_walls[num_walls-1].length = 0;
			printf("Received left turn!\n");
			saw_left++;
			break;
		}
		default:
		{
			printf("Received default msg!\n");
			break;
		}
		}
	}
}

