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
#include "chatUtil.h"

struct clientInformation clientRegister[MAX_CLIENTS];

void usage();
void startServer(int port, int debug);
int readClientMessage(int debug, int clientSD);
void addClient(int clientSD, struct sockaddr_in client_addr, 
	char *domain, int debug);
void initialize();
void sendJoinAck(int sd, int connectionID, int debug);
void sendBcastMessage(int sd, struct message theMessage, int debug);
void sendMessage(int sd, int connectionID, struct message theMessage, int debug);

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

void initialize() {
	int i;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		clientRegister[i].connected = 0;
	}
}

void usage() {
	printf("Usage: chatServer <port> <debug>\n");
}

void startServer(int port, int debug) {
	struct sockaddr_in serv_addr, cli_addr;
	int sd;
	int clilen;
	int childSD;
	int i;
	fd_set active_fd_set, read_fd_set;

	pDebug(debug, "Started server...");

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if ( sd < 0 ) {
		pDebug(debug, "Error opening socket");
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);
	
	if ( bind(sd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) 
		< 0 ) {
		pDebug(debug, "Error binding to socket");
	}

	while ( 1 ) {
		receiveClientMessage(sd, debug);
	}
}

int receiveClientMessage(int sd, int debug) {
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);
	int receivedLen = 0;
	struct message rmsg;
	char buffer[3*MAX_LINE];
	bzero((char *) &buffer, sizeof(buffer));

	receivedLen = recvfrom(sd, buffer, 3*MAX_LINE, 0, 
		(struct sockaddr *)&clientAddr, &clientLen);

	rmsg = parseMessage(buffer);
	if ( rmsg.cid == 0 && strcmp(rmsg.str1,"JOIN") == 0 ) {
		addClient(sd, clientAddr, rmsg.str2, debug);
	} else if (rmsg.cid < 0 && strcmp(rmsg.str1,"QUIT") == 0 ) {
		//quit logic
	} else {
		sendBcastMessage(sd, rmsg, debug);
		fprintf(stderr, "CID: %i\n", rmsg.cid);
		fprintf(stderr, "Control: %s\n", rmsg.str1);
		fprintf(stderr, "Message: %s\n", rmsg.str2);
	}
}

void addClient(int sd, struct sockaddr_in client_addr, char *domain, int debug) {
	int i;
	int allocated = -1;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
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

	if ( allocated >= 0 ) {
		pDebug(debug, "Allocating ...");
		sendJoinAck(sd, allocated, debug);
	} else {
		pDebug(debug, "Could not allocate ... server busy");
	}
}
	
void sendJoinAck(int sd, int connectionID, int debug) {
	struct message joinack;
	joinack.cid = connectionID;
	strcpy(joinack.str1, "JOIN");
	strcpy(joinack.str2, clientRegister[connectionID].hostname);
	sendBcastMessage(sd, joinack, debug);
}

void sendBcastMessage(int sd, struct message theMessage, int debug) {
	int i;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( clientRegister[i].connected == 1 ) {
			sendMessage(sd, i, theMessage, debug);
		}
	}
}

void sendMessage(int sd, int connectionID, struct message theMessage, int debug) {
	char buffer[3*MAX_LINE];
	bzero((char *) &buffer, sizeof(buffer));

	sprintf(buffer, "%i %s %s", theMessage.cid, theMessage.str1,
		theMessage.str2);

	if ( sendto(sd, buffer, strlen(buffer), 0, 
		(struct sockaddr *)&clientRegister[connectionID].address,
		sizeof(clientRegister[connectionID].address)) < 0 ) {
	}
	pDebug(debug, "Sent message");
}
