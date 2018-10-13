# IPC's
> **Interprocess Communication:** Most interprocess communication can be handled with system calls -- one process writes data, while the other reads it sometime later. A kernel upcall is needed if a process generates an event that needs the instant attention of another process. As an example, UNIX sends an upcall to notify a process when the debugger wants to suspend or resume the process. Another use is for logout -- to notify applications that they should save file data and cleanly terminate. (OSPaP, 80)
> See also: (OSPaP, 113-115)

## IPC System Call Requirements
We need a socket-esque implementation for mailboxes. Enforce all sockets as 1 to 1? Or do we want to figure out how to do client/server sockets? I'm proceeding under 1 to 1 assumptions but we can adapt otherwise.
```C
/* We need to write Socket.h/c implemented with the following */
/* SocketBuffers use a rolling index to speed up queries. Instead 
of reading the first byte and shifting the whole array down, we 
read from dataStart++. We know the array is full when 
	dataStart = (dataEnd - 1) % SOCKET_BUFFER_SIZE. 
Let me know if I should document what my intentions are more */
struct SocketBuffer {
	char[SOCKET_BUFFER_SIZE] buffer;
	int dataStart = 0;
	int dataEnd = 0;
};

struct Socket {
	int portNumber;
	struct SocketBuffer writeBuffer;
	struct SocketBuffer readBuffer;
	pid_t creator, connector;
};
```

### Client Socket Interface
```C
// Create a socket set up for shared memory at given portNumber
// Return socket's portNumber is 0 if failed? Or throw exception?
// Pass portNumber = 0 to generate a random, unoccupied port
struct Socket CreateSocket(int portNumber)

// Connect to an existing socket (reverses which buffer is which, essentially)
// Same rules for portNumber except passing 0 shouldn't generate a random connection... that would be bad
struct Socket ConnectSocket(int portNumber)

// Close one end of a socket, when both creator and connector close, free up the Socket
// Right now it returns bool, but it should probably be void and just throw an exception if something happens?
// (Like the client trying to close a port they don't own)
bool CloseSocket(int portNumber)

// Writes to writeBuffer, blocks if full
void SendByte(struct Socket socket, char data)

// Reads from writeBuffer, blocks until data
void ReadByte(struct Socket socket, char& data)
```

### Why do we want this Socket abstraction?
- We can start a program with sockets for "cin" and "cout" by default
- Implementing pipes and file redirects is now just a matter of switching which socket is defined as "cin" and "cout"
- That's what Linux does, so it can't be THAT wrong, right?

### So how do we do it?
The kernel needs to keep an internal registry of sockets, meaning it needs a pre-defined amount of sockets. When a user requests a socket, it needs to call a kernel method to request said socket, we'll traverse the data structure finding if the desired port is open and then assign it to a new socket. We need to allocate shared memory for this new socket (how to allow virtual mem access to shared memory?!?). The memory is allocated in two chunks of SOCKET\_BUFFER\_SIZE length which are then assigned to the Socket struct depending on whether this is a primary or secondary connection. (Just invert read/write buffers).

