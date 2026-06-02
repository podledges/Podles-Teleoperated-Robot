// Configure Below to Match Your Setup
// 
// Server IP and port are taken from command-line arguments (see main()).
//
// Cert filenames assume the cert files live in the working directory
// (or you can edit these to absolute paths).
// Generate your own — see docs/TLS_SETUP.md in the repo root.
// =========================================================================

#define CA_CERT_FNAME       "signing.pem"
#define CLIENT_CERT_FNAME   "laptop.crt"
#define CLIENT_KEY_FNAME    "laptop.key"

// MUST match the Common Name (CN) on the Pi's server certificate.
// If you used "alex" when signing the Pi's cert, leave this as "alex".
#define SERVER_NAME_ON_CERT "alex"

// Routines to create a TLS client
#include "make_tls_client.h"

// Network packet types
#include "netconstants.h"

// Packet types, error codes, etc.
#include "constants.h"

#include <ncurses.h>
// Tells us that the network is running.
static volatile int networkActive=0;
volatile char lastCommand = 'r';
void handleError(const char *buffer)
{
	switch(buffer[1])
	{
		case RESP_OK:
			printf("Command / Status OK\n");

			break;

		case RESP_BAD_PACKET:
			printf("BAD MAGIC NUMBER FROM ARDUINO\n");
			break;

		case RESP_BAD_CHECKSUM:
			printf("BAD CHECKSUM FROM ARDUINO\n");
			break;

		case RESP_BAD_COMMAND:
			printf("PI SENT BAD COMMAND TO ARDUINO\n");
			break;

		case RESP_BAD_RESPONSE:
			printf("PI GOT BAD RESPONSE FROM ARDUINO\n");
			break;

		default:
			printf("PI IS CONFUSED!\n");
	}
}

void handleStatus(const char *buffer)
{
	int32_t data[16];
	memcpy(data, &buffer[1], sizeof(data));

	printf("\n ------- ALEX STATUS REPORT ------- \n\n");
	printf("Left Forward Ticks:\t\t%d\n", data[0]);
	printf("Right Forward Ticks:\t\t%d\n", data[1]);
	printf("Left Reverse Ticks:\t\t%d\n", data[2]);
	printf("Right Reverse Ticks:\t\t%d\n", data[3]);
	printf("Left Forward Ticks Turns:\t%d\n", data[4]);
	printf("Right Forward Ticks Turns:\t%d\n", data[5]);
	printf("Left Reverse Ticks Turns:\t%d\n", data[6]);
	printf("Right Reverse Ticks Turns:\t%d\n", data[7]);
	printf("Forward Distance:\t\t%d\n", data[8]);
	printf("Reverse Distance:\t\t%d\n", data[9]);
	printf("\n---------------------------------------\n\n");
}

void handleMessage(const char *buffer)
{
	printf("MESSAGE FROM ALEX: %s\n", &buffer[1]);
}

void handleCommand(const char *buffer)
{
	// We don't do anything because we issue commands
	// but we don't get them. Put this here
	// for future expansion
}

void handleNetwork(const char *buffer, int len)
{
	// The first byte is the packet type
	int type = buffer[0];

	switch(type)
	{
		case NET_ERROR_PACKET:
		handleError(buffer);
		break;

		case NET_STATUS_PACKET:
		handleStatus(buffer);
		break;

		case NET_MESSAGE_PACKET:
		handleMessage(buffer);
		break;

		case NET_COMMAND_PACKET:
		handleCommand(buffer);
		break;
	}
}

void sendData(void *conn, const char *buffer, int len)
{
	int c;
	printf("\nSENDING %d BYTES DATA\n\n", len);
	if(networkActive)
	{
		// Insert SSL write here to write buffer to network */

		c = sslWrite(conn, buffer, len);


		/* END TODO */	
		networkActive = (c > 0);
	}
}

void *readerThread(void *conn)
{
	char buffer[128];
	int len;

	while(networkActive)
	{
		/* Insert SSL read here into buffer */

		len = sslRead(conn, buffer, sizeof(buffer));
        printf("read %d bytes from server.\n", len);

		networkActive = (len > 0);

		if(networkActive)
			handleNetwork(buffer, len);
	}

	printf("Exiting network listener thread\n");
    
    /* Stop the client loop and call EXIT_THREAD */
		stopClient();
		EXIT_THREAD(conn);

    return NULL;
}

