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

#include "mftp.h"

/*
	This is the client side to a mini ftp system. A user can connect to a remote server and using 
	some basic terminal commands access the file structure on the server side and print out information 
	on the terminal on the client side.
*/

char* getAck(int socketfd){

	char buff[2];
	char* result = (char *) calloc(MAX_LENGTH,sizeof(char));

	while(buff[0] != '\n'){
		read(socketfd, buff, 1);
		buff[1] = '\0';
		strcat(result, buff);
	}
	strcat(result, "\0");

	return result;
}

char* getName(char* path){
	int i = 0;
	int j = 0;
	while(path[i] != '\0'){
		if(path[i] == '/')
		j = i+1;
		i++;
	}

	if( j == 0)
		return path;
	else
		return &path[j];
}

char** getCommand(char** cmd){
	char* buff = NULL;
		size_t n = 0;
		
		if(getline(&buff, &n, stdin) == -1){
			fprintf(stderr, "Failed to read line\n");
			return NULL;
		}
		
		char* token;
		const char delimit[] = {' ','\n','\0'};
		token = strtok(buff,delimit);

		int i = 0;
		while(1){
			if(token == NULL){
				cmd[i] = NULL;
				break;
			}
			cmd[i] = token;
			token = strtok(NULL, delimit);
			i++;
		}
	return cmd;
}

void terminate(int socketfd){
	printf("Exiting program...\n");
	write(socketfd,"Q\n",2);
	
	char* buff = getAck(socketfd);
	if(buff[0] == 'A'){
		close(socketfd);
		free(buff);
		exit(0);
	}else{
	  fprintf(stderr, "Error: %s\n", &buff[1]);
	  free(buff);
	}
}

void cd(char* path){
	struct stat area, *s = &area;
    if(path == NULL){
		fprintf(stderr, "Error: you need to specify a directory\n");
    }else if((lstat(path, s) == 0) && S_ISDIR(s->st_mode)){
        if(chdir(path) == -1){
			fprintf(stderr, "Error occurred while changing directory: %s\n", strerror(errno));
		}
    }else{
		fprintf(stderr, "Error: This is not a valid directory\n");
	}
}

void rcd(char* path, int socketfd){
	write(socketfd, "C", 1);
	writePath(path, socketfd);
	write(socketfd, "\n", 1);

	char* buff = getAck(socketfd);

	if(buff[0] == 'A'){
		printf("Directory changed\n");
		free(buff);
	}else if(buff[0] == 'E'){
		fprintf(stderr, "Error: %s", &buff[1]);
		free(buff);
	}
}

void ls(){	
	if(fork()){ // parent process
		wait(NULL);
	}else{ // child process executing ls | more -20
		int fd[2];
		int rdr, wtr;
	  
		assert(pipe(fd) >= 0);
		rdr = fd[0]; wtr = fd[1];
		
		if(fork()){
			close(wtr);
			close(0); dup(rdr); close(rdr);

			execlp("more", "more", "-20", (char *) NULL);
			fprintf(stderr, "Error Executing more: %s\n", strerror(errno));
		}else{
			close(rdr);
			close(1); dup(wtr); close(wtr);
			
			execlp("ls", "ls", "-la", (char *) NULL);
			fprintf(stderr, "Error Executing ls: %s\n", strerror(errno));
		}
	}
}

void rls(int socketfd, char* path){

	write(socketfd, "D\n", 2);
	char* buff  = getAck(socketfd);

	int portNum = atoi(&buff[1]);
	free(buff);
	int dataSocket = socketSetUp(path,portNum);

	write(socketfd, "L\n", 2);

	buff =	getAck(socketfd);
	
	if(buff[0] == 'E'){
		fprintf(stderr, "Error: %s\n", &buff[1]);
		close(dataSocket);
		free(buff);
	}else{
		free(buff);
		if(fork()){ // parent
			close(dataSocket);
			wait(NULL);
		}else{
			close(0); dup(dataSocket); close(dataSocket);
			execlp("more", "more", "-20", (char *) NULL);
			fprintf(stderr, "Error executing more in rls\n");
	  }
	}
}

void get(char* path, int socketfd, char* hostname){
	write(socketfd, "D\n", 2);
	char* buff = getAck(socketfd);
	if(buff[0] == 'E'){
		fprintf(stderr, "Error in get data connection: %s\n", &buff[1]);
		free(buff);
		exit(1);
	}
	int portNum = atoi(&buff[1]);
	free(buff);
	
	int dataSocket = socketSetUp(hostname,portNum);
	write(socketfd, "G",1);
	writePath(path, socketfd);
	write(socketfd, "\n", 1);
	buff = getAck(socketfd);
	
	if(buff[0] == 'E'){
		fprintf(stderr, "Error in get: %s\n", &buff[1]);
		free(buff);
		close(dataSocket);
		return;
	}else{
		free(buff);
		if(fork()){
			close(dataSocket);
			wait(NULL);
		}else{
			char* name = getName(path);
			FILE *fp;
			
			fp = fopen(name, "w");
			if(fp == NULL){
				fprintf(stderr, "Error opening file for writing: %s\n", strerror(errno));
				close(dataSocket);
				exit(1);
			}
			
			char buff[512];
			int i;
			while((i=read(dataSocket,buff,512)) != 0){
				fwrite(buff,sizeof(char),i,fp);
			}
			fclose(fp);
			close(dataSocket);
			exit(0);
		}
	}
}

