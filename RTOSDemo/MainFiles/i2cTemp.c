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
// this is a test
#include "mywebmap.h"
#include "Messages.h"
/* *********************************************** */
// definitions and data structures that are private to this file
// Length of the queue to this task
#define vtTempQLen 10 
// actual data structure that is sent in a message
typedef struct __vtTempMsg {
	uint8_t msgType;
	uint8_t	length;	 // Length of the message to be printed
	uint8_t buf[vtTempMaxLen+1]; // On the way in, message to be sent, on the way out, message received (if any)
} vtTempMsg;

// I have set this to a large stack size because of (a) using printf() and (b) the depth of function calls
//   for some of the i2c operations	-- almost certainly too large, see LCDTask.c for details on how to check the size
#define baseStack 3
#if PRINTF_VERSION == 1
#define i2cSTACK_SIZE		((baseStack+5)*configMINIMAL_STACK_SIZE)
#else
#define i2cSTACK_SIZE		(baseStack*configMINIMAL_STACK_SIZE)
#endif

// end of defs
/* *********************************************** */

/* The i2cTemp task. */
static portTASK_FUNCTION_PROTO( vi2cTempUpdateTask, pvParameters );

int moveForwardFlag = 0;
int moveStopFlag = 0;
int moveRightFlag = 0;
int moveBackFlag = 0;
int moveLeftFlag = 0;
int motorDataFlag = 0;
int moveUseFlag = 0;
int moveMapFlag = 0;
int moveStartFlag = 0;
char message[30];
int count = 0;
int timerExtender = 0;

/*-----------------------------------------------------------*/
// Public API
void vStarti2cTempTask(vtTempStruct *params,unsigned portBASE_TYPE uxPriority, vtI2CStruct *i2c,vtLCDStruct *lcd)
{
	// Create the queue that will be used to talk to this task
	if ((params->inQ = xQueueCreate(vtTempQLen,sizeof(vtTempMsg))) == NULL) {
		VT_HANDLE_FATAL_ERROR(0);
	}
	/* Start the task */
	portBASE_TYPE retval;
	params->dev = i2c;
	params->lcdData = lcd;
	if ((retval = xTaskCreate( vi2cTempUpdateTask, ( signed char * ) "i2cTemp", i2cSTACK_SIZE, (void *) params, uxPriority, ( xTaskHandle * ) NULL )) != pdPASS) {
		VT_HANDLE_FATAL_ERROR(retval);
	}
}

portBASE_TYPE SendTempTimerMsg(vtTempStruct *tempData,portTickType ticksElapsed,portTickType ticksToBlock)
{
	if (tempData == NULL) {
		VT_HANDLE_FATAL_ERROR(0);
	}
	vtTempMsg tempBuffer;
	tempBuffer.length = sizeof(ticksElapsed);
	if (tempBuffer.length > vtTempMaxLen) {
		// no room for this message
		VT_HANDLE_FATAL_ERROR(tempBuffer.length);
	}
	memcpy(tempBuffer.buf,(char *)&ticksElapsed,sizeof(ticksElapsed));
	tempBuffer.msgType = TempMsgTypeTimer;
	return(xQueueSend(tempData->inQ,(void *) (&tempBuffer),ticksToBlock));
}

portBASE_TYPE SendTempValueMsg(vtTempStruct *tempData,uint8_t msgType,uint8_t* values,portTickType ticksToBlock)
{
	vtTempMsg tempBuffer;
	if (tempData == NULL) {
		VT_HANDLE_FATAL_ERROR(0);
	}
	tempBuffer.length = 10;
	if (tempBuffer.length > vtTempMaxLen) {
		// no room for this message
	//	VT_HANDLE_FATAL_ERROR(tempBuffer.length);
	}
	int i;
	for (i=0; i<10; i++) {
	 	tempBuffer.buf[i] = values[i];
	}
	//memcpy(tempBuffer.buf,(char *)&values,sizeof(values));
	tempBuffer.msgType = msgType;
	return(xQueueSend(tempData->inQ,(void *) (&tempBuffer),ticksToBlock));
}

// End of Public API
/*-----------------------------------------------------------*/
int getMsgType(vtTempMsg *Buffer)
{
	return(Buffer->msgType);
}
uint8_t getValue(vtTempMsg *Buffer)
{
	uint8_t *ptr = (uint8_t *) Buffer->buf;
	return(*ptr);
}

