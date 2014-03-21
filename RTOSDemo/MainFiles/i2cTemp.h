#ifndef I2CTEMP_TASK_H
#define I2CTEMP_TASK_H
#include "vtI2C.h"
#include "lcdTask.h"
// Structure used to pass parameters to the task
// Do not touch...
typedef struct __TempStruct {
	vtI2CStruct *dev;
	vtLCDStruct *lcdData;
	xQueueHandle inQ;
} vtTempStruct;

typedef struct __RoverCommStruct {
	vtI2CStruct* i2cStruct;
} RoverCommStruct;

// Maximum length of a message that can be received by this task
#define vtTempMaxLen   10//(sizeof(portTickType))

// Public API
//
// Start the task
// Args:
//   tempData: Data structure used by the task
//   uxPriority -- the priority you want this task to be run at
//   i2c: pointer to the data structure for an i2c task
//   lcd: pointer to the data structure for an LCD task (may be NULL)
void vStarti2cTempTask(vtTempStruct *tempData,unsigned portBASE_TYPE uxPriority, vtI2CStruct *i2c,vtLCDStruct *lcd);
//
// Send a timer message to the Temperature task
// Args:
//   tempData -- a pointer to a variable of type vtLCDStruct
//   ticksElapsed -- number of ticks since the last message (this will be sent in the message)
//   ticksToBlock -- how long the routine should wait if the queue is full
// Return:
//   Result of the call to xQueueSend()
portBASE_TYPE SendTempTimerMsg(vtTempStruct *tempData,portTickType ticksElapsed,portTickType ticksToBlock);
//
// Send a value message to the Temperature task
// Args:
//   tempData -- a pointer to a variable of type vtLCDStruct
//   msgType -- the type of the message to send
//   value -- The value to send
//   ticksToBlock -- how long the routine should wait if the queue is full
// Return:
//   Result of the call to xQueueSend()				
portBASE_TYPE SendTempValueMsg(vtTempStruct *tempData,uint8_t msgType,uint8_t* values,portTickType ticksToBlock);

void moveForward(char* msg);
void moveStop(char* msg);
void moveLeft(char* msg);
void moveRight(char* msg);
void startGettingMotor(char* msg);
void stopGettingMotor(char* msg);
void moveBack(char* msg);
void moveUse(char* msg);
void moveMap(char* msg);
void moveStart(char* msg);

#endif