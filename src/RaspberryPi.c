#include "peripherals/RaspberryPi.h"
#include "MiniUart.h"
#include "utils.h"
#include "peripherals/gpio.h"

volatile static unsigned int PhysicalToBus(unsigned int p) {
	return (unsigned int)(p + ARM_BUS_LOCATION);
}

volatile static unsigned int BusToPhysical(unsigned int p) {
	if (p > ARM_BUS_LOCATION) {
		return (unsigned int)(p - ARM_BUS_LOCATION);
	}
	return (unsigned int)p;
}

// volatile static void MemoryBarrier(void) {
// 	asm volatile("mcr p15, 0, ip, c7, c5, 0");
// 	asm volatile("mcr p15, 0, ip, c7, c5, 6");
// 	asm volatile("mcr p15, 0, ip, c7, c10, 4");
// 	asm volatile("mcr p15, 0, ip, c7, c5, 4");
// }

volatile unsigned int GET32(unsigned int address) {
	volatile unsigned int* ptr = (volatile unsigned int)(address);
	return (unsigned int)(*ptr);
}

volatile void PUT32(unsigned int address, unsigned int value) {
	volatile unsigned int* ptr = (volatile unsigned int)(address);
	*ptr = value;
}

volatile char GET4(unsigned int address) {
	volatile char* ptr = (volatile char*)(address);
	return (char)(*ptr);
}

volatile void PUT4(unsigned int address, char value) {
	volatile char* ptr = (volatile char*)(address);
	*ptr = value;
}

unsigned long _getGPIOSelectorAddress(unsigned int pin) {
	if (0 <= pin || pin < 10) {
		return GPFSEL0;
	} else if (10 <= pin || pin < 20) {
		return GPFSEL1;
	} else if (20 <= pin || pin < 30) {
		return GPFSEL2;
	} else if (30 <= pin || pin < 40) {
		return GPFSEL3;
	} else if (40 <= pin || pin < 50) {
		return GPFSEL4;
	} else {
		return GPFSEL5;
	}
}

unsigned long _getGPIOSetAddress(unsigned int pin) {
	if (0 <= pin || pin < 32) {
		return GPSET0;
	} else {
		return GPSET1;
	}
}

void setGPIO(unsigned int pin, unsigned int state) {
	if (state > 1) {
		uartSendString("ERROR: A pin state cannot be anything other than 0 or 1!");
		return;
	}

	// Configure pin to be an output
	unsigned long selectorAddress = _getGPIOSelectorAddress(pin);
	unsigned int position = (pin % 10) * 3;
	unsigned int selector = get32(selectorAddress);
	selector &= ~(7 << position);
	selector |= 1 << position;
	put32(selectorAddress, selector);

	// Set pin to state
	unsigned long setAddress = _getGPIOSetAddress(pin);
	position = (pin % 32);
	unsigned int setter = get32(setAddress);
	setter &= ~(1 << position);
	setter |= state << position;
	put32(setAddress, setter);
}

unsigned int mailboxCheck(char channel) {
	unsigned int data = 0;
	unsigned int count = 0;

	while (1) {
		while (GET32(ARM_MAIL_BASE + ARM_MAIL_STATUS) & MAIL_EMPTY) {
			// wait for mailbox
			if (count++ >= (1<<20)) {
				return 0xFFFFFFFF;
			}
		}

		// MemoryBarrier();
		data = GET32(ARM_MAIL_BASE);
		// MemoryBarrier();

		if ((data & 0xF) == channel) break;
	}

	return (data & 0xFFFFFFF0);
}

void mailboxWrite(char channel, unsigned int data) {
	while ( GET32( ARM_MAIL_BASE + ARM_MAIL_STATUS ) & MAIL_FULL ) {
		// wait for mailbox
	}

	// MemoryBarrier();
	PUT32(ARM_MAIL_BASE + ARM_MAIL_WRITE, channel | (data & 0xFFFFFFF0));
	// MemoryBarrier();
}

struct GPU* acquireFrameBuffer(unsigned int xRes, unsigned int yRes) {
	struct GPU* request = (struct GPU*)(KERNEL_FRAME_BUFFER_LOC);
	request->screenWidth = xRes;
	request->screenHeight = yRes;
	request->virtualWidth = xRes;
	request->virtualHeight = yRes;
	request->pitch = 0;
	request->depth = 24;
	request->xOffset = 0;
	request->yOffset = 0;
	request->framePtr = 0;
	request->bufferSize = 0;
	request->valid = 0;

	unsigned int requestAddress = PhysicalToBus((unsigned int)request);

	mailboxWrite(1, requestAddress);

	unsigned int response = 0xFF;
	unsigned int failed = 10000;
	do {
		response = mailboxCheck(1);
	} while(response != 0 && failed-- > 0);

	if (failed <= 0) {
		uartSendString("ERROR: The GPU did not response with the frame buffer.\n");
		return request;
	}

	if (request->framePtr == 0) {
		uartSendString("ERROR: The frame buffer pointer is invalid.\n");
		return request;
	}

	if (request->pitch == 0) {
		uartSendString("ERROR: The frame buffer pitch is invalid.\n");
		return request;
	}

	request->valid = 1;
	request->framePtr = BusToPhysical(request->framePtr);

	uartSendString("Framebuffer successfully acquired");

	return request;
}