// I2C commands for commanding the Rover
	const uint8_t i2cCmdInit[]= {0xAC,0x00};

	// DEBUGING MOVING
	uint8_t i2cRoverMoveForward[] = {0x01, 0x00};
	uint8_t i2cRoverMoveRight[] = {0x02, 0x00};
	uint8_t i2cRoverMoveLeft[] = {0x03, 0x00};
	uint8_t i2cRoverMoveBack[] = {0x04, 0x00};
	uint8_t i2cRoverMoveStop[] = {0x05, 0x00};
	uint8_t i2cBrightBlue[] = {'n', 0x00,0x00,0xff};

	// REQUESTING DATA
	uint8_t i2cRoverMsgMotorLeftData[] = {RoverMsgMotorLeftData, 0x00};
	uint8_t i2cRoverSensorFullData[] = {0x11, 0x00};

	const uint8_t i2cCmdStartConvert[]= {0xEE};
	const uint8_t i2cCmdStopConvert[]= {0x22};
	const uint8_t i2cCmdReadVals[]= {0xAA};

// end of I2C command definitions

// Definitions of the states for the FSM below
const uint8_t fsmStateInit1Sent = 0;
const uint8_t fsmStateInit2Sent = 1;
const uint8_t fsmStateTempRead1 = 2;
const uint8_t fsmStateTempRead2 = 3;
const uint8_t fsmStateTempRead3 = 4;
// This is the actual task that is run
static portTASK_FUNCTION( vi2cTempUpdateTask, pvParameters )
{
	const int buffer_size = vtLCDMaxLen;
	char analog_buffer[buffer_size];
	memset(analog_buffer, 0, buffer_size);
	unsigned char buffer_loc = 0;
	//float temperature = 0.0;
	//float countPerC = 100.0, countRemain=0.0;
															
	// Get the parameters
	vtTempStruct *param = (vtTempStruct *) pvParameters;
	// Get the I2C device pointer
	vtI2CStruct *devPtr = param->dev;
	// Get the LCD information pointer
	vtLCDStruct *lcdData = param->lcdData;
	// String buffer for printing
	// char lcdBuffer[vtLCDMaxLen+1];
	// Buffer for receiving messages
	vtTempMsg msgBuffer;
	uint8_t currentState;

	// Assumes that the I2C device (and thread) have already been initialized

	// This task is implemented as a Finite State Machine.  The incoming messages are examined to see
	//   whether or not the state should change.
	//
	// Temperature sensor configuration sequence (DS1621) Address 0x4F
	if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempInit,0x4F,sizeof(i2cCmdInit),i2cCmdInit,0) != pdTRUE) {
		VT_HANDLE_FATAL_ERROR(0);
	}
	currentState = fsmStateInit1Sent;
	// Like all good tasks, this should never exit
	for(;;)
	{
		if ( moveForwardFlag ) {
			count += 1;
			
			i2cRoverMoveForward[1] = count;
			// NOTE: THAT 2 DETERMINES RX LENGTH!!
			// Tell rover to move forward
			if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveForward),i2cRoverMoveForward,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}

			// Request data from the rover
		//	if (vtI2CEnQ(devPtr,roverI2CMsgTypeFullData,0x4F,sizeof(i2cRoverRequestData),i2cRoverRequestData,20) != pdTRUE) {
		//		VT_HANDLE_FATAL_ERROR(0);
		//	}

			// Print something on LCD
			if (SendLCDPrintMsg(lcdData,strnlen(message,vtLCDMaxLen),message,portMAX_DELAY) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			moveForwardFlag = 0;
		}
		if ( moveStopFlag ) {
			count += 1;

			i2cRoverMoveStop[1] = count;
			// NOTE: THAT 2 DETERMINES RX LENGTH!!
			// Tell rover to move forward
			if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveStop),i2cRoverMoveStop,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}

			// Print something on LCD
			if (SendLCDPrintMsg(lcdData,strnlen(message,vtLCDMaxLen),message,portMAX_DELAY) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			moveStopFlag = 0;
		}

		if ( moveBackFlag ) {
			count += 1;

			i2cRoverMoveBack[1] = count;
			// NOTE: THAT 2 DETERMINES RX LENGTH!!
			// Tell rover to move forward
			if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveBack),i2cRoverMoveBack,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}

			// Print something on LCD
			if (SendLCDPrintMsg(lcdData,strnlen(message,vtLCDMaxLen),message,portMAX_DELAY) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			moveBackFlag = 0;
		}

		if ( moveLeftFlag ) {
			count += 1;

			i2cRoverMoveLeft[1] = count;
			// NOTE: THAT 2 DETERMINES RX LENGTH!!
			// Tell rover to move forward
			if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveLeft),i2cRoverMoveLeft,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}

			// Print something on LCD
			if (SendLCDPrintMsg(lcdData,strnlen(message,vtLCDMaxLen),message,portMAX_DELAY) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			moveLeftFlag = 0;
		}

		if ( moveRightFlag ) {
			count += 1;

			i2cRoverMoveRight[1] = count;
			// NOTE: THAT 2 DETERMINES RX LENGTH!!
			// Tell rover to move forward
			if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveRight),i2cRoverMoveRight,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}

			// Print something on LCD
			if (SendLCDPrintMsg(lcdData,strnlen(message,vtLCDMaxLen),message,portMAX_DELAY) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			moveRightFlag = 0;
		}
		if ( moveUseFlag ) {
			count += 1;

			//i2cRoverMoveRight[1] = count;
			// NOTE: THAT 2 DETERMINES RX LENGTH!!
			// Tell rover to move forward
			//if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveRight),i2cRoverMoveRight,10) != pdTRUE) {
			//	VT_HANDLE_FATAL_ERROR(0);
			//}

			// Print something on LCD
			if (SendLCDPrintMsg(lcdData,strnlen(message,vtLCDMaxLen),message,portMAX_DELAY) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			moveUseFlag = 0;
		}
		if ( moveStartFlag ) {
			count += 1;

			//i2cRoverMoveRight[1] = count;
			// NOTE: THAT 2 DETERMINES RX LENGTH!!
			// Tell rover to move forward
			//if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveRight),i2cRoverMoveRight,10) != pdTRUE) {
			//	VT_HANDLE_FATAL_ERROR(0);
			//}

			// Print something on LCD
			if (SendLCDPrintMsg(lcdData,strnlen(message,vtLCDMaxLen),message,portMAX_DELAY) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			moveStartFlag = 0;
		}
		if ( moveMapFlag ) {
			count += 1;

			//i2cRoverMoveRight[1] = count;
			// NOTE: THAT 2 DETERMINES RX LENGTH!!
			// Tell rover to move forward
			//if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveRight),i2cRoverMoveRight,10) != pdTRUE) {
			//	VT_HANDLE_FATAL_ERROR(0);
			//}

			// Print something on LCD
			if (SendLCDPrintMsg(lcdData,strnlen(message,vtLCDMaxLen),message,portMAX_DELAY) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			moveMapFlag = 0;
		}

		// Wait for a message from either a timer or from an I2C operation
		if (xQueueReceive(param->inQ,(void *) &msgBuffer,portMAX_DELAY) != pdTRUE) {
			VT_HANDLE_FATAL_ERROR(0);
		}

		// Now, based on the type of the message and the state, we decide on the new state and action to take
		switch(getMsgType(&msgBuffer)) {
		case vtI2CMsgTypeTempInit: {
			if (currentState == fsmStateInit1Sent) {
				currentState = fsmStateInit2Sent;
				// Must wait 10ms after writing to the temperature sensor's configuration registers(per sensor data sheet)
				vTaskDelay(10/portTICK_RATE_MS);
				// Tell it to start converting
				if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempInit,0x4F,sizeof(i2cCmdStartConvert),i2cCmdStartConvert,0) != pdTRUE) {
					VT_HANDLE_FATAL_ERROR(0);
				}
			} else 	if (currentState == fsmStateInit2Sent) {
				currentState = fsmStateTempRead1;
			} else {
				// unexpectedly received this message
				VT_HANDLE_FATAL_ERROR(0);
			}
			break;
		}
		case RoverMsgMotorLeftData: {

			char str[20];
		//	sprintf(str,"%d,%d,%d,%d,%d",msgBuffer.buf[0],msgBuffer.buf[1],msgBuffer.buf[2],msgBuffer.buf[3]
		//	, msgBuffer.buf[4]);//,msgBuffer.buf[6],msgBuffer.buf[7],msgBuffer.buf[8],msgBuffer.buf[9],msgBuffer.buf[10]);

			int time = (msgBuffer.buf[1]*200); // in ms
			int i;
			int distance = 0;

			for (i = 0; i < msgBuffer.buf[1] % 10; i++) {
				if ( i < 8)
					distance += msgBuffer.buf[i+2];
			}

			if ( time > 100000 ) {
			 	time = 10000;
			}
			distance = distance*0.135;
			int cmPerSec = distance/((float)time/1000);
			/*if (vtI2CEnQ(devPtr,roverI2CMsgTypeFullData,0x09,sizeof(i2cBrightBlue),i2cBrightBlue,0) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}		*/

			sprintf(str,"%d,%dms%dcm%dc/s",msgBuffer.buf[0],time,distance,cmPerSec);
			//sprintf(str,"testlol");
		    // Print something on LCD
			if (SendLCDPrintMsg(lcdData,strnlen(str,vtLCDMaxLen),str,portMAX_DELAY) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}	
			
			 break;
		}
		case TempMsgTypeTimer: {
			// Timer messages never change the state, they just cause an action (or not) 
			if ((currentState != fsmStateInit1Sent) && (currentState != fsmStateInit2Sent)) {
			/*if (vtI2CEnQ(devPtr,roverI2CMsgTypeFullData,0x09,sizeof(i2cBrightBlue),i2cBrightBlue,0) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}		 */
				if (motorDataFlag) {
					if (timerExtender == 5) {
						printf("MessageCount(Mtr): %d\n", getMsgCount());
						i2cRoverMsgMotorLeftData[1] = getMsgCount();
						insertCountDef(RoverMsgMotorLeftData);
						if (vtI2CEnQ(devPtr,roverI2CMsgTypeFullData,0x4F,sizeof(i2cRoverMsgMotorLeftData),i2cRoverMsgMotorLeftData,10) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}
						incrementMsgCount();
						timerExtender = 0;
					} else {
						//count ++;
						//i2cRoverMotorData[1] = count;
						//if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMotorData),i2cRoverMotorData,10) != pdTRUE) {
						//	VT_HANDLE_FATAL_ERROR(0);
						//}
	
						printf("MessageCount(sensor): %d\n", getMsgCount());
						i2cRoverSensorFullData[1] = getMsgCount();
						insertCountDef(RoverMsgSensorAllData);
						if (vtI2CEnQ(devPtr,roverI2CMsgTypeFullData,0x4F,sizeof(i2cRoverSensorFullData),i2cRoverSensorFullData,10) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}
						incrementMsgCount();
	
						timerExtender++;
					}
				}

				// Read in the values from the temperature sensor
				// We have three transactions on i2c to read the full temperature 
				//   we send all three requests to the I2C thread (via a Queue) -- responses come back through the conductor thread
				// Temperature read -- use a convenient routine defined above
//				if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cCmdReadVals),i2cCmdReadVals,2) != pdTRUE) {
//					VT_HANDLE_FATAL_ERROR(0);
//				}
				/*
				if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cCmdReadVals),i2cCmdReadVals,2) != pdTRUE) {
					VT_HANDLE_FATAL_ERROR(0);
				}
				// Read in the read counter
				if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead2,0x4F,sizeof(i2cCmdReadCnt),i2cCmdReadCnt,1) != pdTRUE) {
					VT_HANDLE_FATAL_ERROR(0);
				}
				// Read in the slope;
				if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead3,0x4F,sizeof(i2cCmdReadSlope),i2cCmdReadSlope,1) != pdTRUE) {
					VT_HANDLE_FATAL_ERROR(0);
				}
				*/

			} else {
				// just ignore timer messages until initialization is complete
			} 
			break;
		}
		case vtI2CMsgTypeTempRead1: {
			// Sanity check.
			if (currentState != fsmStateTempRead1) {
				VT_HANDLE_FATAL_ERROR(0);
			}

			char str[20];
		//	sprintf(str,"%d,%d,%d,%d,%d",msgBuffer.buf[0],msgBuffer.buf[1],msgBuffer.buf[2],msgBuffer.buf[3]
		//	, msgBuffer.buf[4]);//,msgBuffer.buf[6],msgBuffer.buf[7],msgBuffer.buf[8],msgBuffer.buf[9],msgBuffer.buf[10]);

//			int time = (msgBuffer.buf[1]*200); // in ms
//			int i;
//			int distance = 0;
//			for (i = 0; i < msgBuffer.buf[1] % 10; i++) {
//				if ( i < 8)
//					distance += msgBuffer.buf[i+2];
//			}
//			if ( time > 100000 ) {
//			 	time = 10000;
//			}
//			distance = distance*0.135;
//			int cmPerSec = distance/((float)time/1000);
//			sprintf(str,"%d,%dms%dcm%dc/s",msgBuffer.buf[0],time,distance,cmPerSec);

			int i;
			int distance = 0;
		//	sprintf(str,"%d,%d,%d,%d",msgBuffer.buf[0],msgBuffer.buf[1],msgBuffer.buf[2],msgBuffer.buf[3]);
			for (i = 0; i < msgBuffer.buf[1] % 10; i++) {
				if ( i < 8)
					distance += msgBuffer.buf[i+2];
			}
			distance = distance / (i+1);

			sprintf(str,"%d,%dcm",msgBuffer.buf[0],msgBuffer.buf[2]);

		    // Print something on LCD
			if (SendLCDPrintMsg(lcdData,strnlen(str,vtLCDMaxLen),str,portMAX_DELAY) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}

			break;
		}
		case roverI2CMsgTypeFullData: {
			// Do something with the data from sensors

			// MAY NOT BE BUFFER SIZE (That's size of our display buffer)
			int i=0;
			for (i; i<buffer_size; i++) {
				// Get all the bytes
				analog_buffer[i] = msgBuffer.buf[i];
			}

			// Display or something
		}
		/*
		case vtI2CMsgTypeTempRead1: {
			if (currentState == fsmStateTempRead1) {
				currentState = fsmStateTempRead2;
				temperature = getValue(&msgBuffer);
			} else {
				// unexpectedly received this message
				VT_HANDLE_FATAL_ERROR(0);
			}
			break;
		}
		case vtI2CMsgTypeTempRead2: {
			if (currentState == fsmStateTempRead2) {
				currentState = fsmStateTempRead3;
				countRemain = getValue(&msgBuffer);
			} else {
				// unexpectedly received this message
				VT_HANDLE_FATAL_ERROR(0);
			}
			break;
		}
		case vtI2CMsgTypeTempRead3: {
			if (currentState == fsmStateTempRead3) {
				currentState = fsmStateTempRead1;
				countPerC = getValue(&msgBuffer);

				// Now have all of the values, so compute the temperature and send to the LCD Task
				// Do the accurate temperature calculation
				temperature += -0.25 + ((countPerC-countRemain)/countPerC);

				#if PRINTF_VERSION == 1
				printf("Temp %f F (%f C)\n",(32.0 + ((9.0/5.0)*temperature)), (temperature));
				sprintf(lcdBuffer,"T=%6.2fF (%6.2fC)",(32.0 + ((9.0/5.0)*temperature)),temperature);
				#else
				// we do not have full printf (so no %f) and therefore need to print out integers
				printf("Temp %d F (%d C)\n",lrint(32.0 + ((9.0/5.0)*temperature)), lrint(temperature));
				sprintf(lcdBuffer,"T=%d F (%d C)",lrint(32.0 + ((9.0/5.0)*temperature)),lrint(temperature));
				#endif
				
				if (lcdData != NULL) {
					if (SendLCDPrintMsg(lcdData,strnlen(lcdBuffer,vtLCDMaxLen),lcdBuffer,portMAX_DELAY) != pdTRUE) {
						VT_HANDLE_FATAL_ERROR(0);
					}
				}
				
			} else {
				// unexpectedly received this message
				VT_HANDLE_FATAL_ERROR(0);
			}
			break;
		}  */
		default: {
			VT_HANDLE_FATAL_ERROR(getMsgType(&msgBuffer));
			break;
		}
		}


	}

   }
