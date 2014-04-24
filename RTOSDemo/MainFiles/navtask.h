#ifndef NAV_TASK_H
#define NAV_TASK_H
#include "queue.h"
#include "timers.h"
#include "vtI2C.h"
#include "maptask.h"



// Define a data structure that is used to pass and hold parameters for this task
// Functions that use the API should not directly access this structure, but rather simply
//   pass the structure as an argument to the API calls
typedef struct __NavStruct {
	vtI2CStruct *dev; //sound like a good idea
	vtLCDStruct *lcdData;
	xQueueHandle inQ;
	myMapStruct *mapData;
} myNavStruct;

#define myNavMaxLen   10

// start the task
void myStartNavTask(myNavStruct *NavData, unsigned portBASE_TYPE uxPriority, vtI2CStruct *i2c, vtLCDStruct *lcd, myMapStruct *MapData);

portBASE_TYPE SendNavValueMsg(myNavStruct *NavData, uint8_t msgType, uint8_t *value, portTickType ticksToBlock);

uint8_t myCommandRover(uint8_t frontRight, uint8_t frontLeft, uint8_t sideFront, uint8_t sideBack, uint8_t lastCommand, uint8_t tickdata, uint8_t data );

#endif