void flushInput()
{
	char c;

	while((c = getchar()) != '\n' && c != EOF);
}

void getParams(int32_t *params)
{
	printf("Enter distance/angle in cm/degrees (e.g. 50) and power in %% (e.g. 75) separated by space.\n");
	printf("E.g. 50 75 means go at 50 cm at 75%% power for forward/backward, or 50 degrees left or right turn at 75%%  power\n");
	scanf("%d %d", &params[0], &params[1]);
	flushInput();
}

void *writerThread(void *conn)
{
	int quit=0;

	while(!quit)
	{
		char ch;
		printf("NORMAL MODE: f, b, l, k(right), STOP IS 'r' || OTHER MODE: m (WASD) || GET COLOUR = c, OPEN CLAW = o, CLOSE CLAW = p, MEDPACK = i, h=clear stats, g=get stats q=exit)\n");
		scanf("%c", &ch);

		// Purge extraneous characters from input stream
		flushInput();

		char buffer[10];
		int32_t params[2];

		buffer[0] = NET_COMMAND_PACKET;
		switch(ch)
		{

			case 'm':
			case 'M': {
				initscr();
				cbreak();                // Initialize ncurses window
				noecho();
				timeout(300);             // Check every 100ms (responsive yet safe)
				curs_set(0);              // Invisible cursor
				
				int running = 1;
				char CommandSent = 'r';  // Track if forward command is active
				
				while (running) {
					int c = getch();      // Non-blocking check every 100ms
					
					if (c == 'w') {
						flushinp();
                            if (CommandSent != 'w') {
								buffer[1] = 'w';
                                params[0] = 10;     // Safe default distance
                                params[1] = 100;    // Speed
                                memcpy(&buffer[2], params, sizeof(params));
                                sendData(conn, buffer, sizeof(buffer));
                                CommandSent = 'w';
						         }
					          } 
					if(c == 's'){
							flushinp();
								if (CommandSent != 's') {
									buffer[1] = 's';
									params[0] = 10;     // Safe default distance
									params[1] = 100;    // Speed
									memcpy(&buffer[2], params, sizeof(params));
									sendData(conn, buffer, sizeof(buffer));
									CommandSent = 's'; 
								}
							}
							else if(c == 'a') {
							flushinp();
								if (CommandSent != 'a') {
									params[0] = 0;     // Safe default distance
									params[1] = 0;    // Speed
									buffer[1] = 'a';
									memcpy(&buffer[2], params, sizeof(params));
									sendData(conn, buffer, sizeof(buffer));
									CommandSent = 'a'; 
								}
							}
								else if(c == 'd') {
								flushinp();
									if (CommandSent != 'd') {
										buffer[1] = 'd';
										params[0] = 0;     // Safe default distance
										params[1] = 0;    // Speed
										memcpy(&buffer[2], params, sizeof(params));
										sendData(conn, buffer, sizeof(buffer));
										CommandSent = 'd'; 
									}
								}
					else if (c == ERR) { 
						flushinp(); // Key is released (no key detected)
						if (CommandSent != 'r') {
							buffer[1] = 'r';    // send stop command exactly once
							params[0] = 0;
							params[1] = 0;
							memcpy(&buffer[2], params, sizeof(params));
							sendData(conn, buffer, sizeof(buffer));
							CommandSent = 'r';  // Clear buffer to prevent flooding
						}
					} 
					else if (c == 'q' || c == 'Q') {
						flushinp();
						running = 0;
						if (CommandSent != 'r') {
							// Ensure robot stops if quitting
							buffer[1] = 'r';
							params[0] = 0;
							params[1] = 0;
							memcpy(&buffer[2], params, sizeof(params));
							sendData(conn, buffer, sizeof(buffer));
							CommandSent = 'r';
						}
					}
					else if (c == '=') {
						flushinp();
							if (CommandSent != '=') { // increase speed
								// Ensure robot stops if quitting
								buffer[1] = '=';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '=';
							}
						}
					else if (c == '-') {
						flushinp();
							if (CommandSent != '-') { // increase speed
									// Ensure robot stops if quitting
									buffer[1] = '-';
									params[0] = 0;
									params[1] = 0;
									memcpy(&buffer[2], params, sizeof(params));
									sendData(conn, buffer, sizeof(buffer));
									CommandSent = '-';
								}
					}
					else if (c == '1') {
						flushinp();
						if (CommandSent != '1') { // toggle speed
								// Ensure robot stops if quitting
								buffer[1] = '1';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '1';
							}
					}
					else if (c == '2') {
						flushinp();
						if (CommandSent != '2') { // toggle speed
								// Ensure robot stops if quitting
								buffer[1] = '2';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '2';
							}
					}
					else if (c == '3') {
						flushinp();
						if (CommandSent != '3') { // toggle speed
								// Ensure robot stops if quitting
								buffer[1] = '3';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '3';
							}
					}
					else if (c == '4') {
						flushinp();
						if (CommandSent != '4') { // toggle speed
								// Ensure robot stops if quitting
								buffer[1] = '4';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '4';
							}
					}
					else if (c == '5') {
						flushinp();
						if (CommandSent != '5') { // toggle speed
								// Ensure robot stops if quitting
								buffer[1] = '5';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '5';
							}
					}
					else if (c == '6') {
						flushinp();
						if (CommandSent != '6') { // toggle speed
								// Ensure robot stops if quitting
								buffer[1] = '6';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '6';
							}
					}
					else if (c == '7') {
						flushinp();
						if (CommandSent != '7') { // toggle speed
								// Ensure robot stops if quitting
								buffer[1] = '7';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '7';
							}
					}
					else if (c == '8') {
						flushinp();
						if (CommandSent != '8') { // toggle speed
								// Ensure robot stops if quitting
								buffer[1] = '8';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '8';
							}
					}
					else if (c == '9') {
						flushinp();
						if (CommandSent != '9') { // toggle speed
								// Ensure robot stops if quitting
								buffer[1] = '9';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '9';
							}
					}
					else if (c == '0') {
						flushinp();
						if (CommandSent != '0') { // toggle speed
								// Ensure robot stops if quitting
								buffer[1] = '0';
								params[0] = 0;
								params[1] = 0;
								memcpy(&buffer[2], params, sizeof(params));
								sendData(conn, buffer, sizeof(buffer));
								CommandSent = '0';
							}
					}					
					napms(150);
					 // Latency delay in between commands sent
				}
				endwin();
				break;
					}

			case 'f':
			case 'F':
			case 'b':
			case 'B':
			case 'l':
			case 'L':
			case 'k':
			case 'K':
						getParams(params);
						buffer[1] = ch;
						memcpy(&buffer[2], params, sizeof(params));
						sendData(conn, buffer, sizeof(buffer));
						break;
			case 'r':
			case 'R':
			case 'c':
			case 'C':
			case 'g':
			case 'G':
			case 'z':
			case 'Z':
			case 'o':
			case 'i':
			case 'p':
			case 'h':
			case 'w':
			case 's':
			case 'd':
			case 'a':
					params[0]=0;
					params[1]=0;
					memcpy(&buffer[2], params, sizeof(params));
					buffer[1] = ch;
					sendData(conn, buffer, sizeof(buffer));
					break;
			case 'q':
			case 'Q':
				quit=1;
				break;
			default:
				printf("BAD COMMAND\n");
		}
	}

	printf("Exiting keyboard thread\n");

stopClient();
EXIT_THREAD(conn);


    return NULL;
}




void connectToServer(const char *serverName, int portNum)
{
createClient(serverName, portNum, 1, CA_CERT_FNAME, SERVER_NAME_ON_CERT, 1, CLIENT_CERT_FNAME, CLIENT_KEY_FNAME, readerThread, writerThread);
}

int main(int ac, char **av)
{


	/* want to put this in the begining of my program which sets up the terminal*/

	if(ac != 3)
	{
		fprintf(stderr, "\n\n%s <IP address> <Port Number>\n\n", av[0]);
		exit(-1);
	}

    networkActive = 1;
    connectToServer(av[1], atoi(av[2]));


while(client_is_running())
{

}

	printf("\nMAIN exiting\n\n");
}
