#ifndef MFTP
#define MFTP
#define MY_PORT_NUMBER 49999
#define MAX_LENGTH 4096


int makeSocket(int portnumber);			// server related functions
int makeDataSocket(int* portnumber);
int getConnect(int listenfd);
int dataConnect(int connectfd);

void writeMsg(char* msg, int connectfd); // common to both server and client

int socketSetUp(char * hostname,int portnumber); // client related functions
void writePath(char* path, int socketfd);

#endif