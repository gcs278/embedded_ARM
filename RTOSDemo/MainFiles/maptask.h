#include "queue.h"
#include "timers.h"

#ifndef MAP_TASK_H
#define MAP_TASK_H

#define myMapMaxLen   (sizeof(portTickType))

// Define a data structure that is used to pass and hold parameters for this task
// Functions that use the API should not directly access this structure, but rather simply
//   pass the structure as an argument to the API calls
typedef struct __myMapStruct {
	vtI2CStruct *dev; //sound like a good idea
	xQueueHandle inQ;
} myMapStruct;

// start the task
void myStartMapTask(myMapStruct *Data, unsigned portBASE_TYPE uxPriority, vtI2CStruct *i2c);

portBASE_TYPE SendMapValueMsg(myMapStruct *Data, uint8_t msgType, uint8_t* value, portTickType ticksToBlock);


#endif