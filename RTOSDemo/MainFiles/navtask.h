#ifndef NAV_TASK_H
#define NAV_TASK_H
#include "queue.h"
#include "timers.h"
#include "vtI2C.h"



// Define a data structure that is used to pass and hold parameters for this task
// Functions that use the API should not directly access this structure, but rather simply
//   pass the structure as an argument to the API calls
typedef struct __NavStruct {
	vtI2CStruct *dev; //sound like a good idea
	xQueueHandle inQ;
} myNavStruct;

#define myNavMaxLen   (sizeof(portTickType))

// start the task
void myStartNavTask(myNavStruct *NavData, unsigned portBASE_TYPE uxPriority, vtI2CStruct *i2c);

portBASE_TYPE SendNavValueMsg(myNavStruct *NavData, uint8_t msgType, uint8_t value, portTickType ticksToBlock);

//unint8_t myCommandRover();

#endif