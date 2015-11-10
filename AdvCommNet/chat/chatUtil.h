enum {
	MAX_LINE = 80
};

struct clientInformation {
	int connected;
	struct sockaddr_in address;
	char hostname[MAX_LINE];
};

struct message {
	int cid;
	char str1[MAX_LINE];
	char str2[MAX_LINE];
};

enum {
	JOIN_CID_CODE = 0,
	DEBUG_ON = 1,
	DEBUG_OFF = 0,
	MAX_CLIENTS = 10
};

static const char JOIN_STRING[] = "JOIN";
static const char QUIT_STRING[] = "QUIT";

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

void pDebug(int debug, char *message) {
	if ( debug == 1 ) {
		printf("debug: %s \n");
	}
}
