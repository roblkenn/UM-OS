.globl irqVectorInit
irqVectorInit:
	adr x0, vectors		//load VBAR_EL1 with virtual
	msr vbar_el1, x0	//vector table address
	ret

.globl enableIrq
enableIrq:
	msr	daifclr, #2
	ret

.global disableIrq
disableIrq:
	msr	daifset, #2
	ret
