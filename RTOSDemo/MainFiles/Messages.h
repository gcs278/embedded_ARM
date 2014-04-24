#ifndef MESSAGES_H
#define MESSAGES_H

#define CleanMsg 0x00	// For clearing
#define BadMsg 0xFF 	// For when we receive a bad message

// Rover Debugging commands
#define RoverMsgMotorForward 0x01
#define RoverMsgMotorRight 0x02
#define RoverMsgMotorLeft 0x03
#define RoverMsgMotorBack 0x04
#define RoverMsgMotorStop 0x05

// Motor encoder distance requests
#define RoverMsgMotorLeftData 0x07
#define RoverMsgMotorRightData 0x08

// 

// Sensor data requests
#define RoverMsgSensorAllData 0x10
#define RoverMsgSensorRightForward 0x11
#define RoverMsgSensorRightRear 0x12
#define RoverMsgSensorForwardLeft 0x13
#define RoverMsgSensorForwardRight 0x14
#define RoverMsgSensorRightAverage 0x15

// Motor adjustment commands
#define RoverMsgMotorLeft2 0x1A
#define RoverMsgMotorLeft5 0x1B
#define RoverMsgMotorLeft7 0x1C
#define RoverMsgMotorLeft10 0x1D
#define RoverMsgMotorLeft15 0x1E
#define RoverMsgMotorLeft20 0x1F
#define RoverMsgMotorLeft25 0x20
#define RoverMsgMotorLeft30 0x21
#define RoverMsgMotorLeft45 0x22
#define RoverMsgMotorLeft75 0x23
#define RoverMsgMotorLeft90 0x24

#define RoverMsgMotorRight2 0x25
#define RoverMsgMotorRight5 0x26
#define RoverMsgMotorRight7 0x27
#define RoverMsgMotorRight10 0x28
#define RoverMsgMotorRight15 0x29
#define RoverMsgMotorRight90 0x2F

#define RoverMsgTurnOnWallTracking 0x0E
#define RoverMsgTurnOffWallTracking 0x0F

// Speed Modes
#define RoverMsgMotorSpeedCreepin 0x2A
#define RoverMsgMotorSpeedSlow 0x2B
#define RoverMsgMotorSpeedMedium 0x2C
#define RoverMsgMotorSpeedMediumFast 0x2D
#define RoverMsgMotorSpeedFastBRAH 0x2E

#define RoverMsgMotorForwardCMDelim 0x30

	int messageCount = 0;

#endif