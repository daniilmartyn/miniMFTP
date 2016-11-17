#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int makeSocket(int portnumber){
	
	int listenfd;
	if( 0 > (listenfd = socket(AF_INET,SOCK_STREAM,0))){
		perror("Error opening socket\n");
		exit(1);
	}
	
	//printf("socket made with descriptor %d\n", listenfd);
	
	struct sockaddr_in servAddr;
	
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(portnumber);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if( bind( listenfd,
				(struct sockaddr*) &servAddr, sizeof(servAddr)) < 0){
					perror("Error binding socket to port: ");
					exit(1);
	}
	
	//printf("socket bound to port %d\n", portnumber);
	
	listen( listenfd, 4);	
	
	//printf("listening with connection queue of 4\n");
	
	return listenfd;
}

int makeDataSocket(int* portnumber){

	int listenfd;
	if( 0 > (listenfd = socket(AF_INET,SOCK_STREAM,0))){
		perror("Error opening data socket\n");
		exit(1);
	}
	
	//printf("data socket made with descriptor %d\n", listenfd);
	
	struct sockaddr_in servAddr;
	
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(0);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//printf("binding...\n");
	if( bind( listenfd,
				(struct sockaddr*) &servAddr, sizeof(servAddr)) < 0){
					perror("Error binding socket to port\n");
					exit(1);
	}
	
	//printf("bind successful?\n");
	socklen_t addrlen = sizeof(servAddr);

	if(getsockname(listenfd, (struct sockaddr*) &servAddr, &addrlen) < 0){
		fprintf(stderr,"Error: %s\n", strerror(errno));
		exit(1);
	}
	
	//printf("sin_port: %d\n", servAddr.sin_port);
	//printf("portnumber: %x\n", portnumber);	

	*portnumber = ntohs(servAddr.sin_port);

	//printf("data socket bound to port %d\n", *portnumber);
	
	listen( listenfd, 1);	
	
	//printf("listening with connection queue of 1\n");
	
	return listenfd;

}

int getConnect(int listenfd){
	
	int connectfd;
	socklen_t length = sizeof(struct sockaddr_in);
	struct sockaddr_in clientAddr;
	
	connectfd = accept( listenfd, (struct sockaddr *) &clientAddr, &length);
	
	if(connectfd < 0){
		perror("Error establishing connection\n");
		exit(1);
	}
	
	return connectfd;
}

void writeMsg(char* msg, int connectfd){
  int i = 0;
  while(msg[i] != '\0') i++;
  write(connectfd,msg,i);
	write(connectfd, "\n", 1);
}

int dataConnect(int connectfd){
	int port = 0;
	int listenfd = makeDataSocket(&port);

	char portnum[15];
	sprintf(portnum, "%d", port);
	
	write(connectfd, "A", 1);
	writeMsg(portnum, connectfd);
	
	int dataconnectfd = getConnect(listenfd); 
	return dataconnectfd;
}

int socketSetUp(char * hostname,int portnumber){
    
    int socketfd;
    
    socketfd = socket( AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in servAddr;
    struct hostent* hostEntry;
    struct in_addr **pptr;
    
    memset( &servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(portnumber);
    
    hostEntry = gethostbyname(hostname);    
    
    if(hostEntry == NULL){
        herror("Error getting host name!");
        exit(1);
    }
        
    pptr = (struct in_addr **) hostEntry->h_addr_list;
    memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));
    
    int error = connect(socketfd, (struct sockaddr *) &servAddr, sizeof(servAddr));
    
    if( error < 0){
        fprintf(stderr, "Error connecting to server!");
        exit(1);
    }
    
    return socketfd;
}

void writePath(char* path, int socketfd){
	int i = 0;
	while(path[i] != '\0') i++;
	write(socketfd,path,i);
}