CC = gcc
COPTS =-g -Wall -pedantic -std=c99  
JUNK = *.o *~ *.gch *.dSYM mftpserve mftp

all: mftpserve mftp

mftpserve: mftpserve.o mftp_include.o
	$(CC) $(COPTS) -o mftpserve mftpserve.o mftp_include.o

mftp: mftp.o mftp_include.o
	$(CC) $(COPTS) -o mftp mftp.o mftp_include.o

mftp_include.o:	mftp_include.c mftp.h
	$(CC) $(COPTS) -c mftp_include.c mftp.h

mftpserve.o: mftpserve.c
	$(CC) $(COPTS) -c mftpserve.c

mftp.o: mftp.c
	$(CC) $(COPTS) -c mftp.c

clean: 
	rm $(JUNK)
