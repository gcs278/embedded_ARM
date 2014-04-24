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
#include "navtask.h"
#include "conductor.h"
#include "mywebmap.h"
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

void myStartNavTask(myNavStruct *NavData, unsigned portBASE_TYPE uxPriority, vtI2CStruct *i2c,  vtLCDStruct *lcd, myMapStruct *MapData)
{
	mapStruct.SEMForSensors = NULL;
	// create the semaphore
	mapStruct.SEMForSensors = xSemaphoreCreateMutex();
 	if ((NavData->inQ = xQueueCreate(myNavQLen,sizeof(myNavMsg))) == NULL) {
		VT_HANDLE_FATAL_ERROR(0);
	}
/* Start the task */
	portBASE_TYPE retval;
	NavData->dev = i2c;
	NavData->lcdData = lcd;
	NavData->mapData = MapData;
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
	// Get the LCD information pointer
	vtLCDStruct *lcdData = param->lcdData;
	// Get the sensor data
	myNavMsg msgBuffer;
	//flag
	uint8_t startNav = 0;

	mapStruct.sensor1 = 0;
	mapStruct.sensor2 = 0;
	mapStruct.sensor3 = 0;
	mapStruct.sensor4 = 0;

	int lastDistance = 0; // The last distance calculated

	int lastSideDistance = 0;		  

	// commands for the function
	uint8_t lastCommand = 0;
	uint8_t currentCommand = 0;	

	int extender = 0; // For extending the time between right turns
	int moveExtend = 0;
	unsigned char messageCount = 0;

	uint8_t Buffer_map[vtI2CMLen] = {0,0,0,0,0,0,0,0,0,0};

	uint8_t i2cRoverStop[] = {0x05, 0x00};
	uint8_t i2cRoverMoveR90[] = {0x2f, 0x33};
	//uint8_t i2cBrightRed[] = {'n', 0x00,0xff,0x00}; 
	char str[20];
	uint8_t i2cRoverMoveForward[] = {RoverMsgMotorForward, 0x00};
    uint8_t i2cRoverMoveForward50[] = {0xC7, 0x00};
	uint8_t i2cRoverMove90[] = {RoverMsgMotorLeft90, 0x33};
	uint8_t i2cRoverMoveLeft2[] = {RoverMsgMotorLeft2, 0x00};
	uint8_t i2cRoverMoveRight2[] = {RoverMsgMotorRight2, 0x00};

	myMapStruct *mapData = param->mapData;
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
			//printf(" navStart\n");
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
		case 0x90:

			insertCountDef(RoverMsgMotorRight2);
			i2cRoverMoveRight2[1] = getMsgCount();
		  	if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveRight2),i2cRoverMoveRight2,10) != pdTRUE) {
				VT_HANDLE_FATAL_ERROR(0);
			}
			incrementMsgCount();
			break;
		case RoverMsgMotorLeftData:
			
			//printf("MotorMessageNav:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",msgBuffer.buf[0],msgBuffer.buf[1],msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5],msgBuffer.buf[6],msgBuffer.buf[7],msgBuffer.buf[8],msgBuffer.buf[9]);
			if (msgBuffer.buf[1] != 0 ) {
				currentCommand = myCommandRover(50, 50, 30, 30, lastCommand, msgBuffer.buf[2], 0);
				printf(" This is currentCommand :D %d\n",currentCommand);
			}
			
			GPIO_ClearValue(0,0x78000);
			GPIO_SetValue(0, 0x60000);
			if(currentCommand != lastCommand) {
			GPIO_ClearValue(0,0x78000);
			GPIO_SetValue(0, 0x68000);
			if(currentCommand == 1 || currentCommand == 6 || currentCommand == 11){
						sprintf(str,"G%d",msgBuffer.buf[2]);
						//sprintf(str,"testlol");
		    			// Print something on LCD
						
						if (SendLCDPrintMsg(lcdData,strnlen(str,vtLCDMaxLen),str,portMAX_DELAY) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}
						if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveForward),i2cRoverMoveForward,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
						}
						/*if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(RoverMsgTurnOnWallTracking),RoverMsgTurnOnWallTracking,10) != pdTRUE) {
							printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
							VT_HANDLE_FATAL_ERROR(0);
						}*/	
					}
					// Stop Command
					else if(currentCommand == 3 || currentCommand == 5 || currentCommand == 0|| currentCommand == 8 || currentCommand == 10 || currentCommand == 13){
						sprintf(str,"S%d",msgBuffer.buf[2]);
						//sprintf(str,"testlol");
		    			// Print something on LCD
						if (SendLCDPrintMsg(lcdData,strnlen(str,vtLCDMaxLen),str,portMAX_DELAY) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}
						/*if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(RoverMsgTurnOffWallTracking),RoverMsgTurnOffWallTracking,10) != pdTRUE) {
							printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
							VT_HANDLE_FATAL_ERROR(0);
						}*/
						if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverStop),i2cRoverStop,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
						}
					}
					// Rigth Command
					else if(currentCommand == 9 || currentCommand == 14) {
						sprintf(str,"R%d",msgBuffer.buf[2]);
						//sprintf(str,"testlol");
		    			// Print something on LCD
						if (SendLCDPrintMsg(lcdData,strnlen(str,vtLCDMaxLen),str,portMAX_DELAY) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}
						printf("Sending right command!\n");
						SendMapValueMsg(mapData,RoverMsgMotorRight90, Buffer_map, portMAX_DELAY);
						if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveR90),i2cRoverMoveR90,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
						}
					}
					else if( currentCommand == 4) {
						sprintf(str,"L%d",msgBuffer.buf[2]);
						//sprintf(str,"testlol");
		    			// Print something on LCD
						if (SendLCDPrintMsg(lcdData,strnlen(str,vtLCDMaxLen),str,portMAX_DELAY) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}
						printf("Sending left command!\n");
						SendMapValueMsg(mapData,RoverMsgMotorLeft90, Buffer_map, portMAX_DELAY);
						if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMove90),i2cRoverMove90,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
						}
					}
					else {
					//do nothing
					}
					}
					lastCommand = currentCommand;
			//printf(" This is currentCommand :D %d\n",currentCommand);
			break;		 
		case 0x11:
			//printf("NavMessage:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",msgBuffer.buf[0],msgBuffer.buf[1],msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5],msgBuffer.buf[6],msgBuffer.buf[7],msgBuffer.buf[8],msgBuffer.buf[9]);
			//printf("Extender: %d\n",extender);
			GPIO_ClearValue(0,0x78000);
			GPIO_SetValue(0, 0x50000);

			if(mapStruct.SEMForSensors != NULL)
			{
				if( xSemaphoreTake( mapStruct.SEMForSensors , 1 ) == pdPASS ) {
					mapStruct.sensor1 = msgBuffer.buf[2];
					mapStruct.sensor2 = msgBuffer.buf[3];
					mapStruct.sensor3 = msgBuffer.buf[4];
					mapStruct.sensor4 = msgBuffer.buf[5];
					//printf("take\n");
						if(	xSemaphoreGive( mapStruct.SEMForSensors ) == pdFALSE )
						{
							printf("YOU DONE FUCKED UP A-AARON");
						}
					}
					//printf("Give\n");
			}

			extender--;
			if ( extender < 0 ) {
				if (msgBuffer.buf[1] != 0 ) {
					currentCommand = myCommandRover(msgBuffer.buf[2], msgBuffer.buf[3] ,msgBuffer.buf[4], msgBuffer.buf[5], currentCommand, 55, 1);
					printf(" This is currentCommand :D %d\n",currentCommand);
				}
				//currentCommand = myCommandRover(msgBuffer.buf[2], msgBuffer.buf[3] ,msgBuffer.buf[4], msgBuffer.buf[5], currentCommand, 55, 1);
				

				if(currentCommand != lastCommand) {
					GPIO_ClearValue(0,0x78000);
					GPIO_SetValue(0, 0x58000);
					// Move Command
					if(currentCommand == 1 || currentCommand == 6 || currentCommand == 11){
						sprintf(str,"G%d,%d,%d,%d",msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5]);
						//sprintf(str,"testlol");

		    			// Print something on LCD
						if (SendLCDPrintMsg(lcdData,strnlen(str,vtLCDMaxLen),str,portMAX_DELAY) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}
						
						insertCountDef(i2cRoverMoveForward[0]);
						i2cRoverMoveForward[1] = getMsgCount();
						if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveForward),i2cRoverMoveForward,10) != pdTRUE) {
							printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
							VT_HANDLE_FATAL_ERROR(0);
						}
						/*if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(RoverMsgTurnOnWallTracking),RoverMsgTurnOnWallTracking,10) != pdTRUE) {
							printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
							VT_HANDLE_FATAL_ERROR(0);
						}*/
						incrementMsgCount();	
					}
					// Stop Command
					else if(currentCommand == 3 || currentCommand == 5 || currentCommand == 0|| currentCommand == 8 || currentCommand == 10 || currentCommand == 13){
						sprintf(str,"S%d,%d,%d,%d",msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5]);
						//sprintf(str,"testlol");
		    			// Print something on LCD
						/*if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(RoverMsgTurnOffWallTracking),RoverMsgTurnOffWallTracking,10) != pdTRUE) {
							printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
							VT_HANDLE_FATAL_ERROR(0);
						}*/
						if (SendLCDPrintMsg(lcdData,strnlen(str,vtLCDMaxLen),str,portMAX_DELAY) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}
						
						insertCountDef(i2cRoverStop[0]);
						i2cRoverStop[1] = getMsgCount();
						if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverStop),i2cRoverStop,10) != pdTRUE) {
							printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
							VT_HANDLE_FATAL_ERROR(0);
						}
						incrementMsgCount();
					}

					// Rigth Command
					else if(currentCommand == 9 || currentCommand == 14) {
						sprintf(str,"R%d,%d,%d,%d",msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5]);
						//sprintf(str,"testlol");
		    			// Print something on LCD
						if (SendLCDPrintMsg(lcdData,strnlen(str,vtLCDMaxLen),str,portMAX_DELAY) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}

						insertCountDef(i2cRoverMoveR90[0]);
						i2cRoverMoveR90[1] = getMsgCount();
						if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveR90),i2cRoverMoveR90,10) != pdTRUE) {
							printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
							VT_HANDLE_FATAL_ERROR(0);
						}
						incrementMsgCount();
					}
					else if( currentCommand == 4) {
						sprintf(str,"L%d,%d,%d,%d",msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5]);
						//sprintf(str,"testlol");
		    			// Print something on LCD
						if (SendLCDPrintMsg(lcdData,strnlen(str,vtLCDMaxLen),str,portMAX_DELAY) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}

						insertCountDef(i2cRoverMove90[0]);
						i2cRoverMove90[1] = getMsgCount();
						if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMove90),i2cRoverMove90,10) != pdTRUE) {
							printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
							VT_HANDLE_FATAL_ERROR(0);
						}
						incrementMsgCount();
					}
					else {
					//do nothing
					}

				} else {
				//do tnoghint
				}
				lastCommand = currentCommand; 			
			//if(startNav == 1) {
				/*int i;
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
				//printf("FrontDistance: %d\n", frontDistance);

			/*	if ( moveExtend == 1 ) {
					incrementMsgCount();
					insertCountDef(0xC7);
					i2cRoverMoveForward50[1] = getMsgCount();
					printf("MessageCount: %d\n", getMsgCount());
					if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveForward50),i2cRoverMoveForward50,10) != pdTRUE) {
						VT_HANDLE_FATAL_ERROR(0);
					} 	 
					incrementMsgCount();
					moveExtend=0;
				}
			 if (frontDistance < 60 && lastDistance < 60 && frontDistance >1 && lastDistance > 1) {
					printf("----sent90\n"); 

					insertCountDef(RoverMsgMotorLeft90);
					i2cRoverMove90[1] = getMsgCount();
					printf("MessageCount: %d\n", getMsgCount());   */
					/*if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x09,sizeof(i2cBrightRed),i2cBrightRed,0) != pdTRUE) {
							VT_HANDLE_FATAL_ERROR(0);
						}*/	 
				/*	if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMove90),i2cRoverMove90,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
					} 	 
					incrementMsgCount();
					extender = 15;
				}	 */
				//else {
					//printf("----sentMove\n");
					/*if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveForward),i2cRoverMoveForward,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
					} */
				//}
				//lastDistance = distance;
		//	}
		}
			break;

