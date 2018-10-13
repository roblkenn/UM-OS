#include "MiniUart.h"

void kernelMain(void) {
    uartInit();

    uartSendString("Hello, Kernel!\r\n");

    while (1) {
        uartSend(uartRecv());
    }
}