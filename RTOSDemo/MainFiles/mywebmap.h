#ifndef MY_WEB_MAP_H
#define MY_WEB_MAP_H


//#include <RTL.h>                      /* RTX kernel functions & defines      */
//#include <LPC21xx.H>                  /* LPC21xx definitions                 */
#include <stdio.h>
#include <LPC17xx.H>
#include <stdlib.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// this stuct is to be use to add the mapp and also read the map so 
// it can be display on the webserver
struct myMapWebVars {
	int myMapArray [10][10];
	char *myOutputSting;
	int totalDistanceTraveled;
	int sensor1;
	int sensor2;
	int sensor3;
	int sensor4;
	int deciSeconds;
	int seconds;
	int min;
	int timerFlag;
	int mappingFlag;
	xSemaphoreHandle SEMForTotalDistance;
	xSemaphoreHandle SEMForSensors;

	//int testStruct;
	int testVar;
} mapStruct;

typedef struct wallStruct{
	int length;
	int direction;
} wall;

extern wall web_walls[50];
int num_walls;

void update_walls(wall map_walls[50], int map_num)
{
	int i;
	for (i = 0; i < map_num; i++)
	{
		web_walls[i] = map_walls[i];
	}
	num_walls = map_num;
}


#endif
