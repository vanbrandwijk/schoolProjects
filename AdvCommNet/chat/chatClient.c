/******************************************************************************/
// chatClient.c
// A simple chat client
// @author: J. Joel vanBrandwijk
// @date 2015-11-11
/******************************************************************************/

//include system, network, and io libraries
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

//include chat library
#include "chatUtil.h"

/*
 * Function signature declarations see function definitions for further 
 * documentation
 */
void usage();
void startClient(char *serverName, int port, int debug); 
struct sockaddr_in getServer(char *serverName, int port);
int makeClientSocket ();
void joinChat(int sd, struct clientInformation myinfo, int debug);
void sendText(int sd, struct clientInformation myinfo, int debug, char *txt);
void quitChat(int sd, struct clientInformation myinfo, int debug);
void sendMessage(int sd, struct clientInformation myinfo, 
	struct message theMessage, int debug);
int receiveServerMessage(int sd, struct clientInformation myinfo, int debug);
const char * getCDN();

/*
 * main
 * Read command line parameters, validate arguments, and launch the chat
 * routine.
 */
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

/*
 * usage
 * Print usage information and exit
 */
void usage() {
	printf("Usage: chatServer <port> <debug>\n");
	exit(1);
}

/*
 * startClient
 * Start the client loop, beginning with connecting to the server and issuing
 * the JOIN command and completing with a QUIT command.
 * @param serverName The name of the server to which to connect
 * @param port The port to which to connect
 * @param debug Whether debug messages will be printed.
 */
void startClient(char *serverName, int port, int debug) {
	struct clientInformation myInformation;
	int sd;
	struct sockaddr_in server_addr;
	fd_set read_fd_set;
	char buffer[MAX_LINE];

	//Create a client socket
	sd = makeClientSocket();

	//Initialize the file descriptor set
	FD_ZERO(&read_fd_set);

	//Translate serverName and Port into a sockaddr_in
	server_addr = getServer(serverName, port);

	//Initilize the clientInformaiton datastructure for this client
	myInformation.connected = JOIN_CID_CODE;
	strcpy(myInformation.hostname, getCDN());
	bcopy((char *)&server_addr, (char *)&myInformation.address, 
		sizeof(server_addr));

	//While we haven't been issued a client identified, send a join command
	// and process a response.  Responses that are not join-acks will
	// result in a new join command being sent.
	while (myInformation.connected == JOIN_CID_CODE ) {
		joinChat(sd, myInformation, debug);
		myInformation.connected = 
			receiveServerMessage(sd, myInformation, debug);
	}

	//This is the main chat loop.  Adds standard input and the server
	// socket address to the file descriptor monitor.  
	while ( 1 ) {
		FD_SET(0, &read_fd_set);
		FD_SET(sd, &read_fd_set);

		select(sd+1, &read_fd_set, NULL, NULL, NULL);
	
		//If stdin has data, check to see if that data is "QUIT" and 
		// break out of loop. Otherwise, send the data.
		if ( FD_ISSET(0, &read_fd_set) ) {
			bzero((char *) &buffer, sizeof(buffer));
			read(STDIN_FILENO, buffer, MAX_LINE-1);
			if ( strncmp(buffer, QUIT_STRING, 4) == 0 ) {
				break;
			}
				
			sendText(sd, myInformation, debug, buffer);
		}

		//If the server socket has data, process that data
		if ( FD_ISSET(sd, &read_fd_set) ) {
			receiveServerMessage(sd, myInformation, debug);
		}
	}

	//Once the loop has broken, quit chat.
	quitChat(sd, myInformation, debug);
}

/*
 * makeClientSocket
 * Create a socket to use for sending data to the server
 */
