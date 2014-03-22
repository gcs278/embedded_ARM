#ifndef MY_WEB_MAP_H
#define MY_WEB_MAP_H

// this stuct is to be use to add the mapp and also read the map so 
// it can be display on the webserver
struct myMapWebVars {
	int myMapArray [10][10];
	char *myOutputSting;
	//int testStruct;
	int testVar;
} mapStruct;

#endif