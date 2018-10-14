#include "MiniUart.h"
#include "utils.h"
#include "peripherals/RaspberryPi.h"

void kernelMain(void) {
	uartInit();

    uartSendString("Current Exception level: ");
    int el = getEL();
    uartSend('0'+el);
    uartSendString("\r\n");

    uartSendString("Managed to avoid drowning!\r\n");

	acquireFrameBuffer(800, 480);

    while (1) {
        uartSend(uartRecv());
    }
}