int makeClientSocket () {
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

/*
 * getServer
 * Returns a sockaddr_in structure based on the serverName and port provided
 * @param serverName the server's dns name or ip
 * @param port The port
 * @return A sockaddr_in representing the server/port combination.
 */
struct sockaddr_in getServer(char *serverName, int port) {
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

/*
 * receiveServerMessage
 * Receive a message from the server on the client socket
 * @param sd The client socket
 * @param myinfo A clientInformation structure containing the server address
 * @param debug Whether debugging output should be printed
 * @return The new CID if receiving a join-ack for this client, 0 otherwise
 */
int receiveServerMessage(int sd, struct clientInformation myinfo, int debug) {
	socklen_t serverLen = sizeof(myinfo.address);
	int receivedLen = 0;
	struct message rmsg;
	char buffer[3*MAX_LINE];
	bzero((char *) &buffer, sizeof(buffer));

	//Receive a message as a raw string buffer and parse that message
	// into a message data structure.
	receivedLen = recvfrom(sd, buffer, 3*MAX_LINE, 0, 
			(struct sockaddr *)&myinfo.address, &serverLen);
	rmsg = parseMessage(buffer);
	pDebug(debug, RECV_STRING, rmsg);

	//Process messages containing the join command.  If the hostname
	// matches this client's hostname, return the cid in the message.
	if ( strcmp(rmsg.str1, JOIN_STRING) == 0 ) {
		if ( strcmp(rmsg.str2, myinfo.hostname) == 0 ) {
			printf("CID=%i assigned\n", rmsg.cid);
			fflush(stdout);
			return rmsg.cid;
		} else {
			printf("CID=%i %s joined\n", rmsg.cid, rmsg.str2);
			fflush(stdout);
			return 0;
		}
	//Process messages containing the quit command.
	} else if ( strcmp(rmsg.str1, QUIT_STRING) == 0 ) {
		printf("CID=%i %s quit\n", rmsg.cid, rmsg.str2);
		fflush(stdout);
		return 0;
	//Print all other messages which we didn't originally send.
	} else {
		if ( rmsg.cid != myinfo.connected ) {
			printf("CID=%i %s says \"%s\"\n", 
				rmsg.cid, rmsg.str1, rmsg.str2);
		}
		fflush(stdout);
		return 0;
	}
}

/*
 * joinChat
 * Prepare and send a JOIN command.
 * @param sd The client socket
 * @param myinfo A clientInformation structure containing the server address
 * @param debug Whether debugging output should be printed
 */ 
void joinChat(int sd, struct clientInformation myinfo, int debug) {
	struct message joinMessage;
	joinMessage.cid = JOIN_CID_CODE;
	strcpy(joinMessage.str1, JOIN_STRING);
	strcpy(joinMessage.str2, myinfo.hostname);

	sendMessage(sd, myinfo, joinMessage, debug);
}

/*
 * getCDN
 * Get the client domain name.  Prepend the process id to the hostname
 * for uniqueness
 * @return the client domain name.
 */
const char * getCDN() {
	char hostname[70];
	gethostname(hostname, 70);
	char *buffer = malloc(sizeof(char) * MAX_LINE);
	sprintf(buffer, "%i.%s", getpid(), hostname);
	return buffer;
}
	
/*
 * sendText
 * Prepare and send a text message.
 * @param sd The client socket
 * @param myinfo A clientInformation structure containing the server address
 * @param debug Whether debugging output should be printed
 * @param txt The text to send
 */
void sendText(int sd, struct clientInformation myinfo, int debug, char *txt) {
	struct message txtMessage;
	txtMessage.cid = myinfo.connected;
	strcpy(txtMessage.str1, myinfo.hostname);
	strcpy(txtMessage.str2, txt);

	sendMessage(sd, myinfo, txtMessage, debug);
}

/*
 * quitChat 
 * Prepare and send a QUIT command.
 * @param sd The client socket
 * @param myinfo A clientInformation structure containing the server address
 * @param debug Whether debugging output should be printed
 */
void quitChat(int sd, struct clientInformation myinfo, int debug) {
	struct message quitMessage;
	quitMessage.cid = -1 * myinfo.connected;
	strcpy(quitMessage.str1, QUIT_STRING);
	strcpy(quitMessage.str2, myinfo.hostname);

	sendMessage(sd, myinfo, quitMessage, debug);
}

/*
 * sendMessage
 * @param sd The client socket
 * @param myinfo A clientInformation structure containing the server address
 * @param theMessage The prepared message data structure to send
 * @param debug Whether debugging output should be printed
 */
void sendMessage(int sd, struct clientInformation myinfo, struct message theMessage, int debug) {
	char buffer[3*MAX_LINE];
	bzero((char *) &buffer, sizeof(buffer));

	sprintf(buffer, "%i %s %s", theMessage.cid, theMessage.str1,
		theMessage.str2);
	sendto(sd, buffer, strlen(buffer), 0, (struct sockaddr *)&myinfo.address, sizeof(myinfo.address));
	
	pDebug(debug, SENT_STRING, theMessage);
}
