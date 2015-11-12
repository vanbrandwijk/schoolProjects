/******************************************************************************/
// chatUtil.h
// Shared datastructures and utility funcitons for chat clients and servers 
// @author J. Joel vanBrandwijk 
// @date 2015-11-11 
/******************************************************************************/

//Maximum input line length
enum {
	MAX_LINE = 80
};

/*
 * Client information data structure
 * @param connected Server: 0 or 1 connection state; Client: connection id
 * @param address Server: client address; client: server address
 * @param hostname: pid.hostname of the client
 */
struct clientInformation {
	int connected;
	struct sockaddr_in address;
	char hostname[MAX_LINE];
};

/*
 * Message data structure
 * @param cid the client id
 * @param str1 command or hostname
 * @param str2 message or hostname
 */
struct message {
	int cid;
	char str1[MAX_LINE];
	char str2[MAX_LINE];
};

/*
 * Configuration values
 */
enum {
	JOIN_CID_CODE = 0,
	DEBUG_ON = 1,
	DEBUG_OFF = 0,
	MAX_CLIENTS = 10
};

/*
 * Strings for joining or quitting
 */
static const char JOIN_STRING[] = "JOIN";
static const char QUIT_STRING[] = "QUIT";

/*
 * Strings for debugging direction
 */
static const char SENT_STRING[] = "SENT";
static const char RECV_STRING[] = "RECV";

/*
 * parseMessage
 * @param buffer The message buffer to parse
 * @return message data structure
 * This fuction parses a message buffer string into component parts.
 */
struct message parseMessage(char *buffer) {
	struct message rmsg;
	int rcid;
	char rstr1[MAX_LINE];
	char rstr2[MAX_LINE];

	sscanf(buffer, "%i %s %80[^\n]", &rcid, rstr1, rstr2);

	rmsg.cid = rcid;
	strcpy(rmsg.str1, rstr1);
	strcpy(rmsg.str2, rstr2);

	return rmsg;
}

/*
 * pDebug
 * Print debugging information
 * @param debug Boolean is debug on?
 * @param direction Formatting parameter
 * @param rmsg The message to print
 */
void pDebug(int debug, const char *direction, struct message rmsg) {
	sscanf(rmsg.str2, "%80[^\n]", &rmsg.str2);
	if ( debug == DEBUG_ON ) {
		if ( strcmp(direction, SENT_STRING) == 0 ) {
			printf("DEBUG: Sending <%i %s %s>\n",
				rmsg.cid, rmsg.str1, rmsg.str2);
		} else if ( strcmp(direction, RECV_STRING) == 0 ) {
			printf("DEBUG: Receiving <%i %s %s>\n",
				rmsg.cid, rmsg.str1, rmsg.str2);
		} else {
			printf("DEBUG: <%i %s %s>\n", 
				rmsg.cid, rmsg.str1, rmsg.str2);
		}
	}
}
