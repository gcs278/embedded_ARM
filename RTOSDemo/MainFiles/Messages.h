#ifndef MESSAGES_H
#define MESSAGES_H

// Rover Debugging commands
#define RoverMsgMotorForward 0x01
#define RoverMsgMotorRight 0x02
#define RoverMsgMotorLeft 0x03
#define RoverMsgMotorBack 0x04
#define RoverMsgMotorStop 0x05

// Motor encoder distance requests
#define RoverMsgMotorLeftData 0x07
#define RoverMsgMotorRightData 0x08

// Sensor data requests
#define RoverMsgSensorAllData 0x11

// Motor adjustment commands
#define RoverMsgMotorLeft2 0x15
#define RoverMsgMotorLeft5 0x16
#define RoverMsgMotorLeft7 0x17
#define RoverMsgMotorLeft10 0x18
#define RoverMsgMotorLeft15 0x19
#define RoverMsgMotorLeft20 0x1A
#define RoverMsgMotorLeft25 0x1B
#define RoverMsgMotorLeft30 0x1C
#define RoverMsgMotorLeft45 0x1D
#define RoverMsgMotorLeft75 0x1E
#define RoverMsgMotorLeft90 0x1F

#define RoverMsgMotorRight2 0x20
#define RoverMsgMotorRight5 0x21
#define RoverMsgMotorRight7 0x22
#define RoverMsgMotorRight10 0x23
#define RoverMsgMotorRight15 0x24
#define RoverMsgMotorRight20 0x25
#define RoverMsgMotorRight25 0x26
#define RoverMsgMotorRight30 0x27
#define RoverMsgMotorRight45 0x28
#define RoverMsgMotorRight75 0x29
#define RoverMsgMotorRight90 0x2A

#endif