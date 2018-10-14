#ifndef     _BOOT_H
#define     _BOOT_H

/*
*Delay given amount of cycles (not time)
*/
extern void delay(unsigned long);

extern void put32(unsigned long, unsigned int);
extern unsigned int get32(unsigned long);

/*
*Returns the current process' exception level
*Should be 1 for OS level, 0 for Application level
*/
extern int getEL();

#endif
