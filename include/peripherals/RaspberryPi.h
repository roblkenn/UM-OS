#ifndef __RASPBERRY_PI_H_
#define __RASPBERRY_PI_H_

#define ARM_BUS_LOCATION		(unsigned int) 0x40000000
#define ARM_MAIL_BASE			(unsigned int) 0x2000B880
#define MAIL_FULL				0x80000000
#define MAIL_EMPTY				0x00000000
#define ARM_MAIL_READ			0x00
#define ARM_MAIL_WRITE			0x20
#define ARM_MAIL_STATUS			0x18

#define KERNEL_FRAME_BUFFER_LOC (volatile unsigned int) 0x00002000

struct GPU {
	unsigned int screenWidth;
	unsigned int screenHeight;
	unsigned int virtualWidth;
	unsigned int virtualHeight;
	unsigned int pitch;
	unsigned int depth;
	unsigned int xOffset;
	unsigned int yOffset;
	unsigned int framePtr;
	unsigned int bufferSize;
	short int valid;
};

void setGPIO(unsigned int pin, unsigned int state);

unsigned int mailboxCheck(char channel);
void mailboxWrite(char channel, unsigned int value);

struct GPU* acquireFrameBuffer(unsigned int xRes, unsigned int yRes);

#endif