/*----------------- GRANT's OLD CODE FOR PRELIM ------------------------------------------------------------
		case RoverMsgSensorRightForward: {
			printf("NavMessage:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",msgBuffer.buf[0],msgBuffer.buf[1],msgBuffer.buf[2],msgBuffer.buf[3],msgBuffer.buf[4],msgBuffer.buf[5],msgBuffer.buf[6],msgBuffer.buf[7],msgBuffer.buf[8],msgBuffer.buf[9]);
			printf("Extender: %d\n",extender);
			extender--;
			if ( extender < 0 ) {
			//if(startNav == 1) {
				int i;
				int sideDistance = 0;
				for (i = 0; i < msgBuffer.buf[1] % 10; i++) {
					if ( i < 8)
					sideDistance += msgBuffer.buf[i+2];
				}
				sideDistance = sideDistance / (i+1);
				printf("sideDistance: %d\n", sideDistance);
				if (sideDistance < 40 && lastSideDistance < 40 && sideDistance >30 && lastSideDistance > 30) {
					printf("----slight turn\n"); 

					insertCountDef(RoverMsgMotorRight2);
					i2cRoverMsgMotorRight2[1] = getMsgCount();
					printf("MessageCount: %d\n", getMsgCount());
					if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMsgMotorRight2),i2cRoverMsgMotorRight2,10) != pdTRUE) {
						VT_HANDLE_FATAL_ERROR(0);
					} 	 
					extender = 15;
					moveExtend=1 ;
				}
				else {
					//printf("----sentMove\n");
					/*if (vtI2CEnQ(devPtr,vtI2CMsgTypeTempRead1,0x4F,sizeof(i2cRoverMoveForward),i2cRoverMoveForward,10) != pdTRUE) {
						printf("GODDAMNIT MOTHER FUCKING PIECE OF SHIT");
						VT_HANDLE_FATAL_ERROR(0);
					} */
			//	}
			//	lastDistance = frontDistance;
		//	}
	//	}
	//	break;
