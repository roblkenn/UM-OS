#include "MiniUart.h"
#include "utils.h"

void kernelMain(void) {
    uartInit();

    uartSendString("Current Exception level: ");
    int el = getEL();
    uartSend('0'+el);
    uartSendString("\r\n");

    uartSendString("Managed to avoid drowning!\r\n");

    while (1) {
        uartSend(uartRecv());
    }
}
