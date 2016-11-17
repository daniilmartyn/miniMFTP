#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

#include "mftp.h"

/*
	This is the server side to a mini ftp system. The server listens to a connection, and as soon 
	as one is made a child process is forked off. The parent continues to listen for more requests 
	while the child accepts commands from the client and sends off information to the client.
*/

int dataconnect = -1;

char* getCmd(int connectfd){

	char buff[2];
	char* result = (char *) calloc(MAX_LENGTH,sizeof(char));

	while(1){
		read(connectfd, buff, 1);
			if(buff[0] == '\n')
				break;
		buff[1] = '\0';
		strcat(result, buff);
	}
	strcat(result, "\0");
	//printf("the result from getCmd is: %s\n", result);
	return result;
}

void terminate(int connectfd){
	printf("quitting\n");
	printf("sending acknowledgement\n");
	write(connectfd, "A\n", 2);
	printf("exited normally\n");
	exit(0);	
}

void rcd(char* cmd, int connectfd){
	
	struct stat area, *s = &area;
	//printf("recieved cmd: %s\n", cmd);
    if(cmd == NULL){
		write(connectfd, "E", 1);
		writeMsg("Error: you need to specify a directory\n",connectfd);
		fprintf(stderr, "Change directory failed.\n");
    }else if((lstat(cmd, s) == 0) && S_ISDIR(s->st_mode)){
        if(chdir(cmd) == -1){
			write(connectfd, "E", 1);
			writeMsg("Error: change directory failed\n", connectfd);
			fprintf(stderr, "Error occurred while changing directory: %s\n", strerror(errno));
		}else{
			printf("sending positive acknowledgement\n");
			write(connectfd, "A\n", 2);
			printf("successfully changed directory to %s\n", cmd);
		}
    }else{
		write(connectfd, "E", 1);
		writeMsg("This is not a valid directory", connectfd);
		fprintf(stderr, "Error: This is not a valid directory\n");
	}
}

void ls(int connectfd){
	if(dataconnect == -1){
		write(connectfd, "E", 1);
		writeMsg("Error: Data connection must be established first\n", connectfd);
		return;
	}else{
		write(connectfd,"A\n",2);
	}
		
	if(fork()){
		close(dataconnect);
		dataconnect = -1;
		printf("forked ls child, performing ls\n");
		wait(NULL);		
	}else{
		close(1); dup(dataconnect); close(dataconnect);
		dataconnect = -1;
		
		execlp("ls", "ls", "-la", (char *) NULL);
		fprintf(stderr, "Error occured with exec of ls\n");
	}
}

void get(char* path, int connectfd){
	
	if(dataconnect == -1){
		write(connectfd, "E",1);
		writeMsg("Error: Data connection must be established first\n", connectfd);
		return;
	}
		
	if(fork()){
		close(dataconnect);
		dataconnect = -1;
		wait(NULL);
	}else{
		FILE *fp;
		struct stat area, *s = &area;
		
		if(lstat(path, s) == 0 && !S_ISREG(s->st_mode)){
			write(connectfd, "E", 1);
			writeMsg("This is not a regular file!", connectfd);
			close(dataconnect);
			dataconnect = -1;
			exit(1);
		}
		
		
		
		fp = fopen(path, "r");
		if(fp == NULL){
			write(connectfd, "E", 1);
			writeMsg("Could not open file: no such file", connectfd);
			fprintf(stderr, "Error opening file for reading: %s\n", strerror(errno));
			close(dataconnect);
			dataconnect = -1;
			exit(1);
		}
		
		printf("sending positive acknoledgement\n");
		write(connectfd, "A\n", 2);
		printf("sending file to client...\n");
		
		char buff[512];
		int i;
		while((i = fread(buff, sizeof(char), 512, fp)) != 0){
			write(dataconnect, buff, i);
		}
		
		printf("sending file completed\n");
		exit(0);
	}
}

void put(char* path, int connectfd){
	
	if(dataconnect == -1){
		write(connectfd, "E",1);
		writeMsg("Error: Data connection must be established first\n", connectfd);
		return;
	}
	
	if(fork()){
		close(dataconnect);
		dataconnect = -1;
		wait(NULL);
	}else{
		FILE *fp;
			
		fp = fopen(path, "w");
		if(fp == NULL){
			write(dataconnect, "E", 1);
			writeMsg("Failed to open file for writing\n", dataconnect);
			fprintf(stderr, "Error opening file for writing: %s\n", strerror(errno));
			close(dataconnect);
			dataconnect = -1;
			exit(1);
		}
		
		printf("sending positive acknowledgement\n");
		write(connectfd, "A\n", 2);
		printf("receiving file from client...\n");
			
		char buff[512];
		int i;
		while((i=read(dataconnect,buff,512)) != 0){
			fwrite(buff,sizeof(char),i,fp);
		}
		
		printf("file received\n");
		exit(0);
	}
}

void doCommands(int connectfd){ 
	
	char* cmd = getCmd(connectfd);

	if(cmd[0] == 'Q'){
		free(cmd);
		terminate(connectfd);
	}else if( cmd[0] == 'C'){
		rcd(&cmd[1], connectfd);
		free(cmd);
	}else if( cmd[0] == 'D'){
		printf("establishing data connection\n");
		dataconnect = dataConnect(connectfd);
		printf("data connection established\n");
		free(cmd);
	}else if(cmd[0] == 'L'){
		free(cmd);
		ls(connectfd);		
	}else if(cmd[0] == 'G'){
		get(&cmd[1], connectfd);
		free(cmd);
	}else if(cmd[0] == 'P'){
		put(&cmd[1], connectfd);
		free(cmd);
	}
}


int main(){
	
	int listenfd = makeSocket(MY_PORT_NUMBER);
	
	while(1){
		int connectfd = getConnect(listenfd);
	
		if(fork()){
			close(connectfd);
			printf("child forked, waiting for new connection\n");
			
			while(waitpid(-1, NULL, WNOHANG)); // kill zombie children
					
		}else{	// child process
			printf("child process started\n");
			
			while(1){
				printf("waiting for new command\n");
				doCommands(connectfd);
			}
		}
	}
	return 0;
}