//		}

		default: {
			printf("  navDefault\n");
			
			//VT_HANDLE_FATAL_ERROR(getMsgType(&msgBuffer));
			break;
		}
		}
		

	}
}

// my rover move command
uint8_t myCommandRover( uint8_t frontRight, uint8_t frontLeft, uint8_t sideFront, uint8_t sideBack, uint8_t lastCommand, uint8_t tickdata, uint8_t data) {
	float percentErrorFront = frontLeft - frontRight;
	percentErrorFront = percentErrorFront / frontLeft;
	float percentErrorSide = sideBack - sideFront;
	percentErrorSide = percentErrorSide / sideBack;
	unsigned int avgFront = frontRight + frontLeft;
	avgFront = avgFront / 2;
	unsigned int avgSide = sideBack + sideFront;
	avgSide = avgSide / 2;
	printf("INSIDE %d,%d,%d,%d\n",frontRight,frontLeft,sideFront,sideBack);
	printf("AVG FRONT %d\n",avgFront);
	printf("AVG SIDE %d\n",avgSide);
	//if( percentErrorFront <= .02 && percentErrorFront >= -.02 && percentErrorSide <= .02 && percentErrorSide >= -.02){
		if (lastCommand == 0 && tickdata == 0) { // off
			return 1;
		}

		else if (lastCommand == 1) {// on
			return 2;
		}
		//off
		else if(lastCommand == 2 && avgFront <= 70 && avgFront >= 1 && data == 1 && percentErrorFront <= .02 && percentErrorFront >= -.02) {
			return 3;
		}
		//off
		else if (lastCommand == 3 && tickdata == 0) {
			return 4;
		}
		//off
		else if (lastCommand == 4 && tickdata == 0 ) {
			return 5;
		}
		//off
		else if (lastCommand == 5 && tickdata == 0) {
			return 6;
		}
		//off
		else if (lastCommand == 6) {
			return 7;
		}
		//on
		else if (lastCommand == 7 && avgFront <= 70 && avgFront >= 1 && data == 1 && percentErrorFront <= .02 && percentErrorFront >= -.02) {
			return 3;
		}
		//off
		else if (lastCommand == 7 && avgSide >=100 && avgFront >=100 && data == 1 && percentErrorFront <= .02 && percentErrorFront >= -.02 && percentErrorSide <= .02 && percentErrorSide >= -.02) {
			return 8;
		}
		else if(lastCommand == 8 && tickdata == 0) {
			return 9;
		}
		else if (lastCommand == 9 && tickdata == 0 ) {
			return 10;
		}
		else if (lastCommand == 10 && tickdata == 0) {
			return 11;
		}
		else if (lastCommand == 11) {
			return 12;
		}

		else if (lastCommand == 12 && avgSide <= 50 && data == 1 && percentErrorSide <= .02 && percentErrorSide >= -.02) {
			return 15;
		}
		else if (lastCommand == 15 && avgSide >= 100 && data == 1 && percentErrorSide <= .02 && percentErrorSide >= -.02) {
			return 13;
		}
		else if (lastCommand == 13 && tickdata == 0){
			return 14;
		}
		else if(lastCommand == 14 && tickdata == 0) {
			return 0;
		}
		else {
			return lastCommand;
		}
	//}
	/*else {
		return lastCommand;
	} */

}
