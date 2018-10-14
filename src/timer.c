#include "utils.h"
#include "printf.h"
#include "peripherals/timer.h"

const unsigned int interval = 200000;
unsigned int curVal = 0;

void timerInit(void){
	curVal = get32(TIMER_CL0);
	curVal += interval;
	put32(TIMER_C1, curVal);
}

void handleTimerIrq(void){
	curVal += interval;
	put32(TIMER_C1, curVal);
	put32(TIMER_C1, TIMER_CS_M1);
	printf("Timer interrupt recieved\r\n");
}
