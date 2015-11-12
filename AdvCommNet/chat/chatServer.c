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

struct clientInformation clientRegister[MAX_CLIENTS+1];

void usage();
void startServer(int port, int debug);
int readClientMessage(int debug, int clientSD);
void addClient(int clientSD, struct sockaddr_in client_addr, 
	char *domain, int debug);
void removeClient(int sd, int cid, char *domain, int debug);
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
	exit(1);
}

void startServer(int port, int debug) {
	struct sockaddr_in serv_addr, cli_addr;
	int sd;
	int clilen;
	int childSD;
	int i;
	fd_set active_fd_set, read_fd_set;

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if ( sd < 0 ) {
		perror("Error opening socket");
		exit(1);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);
	
	if ( bind(sd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) 
		< 0 ) {
		perror("Error binding to socket");
		exit(1);
	} else {
		printf("Waiting for data on UDP port %i\n", port);
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
	pDebug(debug, RECV_STRING, rmsg);	

	if ( rmsg.cid == 0 && strcmp(rmsg.str1,JOIN_STRING) == 0 ) {
		addClient(sd, clientAddr, rmsg.str2, debug);
	} else if (rmsg.cid < 0 && strcmp(rmsg.str1,QUIT_STRING) == 0 ) {
		removeClient(sd, rmsg.cid, rmsg.str2, debug);
	} else {
		sendBcastMessage(sd, rmsg, debug);
	}
}

void addClient(int sd, struct sockaddr_in client_addr, char *domain, int debug) {
	int i;
	int allocated = -1;

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

	if ( allocated >= 0 ) {
		sendJoinAck(sd, allocated, debug);
	} else {
		printf("Server can only support %i connections.\n", i);
	}
}

void removeClient(int sd, int cid, char *domain, int debug) {
	struct message rmsg;
	rmsg.cid = -1 * cid;
	strcpy(rmsg.str1, QUIT_STRING);
	strcpy(rmsg.str2, domain);
	
	sendBcastMessage(sd, rmsg, debug);
	clientRegister[-1 * cid].connected = 0;
}
	
void sendJoinAck(int sd, int connectionID, int debug) {
	struct message joinack;
	joinack.cid = connectionID;
	strcpy(joinack.str1, JOIN_STRING);
	strcpy(joinack.str2, clientRegister[connectionID].hostname);
	sendBcastMessage(sd, joinack, debug);
}

void sendBcastMessage(int sd, struct message theMessage, int debug) {
	int i;
	for ( i = 1; i < MAX_CLIENTS+1; i++ ) {
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
	pDebug(debug, SENT_STRING, theMessage);
}