void moveForward(char* msg) {
   
	strcpy(message, msg);
	moveForwardFlag = 1;
	mapStruct.testVar = 0;
	
}

void moveStop(char* msg) {
	strcpy(message, msg);
	moveStopFlag = 1;
	mapStruct.testVar = 1;
}

void moveRight(char* msg) {
	strcpy(message, msg);
	moveRightFlag = 1;
	mapStruct.testVar = 0;
	
}

void moveLeft(char* msg) {
	strcpy(message, msg);
	moveLeftFlag = 1;
	mapStruct.testVar = 0;
}

void moveBack(char* msg) {
	strcpy(message, msg);
	moveBackFlag = 1;
}

void startGettingMotor(char* msg) {
	strcpy(message, msg);
	motorDataFlag = 1;
}

void stopGettingMotor(char* msg) {
 	strcpy(message, msg);
	motorDataFlag = 0;
}
void moveStart(char* msg) {
	strcpy(message, msg);
	moveStartFlag = 1;
	mapStruct.testVar = 0;
}
void moveMap(char* msg) {
	strcpy(message, msg);
	moveMapFlag = 1;
	mapStruct.testVar = 0;
}
void moveUse(char* msg) {
	strcpy(message, msg);
	moveUseFlag = 1;
	mapStruct.testVar = 0;
}
