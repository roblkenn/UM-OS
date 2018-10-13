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
// Note I've called variables that I'm not sure how we want to implement as void
// Essentially it needs to know it's own pid
// It needs to know where in it's execution it was at (starts at 0x0, interrupt called at 0x8, needs to resume at 0x8)
// It needs to know where it's stack/heap/instructions are in memory (if at all)
// It needs to know where it's stack/heap/instructions are on disk (if not in memory)
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
```

Then the kernel will have a queue of PCBs. If none, we stall until one becomes available. If there are some, we want to pull of the first one, load it's instructions, stack, heap, etc. and continue execution. This occurs until the process either yields control back to the kernel (it finished or is waiting on some IPC), or an interrupt happens. Unless the process has completed, we want to update the PCB and place it at the end of the queue, then pop the next one and continue.
