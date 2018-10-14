#ifndef     _MINIUART_H
#define     _MINIUART_H

void uartInit(void);
char uartRecv(void);
void uartSend(char c);
void uartSendString(char* str);
void putc(void* p, char c);

#endif      