### Prerequisites
We need to be able to transition from user code to kernel code, typically this also involves halting interrupts (you shouldn't interrupt in the middle of kernel code, it is not safe). This implies at the very least that we know the design for:
- What a process looks like (TCB, virtual memory, etc.)
- How to switch from the kernel to a process
- How to switch from a process to the kernel
- How interrupts are handled (disabling, enabling, etc)
- How we handle shared memory

### Consequences and Concerns
- What is to stop a process from creating a Socket struct and filling in their own port number and read/write buffers?
- Answer: Restrict access to certain pids that have been explicitly assigned to a socket
- Where do we store the pid for the current block of code, and how do we ensure that the client can't manipulate it?
- Answer: Store it in the PCB and make sure it is outside of the client's virtual memory

# Process Control
For each process we need a PCB similar to below
```C
/* These are the registers we need to restore for a PCB */
struct ContextRegisters {
	unsigned long sp; //stack pointer: x18 technically
	unsigned long x19;
	unsigned long x20;
	...
	unsigned long x28;
	unsigned long fp; //frame pointer: x29 technically
	unsigned long pc; //program counter: x30 technically

	/* stack pointer: x18 technically (x9 in the tutorial seems to be an arbitrary choice, but 
	looking through specs it seems that 19-28 are callee saved, 29 is fp, 30 is pc. 
	9-15 are temporary registers, so I can assume we could choose any of those. Also,
	16-17 are intra-call-scratch registers (which I doubt we'll use) with the option of
	being additional temp registers. 18 is a platform register with the option of being 
	temp. Since we don't use any of the special parts of these registers, I opted to 
	keep it congruent and have sp be x18.*/
};

struct PCB {
	struct ContextRegisters context; //Stores the register states for this process
	long runDuration;				 //How many timer interrupts this process has left before it
									 //voluntarily yields it's time for other processes
	long priority;					 //Used to determine what to reset runDuration to after time
									 //is yielded (higher priority == more time to run)
	long unyieldableCount;			 //Indicates whether this process CAN yield at the current time.
									 //Note that this is not interrupt disabling, interrupts may still
									 //occur, but the execution is ensured to return to this process after.
									 //If our thread enters some scheduling code, it can handle interrupts safely,
									 //but it cannot give up execution entirely.
	pid_t pid;						 //This just stores the process identifier, pid_t is a generic typename, can be defined later
};

struct PCB* currentPCB;
struct PCB* processes[MAX_PROCESSES];
int numProcesses = 1;
```

As far as what `kernel_main` does, it should load up the invariants for task scheduling and then yield control.

```C
void kernel_main() {
	//Note, this is just the code directly involved in process scheduling, kernel_main may also do other things

	currentPCB = MAIN_PCB; //Const macro that initializes a PCB for kernel_main

	while(1) {
		Yield();
	}
}
```

`yield` may either try and follow the scheduling algorithm outlined in the tutorial's `_schedule` function, or it can be simple and just start by treating `processes` as a FIFO. Note that while a FIFO is simple to get up and running, it disregards any counter/priority we've coded. Either way it should follow the following pseudocode

```C
void Yield() {
	// Increment currentPCB's unyieldableCount
	struct PCB* nextPCB;
	// Decide what process we yield to and store in nextProcess
	SwitchProcessTo(nextPCB);
	// Decrement currentPCB's unyieldableCount
}

void SwitchProcessTo(struct PCB* nextPCB){
	if(nextPCB == currentPCB)
		return;
	struct PCB* prevPCB = currentPCB;
	currentPCB = nextPCB;
	SwitchContext(prevPCB, currentPCB);
}
```

And for SwitchContext, we have to go to Assembly

```Assembly
.global SwitchContext
SwitchContext:
	mov		x10, #PCB_CONTEXT_REGISTERS_OFFSET	// The offset from the start of a PCB struct to it's Context Registers (0 right now)

	add		x8, x0, x10							// Parameter 0 is prevPCB, so x8 now points to the ContextRegisters in prevPCB
	mov		x18, sp								// Move the current stack pointer into a register that we are saving for return
	stp		x18, x19, [x8] #16					// Store x18 and x19 at the start of ContextRegisters (sp and x19 respectively),
												// Post increment x8 by 16 bytes afterward (now points to x20)
	stp 	x20, x21, [x8] #16					// Likewise for the next few instructions
	stp 	x22, x23, [x8] #16
	stp 	x24, x25, [x8] #16
	stp 	x26, x27, [x8] #16
	stp 	x28, x29, [x8] #16					// Reminder that x29 is fp
	str		x30, [x8]							// Store just x30 (pc)

	add		x8, x1, x10							// Parameter 1 is currentPCB, so x8 now points to the ContextRegisters in currentPCB
	ldp		x18, x19, [x8] #16					// Similar to above, but loading a pair now. (Reminder that x18 is sp)
	ldp		x20, x21, [x8] #16
	ldp		x22, x23, [x8] #16
	ldp		x24, x25, [x8] #16
	ldp		x26, x27, [x8] #16
	ldp		x28, x29, [x8] #16					// Reminder that x29 is fp
	ldr		x30, [x8]							// Load just x30 (pc)

	mov		sp, x18								// Load the new stack pointer
	ret
```

### Creating a Process
So now we can switch between processes, but we only have the one. If a process wants to create another, it would call:

```C
int CreateProcess(void* function, void* arg) {
	// Increment currentPCB's unyieldableCount (We don't want to yield in the middle of this)

	pid_t newPid;
	// Get an open pid and store it in newPid. Return 1 if none exist (maxing out our processes global container)
	// Make sure you increment numProcesses too

	struct PCB* newPCB = (struct PCB*) GetPage(); // This is a memory management function, look at tutorial for a nice dummy version
	if(!newPCB)
		return 1;
	
	newPCB->pid = newPid;
	newPCB->priority = currentPCB->priority;
	newPCB->counter = currentPCB->priority;
	newPCB->unyieldableCount = 1;				// Disable yields until this process is called for the first time

	newPCB->context.x19 = function;				// Store the function and args we want to call in arbitrary registers
	newPCB->context.x20 = arg;

	newPCB->context.pc = (unsigned long)StartProcess;	// Set the program counter (also our return instruction) to a function
														// that will call our function with the given argument
	newPCB->context.sp = (unsigned long)newPCB + THREAD_SIZE;	// Stacks grow down, set our sp to the very end of the virtual page

	// Decrement currentPCB's unyieldableCount
	return 0;
}
```

Where StartProcess is an assembly function as follows:
```Assembly
.global StartProcess
StartProcess:
	// Call a function to decrement currentPCB's unyieldableCount
	mov x0, x20		// Store the argument for our process as our first parameter
	blr	x19			// And then call that function (stored in x19 right now)
```

### Side notes
We need a way to physically create a new process. We have a way to create a PCB, but it never gets called. Right now our `kernel_main` is going to boot up, initialize everything, and then continue yielding to itself. If there were any other processes running, it would theoretically yield to them, but since we have no way to create new ones, it kinda just loops. In the tutorial it spins up it's own "Dummy" processes that just print dummy data over and over. We can test with this or we can try and write a VERY basic shell executable that we can run. Chances are we can even store this executable below `LOW_MEMORY` (First 4MB). 
Essentially to test this system we need to either hard code some processes or find a way to have the user call them (calling executables out of the kernel)