void show(char* path, int socketfd, char* hostname){
	write(socketfd, "D\n", 2);
	char* buff = getAck(socketfd);
	
	int portNum = atoi(&buff[1]);
	free(buff);
	
	int dataSocket = socketSetUp(hostname,portNum);
	write(socketfd, "G",1);
	writePath(path, socketfd);
	write(socketfd, "\n", 1);
	buff = getAck(socketfd);
	
	if(buff[0] == 'E'){
		fprintf(stderr, "Error in show: %s\n", &buff[1]);
		close(dataSocket);
		free(buff);
	}else{
		free(buff);
		if(fork()){
			close(dataSocket);
			wait(NULL);
		}else{
			close(0); dup(dataSocket); close(dataSocket);
			execlp("more", "more", "-20", (char *) NULL);
			fprintf(stderr, "Error executing more in show\n");
		}
	}
	
}

void put(char* path, int socketfd, char* hostname){
	
	FILE *fp;

	fp = fopen(path, "r");
	if(fp == NULL){
		fprintf(stderr, "Error opening file for reading: %s\n", strerror(errno));
		return;
	}
	
	struct stat area, *s = &area;
	if(lstat(path, s) == 0 && !S_ISREG(s->st_mode)){
		fprintf(stderr, "File is not a regular file!\n");
		return;
	}
	
	write(socketfd, "D\n", 2);
	char* buff = getAck(socketfd);
	if(buff[0] == 'E'){
		fprintf(stderr, "Error in put data connection: %s\n", &buff[1]);
		free(buff);
		fclose(fp);
		exit(1);
	}
	int portNum = atoi(&buff[1]);
	free(buff);

	int dataSocket = socketSetUp(hostname, portNum);
	write(socketfd, "P", 1);
	char* name = getName(path);
	writePath(name, socketfd);
	write(socketfd, "\n", 1);
	buff = getAck(socketfd);

	if(buff[0] == 'E'){
		fprintf(stderr, "Error in put: %s\n", &buff[1]);
		free(buff);
		fclose(fp);
		close(dataSocket);
		return;
	}else{
		free(buff);
		if(fork()){
			close(dataSocket);
			fclose(fp);
			wait(NULL);
		}else{

			char buff[512];
			int i;
			while((i = fread(buff, sizeof(char), 512, fp)) != 0){
				write(dataSocket, buff, i);
			}
			fclose(fp);
			exit(0);
		}
	}
}

void executeCommand(char** cmd, int socketfd, char* hostname){
	
	if(strcmp(cmd[0],"exit") == 0){
		terminate(socketfd);
	}else if(strcmp(cmd[0],"cd") == 0){
		cd(cmd[1]);
	}else if(strcmp(cmd[0],"rcd") == 0){
		rcd(cmd[1], socketfd);
	}else if(strcmp(cmd[0],"ls") == 0){
		ls();
	}else if(strcmp(cmd[0],"rls") == 0){
	  rls(socketfd, hostname);
	}else if(strcmp(cmd[0],"get") == 0){
		get(cmd[1],socketfd, hostname);
	}else if(strcmp(cmd[0],"show") == 0){
		show(cmd[1],socketfd, hostname);
	}else if(strcmp(cmd[0],"put") == 0){
		put(cmd[1],socketfd, hostname);
	}else{
		fprintf(stderr,"Error, given command in incorrect. Please try again\n");
	}	
}

int numCmds(char** cmd){
	int count = 0;
	while(cmd[count] != NULL) count++;
	return count;
}

int main(int argc, char** argv){
	
	if(argc != 2){
		fprintf(stderr,"Invalid number of arguments. Usage: ./daytime <IPaddress>\n");
		exit(1);
	}
	
	int socketfd = socketSetUp(argv[1], MY_PORT_NUMBER);
	
	printf("Connection to %s successful!\n", argv[1]);
	
	while(1){
		printf("Input your command:\n");

		char* cmd[256];
		if(getCommand(cmd) == NULL || cmd[0] == NULL){
			fprintf(stderr, "Failed to get command\n");
			continue;
		}

		if(numCmds(cmd) > 2){
			fprintf(stderr, "Too many arguments, try again.\n");
			continue;
		}
		executeCommand(cmd, socketfd, argv[1]);		
	}
	return 0;
}
