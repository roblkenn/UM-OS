# IPC's
> **Interprocess Communication:** Most interprocess communication can be handled with system calls -- one process writes data, while the other reads it sometime later. A kernel upcall is needed if a process generates an event that needs the instant attention of another process. As an example, UNIX sends an upcall to notify a process when the debugger wants to suspend or resume the process. Another use is for logout -- to notify applications that they should save file data and cleanly terminate. (OSPaP, 80)
> See also: (OSPaP, 113-115)

## IPC System Call Requirements
We need a socket-esque implementation for mailboxes. Enforce all sockets as 1 to 1? Or do we want to figure out how to do client/server sockets? I'm proceeding under 1 to 1 assumptions but we can adapt otherwise.
```
/* We need to write Socket.h/c implemented with the following */
/* SocketBuffers use a rolling index to speed up queries. Instead of reading the first byte and shifting the whole array down, we read from dataStart++. We know the array is full when dataStart = (dataEnd - 1) % SOCKET_BUFFER_SIZE. Let me know if I should document what my intentions are more */
struct SocketBuffer {
	char[SOCKET_BUFFER_SIZE] buffer;
	int dataStart = 0;
	int dataEnd = 0;
};

struct Socket {
	int portNumber;
	SocketBuffer writeBuffer;
	SocketBuffer readBuffer;
	pid_t creator, connector;
};
```

- ```// Create a socket set up for shared memory at given portNumber
// Return socket's portNumber is 0 if failed? Or throw exception?
// Pass portNumber = 0 to generate a random, unoccupied port
Socket CreateSocket(int portNumber)```
- ```// Connect to an existing socket (reverses which buffer is which, essentially)
// Same rules for portNumber except passing 0 shouldn't generate a random connection... that would be bad
Socket ConnectSocket(int portNumber)```
- ```// Close one end of a socket, when both creator and connector close, free up the Socket
// Right now it returns bool, but it should probably be void and just throw an exception if something happens?
// (Like the client trying to close a port they don't own)
bool CloseSocket(int portNumber)```
- ```// Writes to writeBuffer, blocks if full
void SendByte(Socket socket, char data)```
- ```// Reads from writeBuffer, blocks until data
void ReadByte(Socket socket, char& data)```

###Why do we want this Socket abstraction?
- We can start a program with sockets for "cin" and "cout" by default
- Implementing pipes and file redirects is now just a matter of switching which socket is defined as "cin" and "cout"
- That's what Linux does, so it can't be THAT wrong, right?
