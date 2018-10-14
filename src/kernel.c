#include "MiniUart.h"
#include "utils.h"
#include "peripherals/RaspberryPi.h"
#include "irq.h"
#include "timer.h"
#include "printf.h"

void kernelMain(void) {
	uartInit();
	init_printf(0, putc);

	uartSendString("Current Exception level: ");
	int el = getEL();
	uartSend('0'+el);
	uartSendString("\r\n");

	//interupt registration
	irqVectorInit();
	timerInit();
	enableInterruptController();
	enableIrq();
	printf("hello");

	uartSendString("Managed to avoid drowning!\r\n");
	acquireFrameBuffer(800, 480);

    while (1) {
        uartSend(uartRecv());
	}
}
