#include <unistd.h>
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
#include "chatUtil.h"

void usage();
void startClient(char *serverName, int port, int debug); 
struct sockaddr_in getServer(char *serverName, int port, int debug);
int makeClientSocket (int debug);
void joinChat(int sd, struct clientInformation myinfo, int debug);
void sendText(int sd, struct clientInformation myinfo, int debug, char *txt);
void quitChat(int sd, struct clientInformation myinfo, int debug);
void sendMessage(int sd, struct clientInformation myinfo, struct message theMessage, int debug);
int receiveServerMessage(int sd, struct clientInformation myinfo, int debug);
const char * getCDN();

int main( int argc, char *argv[] ) {
	char* serverName;
	int port = 0;
	int debug = 0;
	
	if ( argc != 4 ) {
		usage();
	}

	serverName = argv[1];

	//validate & set debug
	debug = atoi(argv[3]);
	if ( debug == DEBUG_ON ) {
		printf("Debug is on.\n");
	} else if ( debug == DEBUG_OFF) {
	} else {
		usage();
	}

	port = atoi(argv[2]);
	if ( port < 0 || port > 65535 ) {
		usage();
	}

	startClient(serverName, port, debug);

	return 0;
}

void usage() {
	printf("Usage: chatServer <port> <debug>\n");
}

void startClient(char *serverName, int port, int debug) {
	struct clientInformation myInformation;
	int sd;
	struct sockaddr_in server_addr;
	fd_set read_fd_set;
	char buffer[MAX_LINE];

	sd = makeClientSocket(debug);

	FD_ZERO(&read_fd_set);

	server_addr = getServer(serverName, port, debug);

	myInformation.connected = 0;
	strcpy(myInformation.hostname, getCDN());
	bcopy((char *)&server_addr, (char *)&myInformation.address, 
		sizeof(server_addr));

	while (myInformation.connected == 0 ) {
		joinChat(sd, myInformation, debug);
		myInformation.connected = 
			receiveServerMessage(sd, myInformation, debug);
	}
	while ( 1 ) {
		FD_SET(0, &read_fd_set);
		FD_SET(sd, &read_fd_set);

		select(sd+1, &read_fd_set, NULL, NULL, NULL);
		if ( FD_ISSET(0, &read_fd_set) ) {
			read(STDIN_FILENO, buffer, MAX_LINE);
			if ( strncmp(buffer, "QUIT", 4) == 0 ) {
				break;
			}
				
			sendText(sd, myInformation, debug, buffer);
		}

		if ( FD_ISSET(sd, &read_fd_set) ) {
			receiveServerMessage(sd, myInformation, debug);
		}
	}
	quitChat(sd, myInformation, debug);
}

int makeClientSocket ( int debug ) {
	int sd;
	struct sockaddr_in client_addr;
	
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if ( sd < 0 ) {
		perror("Could not create client socket");
	}

	bzero((char *) &client_addr, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	client_addr.sin_port = htons(0);

	if ( bind(sd, (struct sockaddr *) &client_addr, sizeof(client_addr))
		< 0 ) {
		perror("Could not bind client socket");
	}
	return sd;
}

struct sockaddr_in getServer(char *serverName, int port, int debug) {
	struct sockaddr_in server_addr;
	struct hostent *server;
	
	server = gethostbyname(serverName);

	bzero((char*) &server_addr, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,
		(char *)&server_addr.sin_addr.s_addr,
		server->h_length);
	server_addr.sin_port = htons(port);

	return server_addr;
}	

int receiveServerMessage(int sd, struct clientInformation myinfo, int debug) {
	socklen_t serverLen = sizeof(myinfo.address);
	int receivedLen = 0;
	struct message rmsg;
	char buffer[3*MAX_LINE];
	bzero((char *) &buffer, sizeof(buffer));

	receivedLen = recvfrom(sd, buffer, 3*MAX_LINE, 0, 
			(struct sockaddr *)&myinfo.address, &serverLen);
	rmsg = parseMessage(buffer);
	pDebug(debug, "RECV", rmsg);

	if ( strcmp(rmsg.str1, "JOIN") == 0 ) {
		if ( strcmp(rmsg.str2, myinfo.hostname) == 0 ) {
			printf("CID=%i assigned\n", rmsg.cid);
			fflush(stdout);
			return rmsg.cid;
		} else {
			printf("CID=%i %s joined\n", rmsg.cid, rmsg.str2);
			fflush(stdout);
			return 0;
		}
	} else if ( strcmp(rmsg.str1, "QUIT") == 0 ) {
		printf("CID=%i %s quit\n", rmsg.cid, rmsg.str2);
		fflush(stdout);
		return 0;
	} else {
		if ( rmsg.cid != myinfo.connected ) {
			printf("CID=%i %s says \"%s\"\n", 
				rmsg.cid, rmsg.str1, rmsg.str2);
		}
		fflush(stdout);
		return 0;
	}
}

void joinChat(int sd, struct clientInformation myinfo, int debug) {
	struct message joinMessage;
	joinMessage.cid = 0;
	strcpy(joinMessage.str1, "JOIN");
	strcpy(joinMessage.str2, myinfo.hostname);

	sendMessage(sd, myinfo, joinMessage, debug);
}

const char * getCDN() {
	char hostname[70];
	gethostname(hostname, 70);
	char *buffer = malloc(sizeof(char) * MAX_LINE);
	sprintf(buffer, "%i.%s", getpid(), hostname);
	return buffer;
}
	

void sendText(int sd, struct clientInformation myinfo, int debug, char *txt) {
	struct message txtMessage;
	txtMessage.cid = myinfo.connected;
	strcpy(txtMessage.str1, myinfo.hostname);
	strcpy(txtMessage.str2, txt);

	sendMessage(sd, myinfo, txtMessage, debug);
}

void quitChat(int sd, struct clientInformation myinfo, int debug) {
	struct message quitMessage;
	quitMessage.cid = -1 * myinfo.connected;
	strcpy(quitMessage.str1, "QUIT");
	strcpy(quitMessage.str2, myinfo.hostname);

	sendMessage(sd, myinfo, quitMessage, debug);
}

void sendMessage(int sd, struct clientInformation myinfo, struct message theMessage, int debug) {
	char buffer[3*MAX_LINE];
	bzero((char *) &buffer, sizeof(buffer));

	sprintf(buffer, "%i %s %s", theMessage.cid, theMessage.str1,
		theMessage.str2);
	sendto(sd, buffer, strlen(buffer), 0, (struct sockaddr *)&myinfo.address, sizeof(myinfo.address));
	
	pDebug(debug, "SENT", theMessage);
}
