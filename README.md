# client-server-app
Multi-threaded client server application

Client:
Connects to the servers using the socket library in C.
Send and Receives command sent by the server.
No parsing done on client end.
Client can send command to ping sites, check status of their request, help to see all available commands and exit to exit the application. Client can also send specific <REQUEST NUMBER> in their command to check its result.

Server:
Threads implemented using pthread POSIX library.
Server starts with a Parent thread.
As client connects, each client is connection is given its own thread.
Client thread parse data and sends back reply to client.
5 Worker threads responsible for Pinging Sites and saving result in a queue.
Queue implemented using linked list data structure, which stores the request number, websites to be pinged and their results.
Another threads waits for user input to exit the server application. Server waits for all clients to exit before shutting down the application.

Variable run is not locked using a semaphore. This should be done by whoever uses this code.

#Instructions

Both server.c and client.c can be compiled as follows: 
gcc -o server server.c -pthread
gcc -o client client.c

Server can be executed using ./server command
For client, we are required to input the port no which the client application will attempt to connect to. The listening port number is shown on the server application's terminal screen and hence should be executed first.
./client 4000

The server starts with port no 4000 in an attempt to create a socket and bind it, if it fails the port number is increased by 1 and an attempt is made again to create a socket. This goes on till port no reaches its set limit at 5000 and if the application still fails to create a socket, it exits. User should then wait for sometime before the port number becomes available.
 
The server displays its status and the commands received on its terminal. It also allows to start the exit proceure from its terminal by pressing 'q'. The server waits for all clients to exit before finally shutting down. 

The available client commands can be checked by sending the 'help' command.

The pingSites command has a limit of 10 websites which can be easily increased as required. 
