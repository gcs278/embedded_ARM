#ifndef __WEBTASK_H__
#define __WEBTASK_H__

#include <stdarg.h>
#include "queue.h"
#include "timers.h"
#include "vtUtilities.h"
#include "vtI2C.h"

// Message for sending to WebTask
typedef struct __WebMessage {
	uint8_t msgType;
	uint8_t length;
	uint8_t buf[20];
} WebMessage;

typedef struct __webStruct {
	xQueueHandle inQ;
} webStruct;

void startWebTask(webStruct* webData,unsigned portBASE_TYPE uxPriority);

portBASE_TYPE sendWebMessage(webStruct* web,WebMessage* msg,portTickType ticksToBlock);

#endif