#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/md5.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include "support.h"

/*
 * Make sure that the student name and email fields are not empty.
 */
void check_team(char * progname) {
	if((strcmp("", team.name1) == 0) || (strcmp("", team.email1) == 0)) {
		printf("%s: Please fill in the student struct in team.c\n", progname);
		exit(1);
	}
	printf("Student 1: %s\n", team.name1);
	printf("Email 1  : %s\n", team.email1);
	printf("Student 2: %s\n", team.name2);
	printf("Email 2  : %s\n", team.email2);
	printf("Student 3: %s\n", team.name3);
	printf("Email 3  : %s\n", team.email3);
	printf("\n");
}

inline size_t min(size_t a, size_t b) {
	if (a < b) {
		return a;
	}
	return b;
}

int streamData(const int socketid, const void* data, const long long size) {
	const char* buffer = (const char*)data;
	long long bytesRemaining = size;
	long long bytesSent;

	//continue to send information across the socket until an error occurs
	//or we've fulfilled the number of bytes of the request
	while (bytesRemaining > 0) {
		if ((bytesSent = write(socketid, buffer, bytesRemaining)) <= 0) {
			if (errno != EINTR) {
				fprintf(stderr, "Write error: %s\n", strerror(errno));
				return -1;
			}
			bytesSent = 0;
		}
		bytesRemaining -= bytesSent;
		buffer += bytesSent;
	}
	
	return 0;
}

Buffer* socketGetLine(const int socketid) {
	bool retry = true;
	char data = 0;
	int bytesRead;
	Buffer* message = new Buffer(BUFFER_SIZE);

	//continue to attempt reading data from the socket
	//until we encounter a new-line character or there was an error
	while (retry) {
		//read a single character
		if ((bytesRead = read(socketid, &data, 1)) < 0) {
			if (errno != EINTR) {
				fprintf(stderr, "Read error: %s\n", strerror(errno));
				delete message;
                return NULL;
			}
			continue;
		}
		
		//if nothing was read, connection ended
		if (bytesRead == 0) {
			break;
		}

		//add to buffer and check exit condition
		message -> add(data);
		if (data == '\n') {
			break;
		}
	}

	//if nothing was read, report an error
	if (message -> currentSize == 0) {
		fprintf(stderr, "Nothing read\n");
		delete message;
		return NULL;
	}

	return message;
}

char* socketRead(const int socketid, const unsigned long long size) {
	//attempt to malloc size bytes of data
	char* message = (char*)malloc(size * sizeof(char));
	if (message == NULL) {
		fprintf(stderr, "Could not malloc %lld bytes\n", size);
		return NULL;
	}
	
	//continue to attempt reading data from the socket
	//until we read in size bytes or there was an error
	char data[BUFFER_SIZE];
	unsigned long long totalRead = 0;
	int bytesRead;
	while (totalRead < size) {
		size_t requestSize = min(BUFFER_SIZE, size - totalRead);
		if ((bytesRead = read(socketid, &data, requestSize)) < 0) {
			if (errno != EINTR) {
				fprintf(stderr, "Read error: %s\n", strerror(errno));
                free(message);
                return NULL;
			}
			continue;
		}

		//if nothing was read, connection ended
		if (bytesRead == 0) {
			break;
		}

		//copy data into the message buffer
		memcpy(message + totalRead, data, bytesRead);
		totalRead += bytesRead;
	}

	//if we didn't end up reading the requested amount, return an error
	if (totalRead != size) {
		fprintf(stderr, "Did not read the amount specified\n");
		free(message);
		return NULL;
	}

	return message;
}

//do we need to handle 0 incoming bytes?
long long getIncomingBytes(const int socketid) {
	Buffer* message = socketGetLine(socketid);
    if (message == NULL) {
        fprintf(stderr, "Error in processing file size\n");
        delete message;
        return -1;
    }

    //Transfer string into a number
    long long fileSize = 0;
    size_t i = 0;
    while (i < message -> currentSize - 1) {
        char ch = message -> data[i];
        if (ch > '9' || ch < '0') {
            fprintf(stderr, "Error: protocol has invalid size data\n");
            delete message;
            return -1;
        }
        fileSize = fileSize * 10 + (ch - '0');
        i++;
    }
    //clean up message
    delete message;
    return fileSize;
}

//Buffer Class Implementation
Buffer::Buffer(unsigned long long size) {
	this -> data = new char[size]();
	this -> currentSize = 0;
	this -> maxSize = size;
}

Buffer::~Buffer() {
	delete [] this -> data;
}

void Buffer::add(const char ch) {
	//if the buffer is at full capacity, create more space
	if (this -> currentSize == this -> maxSize) {
		unsigned int newMaxSize = this -> maxSize * 2 + 1;
		char* newData = (char*)malloc(newMaxSize * sizeof(char));
		//copy old data into new, larger array
		memcpy(newData, this -> data, this -> currentSize);
		//delete old data
		free(this -> data);
		//set pointer to new data location
		this -> data = newData;
		//update max size
		this -> maxSize = newMaxSize;
	}
	this -> data[this -> currentSize] = ch;
	this -> currentSize++;
}