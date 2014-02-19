#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "projdefs.h"
#include "semphr.h"

#include "my_WebTask.h"

//static webInput_t webInput;

static portTASK_FUNCTION_PROTO( webTask, pvParameters );

void startWebTask(webStruct *ptr, unsigned portBASE_TYPE uxPriority)
{
	if (ptr == NULL) {						   
		VT_HANDLE_FATAL_ERROR(0);
	}
	// Create the queue that will be used to talk to this task
	if ((ptr->inQ = xQueueCreate(10,sizeof(WebMessage))) == NULL) {
		VT_HANDLE_FATAL_ERROR(0);
	}
	/* Start the task */
	portBASE_TYPE retval;
	if ((retval = xTaskCreate( webTask, ( signed char * ) "WEB", configMINIMAL_STACK_SIZE*5, (void*)ptr, uxPriority, ( xTaskHandle * ) NULL )) != pdPASS) {
		VT_HANDLE_FATAL_ERROR(retval);
	}
}

static portTASK_FUNCTION( webTask, pvParameters )
{
	// Get the parameters
	webStruct *param = (webStruct *) pvParameters;

	WebMessage msgBuffer;

	for(;;)
	{
		if (xQueueReceive(param->inQ,(void *) &msgBuffer,portMAX_DELAY) != pdTRUE) {
			VT_HANDLE_FATAL_ERROR(0);
		}

		// DO web stuff here
		switch (msgBuffer.msgType){
				
			default:
				break;
		}


	}
}																	 

portBASE_TYPE sendWebMessage(webStruct* web,WebMessage* msg,portTickType ticksToBlock) {
	// Check if web is null
	if (msg == NULL || web == NULL ) {
		return pdFALSE;
	}

	return(xQueueSend(web->inQ,(void*)(msg),ticksToBlock));
}

//void sendWebSubmit(webInput_t* in){

//}