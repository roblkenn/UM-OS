#ifndef	_IRQ_H
#define	_IRQ_H

void enableInterruptController(void);

void irqVectorInit(void);
void enableIrq(void);
void disableIrq(void);

#endif
