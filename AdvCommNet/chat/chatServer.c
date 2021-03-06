/******************************************************************************/
// chatClient.c
// A simple chat client
// @author: J. Joel vanBrandwijk
// @date 2015-11-11
/******************************************************************************/

//include system, network, and io libraries
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

//include chat library
#include "chatUtil.h"

//define a global list of registered clients
struct clientInformation clientRegister[MAX_CLIENTS+1];

/*
 * Function signature declarations see function definitions for further 
 * documentation
 */
void usage();
void startServer(int port, int debug);
int readClientMessage(int debug, int clientSD);
void addClient(int clientSD, struct sockaddr_in client_addr, 
	char *domain, int debug);
void removeClient(int sd, int cid, char *domain, int debug);
void initialize();
void sendJoinAck(int sd, int connectionID, int debug);
void sendBcastMessage(int sd, struct message theMessage, int debug);
void sendMessage(int sd, int connectionID, struct message theMessage, 
	int debug);

/*
 * main
 * Read command line parameters, validate arguments, and launch the chat
 * routine.
 */
int main( int argc, char *argv[] ) {
	char* server;
	int port = 0;
	int debug = 0;
	
	if ( argc != 3 ) {
		usage();
	}

	//validate & set debug
	debug = atoi(argv[2]);
	if ( debug == DEBUG_ON ) {
		printf("Debug is on.\n");
	} else if ( debug == DEBUG_OFF) {
	} else {
		usage();
	}

	port = atoi(argv[1]);
	if ( port < 0 || port > 65535 ) {
		usage();
	}

	initialize();
	startServer(port, debug);

	return 0;
}

/*
 * initialize
 * Set all clients into an unregistered state
 */
void initialize() {
	int i;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		clientRegister[i].connected = 0;
	}
}

/*
 * usage
 * Print usage information and exit
 */
void usage() {
	printf("Usage: chatServer <port> <debug>\n");
	exit(1);
}

/*
 * startServer
 * Start the server loop, listenting for and processing data on the port
 * @param port The port to which to listen 
 * @param debug Whether debug messages will be printed.
 */
void startServer(int port, int debug) {
	struct sockaddr_in serv_addr, cli_addr;
	int sd;
	int clilen;
	int childSD;
	int i;
	fd_set active_fd_set, read_fd_set;

	//initialize the server socket
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if ( sd < 0 ) {
		perror("Error opening socket");
		exit(1);
	}

	//Set up the server listening address and port
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);
	
	//Bind the server socket
	if ( bind(sd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) 
		< 0 ) {
		perror("Error binding to socket");
		exit(1);
	} else {
		printf("Waiting for data on UDP port %i\n", port);
	}

	//Recieve client data as it becomes available on the server socket
	while ( 1 ) {
		receiveClientMessage(sd, debug);
	}
}

/*
 * receiveClientMessage
 * Receive a message from the client on the server socket
 * @param sd The server socket
 * @param debug Whether debugging output should be printed
 */
int receiveClientMessage(int sd, int debug) {
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);
	int receivedLen = 0;
	struct message rmsg;
	char buffer[3*MAX_LINE];
	bzero((char *) &buffer, sizeof(buffer));

	//Receive a message as a raw string buffer and parse that message into
	// a message data structure.
	receivedLen = recvfrom(sd, buffer, 3*MAX_LINE, 0, 
		(struct sockaddr *)&clientAddr, &clientLen);
	
	rmsg = parseMessage(buffer);
	pDebug(debug, RECV_STRING, rmsg);	

	//Process messages containing the join command; add the client
	if ( rmsg.cid == 0 && strcmp(rmsg.str1,JOIN_STRING) == 0 ) {
		addClient(sd, clientAddr, rmsg.str2, debug);
	//Process messages containing the quit command; remove the client
	} else if (rmsg.cid < 0 && strcmp(rmsg.str1,QUIT_STRING) == 0 ) {
		removeClient(sd, rmsg.cid, rmsg.str2, debug);
	//Re-broadcast all other messages
	} else {
		sendBcastMessage(sd, rmsg, debug);
	}
}

/*
 * addClient
 * Add a new client to the registered client list
 * @param sd The server socket
 * @param client_addr The client address sockaddr_in structure
 * @param domain The client domain
 * @param debug Whether to output debugging informaiton
 */
void addClient(int sd, struct sockaddr_in client_addr, char *domain, int debug) {
	int i;
	int allocated = -1;

	//Find the first available client registration number and assign
	// this client's information to that number.  Remember that number.
	for ( i = 1; i < MAX_CLIENTS+1; i++ ) {
		if ( clientRegister[i].connected == 0 ) {
			clientRegister[i].connected = 1;
			bcopy((char *)&client_addr, 
				(char *)&clientRegister[i].address, 
				sizeof(client_addr));
			strcpy(clientRegister[i].hostname, domain);
			allocated = i;
			break;
		}
	}

	//Send a join-ack including the newly registered client's number.
	if ( allocated >= 0 ) {
		sendJoinAck(sd, allocated, debug);
	} else {
		printf("Server can only support %i connections.\n", i);
	}
}

/*
 * removeClient
 * Remove a client from the registered client list
 * @param sd The server socket
 * @param cid The client to remove
 * @param domain The client domain
 * @param debug Whether to output debugging informaiton
 */
void removeClient(int sd, int cid, char *domain, int debug) {
	struct message rmsg;

	//construct a QUIT-ACK message
	rmsg.cid = -1 * cid;
	strcpy(rmsg.str1, QUIT_STRING);
	strcpy(rmsg.str2, domain);
	
	//broadcast the quit-ack message
	sendBcastMessage(sd, rmsg, debug);

	//remove the client from the register
	clientRegister[-1 * cid].connected = 0;
}

/*
 * sendJoinAck
 * Send a join acknowledgement over broadcast
 * @param sd The server socket
 * @param connectionID the client id to ack
 * @debug debug Whether to output debugging information
 */	
void sendJoinAck(int sd, int connectionID, int debug) {
	struct message joinack;
	joinack.cid = connectionID;
	strcpy(joinack.str1, JOIN_STRING);
	strcpy(joinack.str2, clientRegister[connectionID].hostname);
	sendBcastMessage(sd, joinack, debug);
}

/*
 * sendBcastMessage
 * Send a message to all registered clients
 * @param sd The server socket
 * @param theMessage The message to send 
 * @debug debug Whether to output debugging information
 */	
void sendBcastMessage(int sd, struct message theMessage, int debug) {
	int i;
	for ( i = 1; i < MAX_CLIENTS+1; i++ ) {
		if ( clientRegister[i].connected == 1 ) {
			sendMessage(sd, i, theMessage, debug);
		}
	}
}

/*
 * sendMessage
 * Send a message to a registered clients
 * @param sd The server socket
 * @param connectionID the client id to which to send 
 * @param theMessage The message to send 
 * @debug debug Whether to output debugging information
 */	
void sendMessage(int sd, int connectionID, struct message theMessage, int debug) {
	char buffer[3*MAX_LINE];
	bzero((char *) &buffer, sizeof(buffer));
	
	//Translate the message from a message data structure into a raw 
	// buffer string
	sprintf(buffer, "%i %s %s", theMessage.cid, theMessage.str1,
		theMessage.str2);

	if ( sendto(sd, buffer, strlen(buffer), 0, 
		(struct sockaddr *)&clientRegister[connectionID].address,
		sizeof(clientRegister[connectionID].address)) < 0 ) {
	}
	pDebug(debug, SENT_STRING, theMessage);
}
