#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "support.h"
#include "Client.h"

void help(char *progname)
{
	printf("Usage: %s [OPTIONS]\n", progname);
	printf("Perform a PUT or a GET from a network file server\n");
	printf("  -c    Use checksums\n");
	printf("  -e    Use encryption\n");
	printf("  -P    PUT file indicated by parameter\n");
	printf("  -G    GET file indicated by parameter\n");
	printf("  -s    server info (IP or hostname)\n");
	printf("  -p    port on which to contact server\n");
	printf("  -S    for GETs, name to use when saving file locally\n");
}

void die(const char *msg1, const char *msg2)
{
	fprintf(stderr, "%s, %s\n", msg1, msg2);
	exit(0);
}

/*
 * connect_to_server() - open a connection to the server specified by the
 *                       parameters
 */
int connect_to_server(char *server, int port)
{
	int clientfd;
	struct hostent *hp;
	struct sockaddr_in serveraddr;
	char errbuf[256];                                   /* for errors */

	/* create a socket */
	if((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		die("Error creating socket: ", strerror(errno));
	}

	/* Fill in the server's IP address and port */
	if((hp = gethostbyname(server)) == NULL)
	{
		printf(errbuf, "%d", h_errno);
		die("DNS error: DNS error ", errbuf);
	}
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr_list[0], (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	/* connect */
	if(connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		die("Error connecting: ", strerror(errno));
	}
	return clientfd;
}

/*
 * put_file() - send a file to the server accessible via the given socket fd
 */
void put_file(int fd, char *fileName, bool useChecksum, bool useEncryption) {
	//send file over server
	int retval = 0;
	if (useChecksum) {
		retval = PUTS(fd, fileName, "PUTC", useChecksum, useEncryption);
	}
	else {
		retval = PUTS(fd, fileName, "PUT", useChecksum, useEncryption);
	}

	//ensure the PUT call worked
	if (retval == -1) { 
		fprintf(stderr, "Could not send info to server");
		return; 
	}
	
	//retrieve and report message
	Buffer* response = socketGetLine(fd);
	if (response == NULL) {
		fprintf(stderr, "Did not receive a response\n");
		return;
	}
	//print responnse
	response -> data[response -> currentSize - 1] = '\0';
	printf("%s\n", response -> data);
	delete response;
	return;
}

/*
 * get_file() - get a file from the server accessible via the given socket
 *              fd, and save it according to the save_name
 */
void get_file(const int fd, char *get_name, char *save_name, const bool useChecksum, const bool useEncryption) {
	//Create header content
	const char* protocol;
	if (useChecksum) {
		protocol = "GETC";
	}
	else {
		protocol = "GET";
	}
	const int messageLength = strlen(get_name) + strlen(protocol) + 2; //protocol + _ + get_name + \n
	char* message = (char*)malloc(messageLength * sizeof(char));

	//Copy protocol + filename into message
	size_t split = strlen(protocol);
    memcpy(message, protocol, split);
    message[split] = ' ';
    memcpy(message + split + 1, get_name, messageLength - split - 2);
    message[messageLength - 1] = '\n';

	//Send information to server
	streamData(fd, message, messageLength);
	free(message);


	fprintf(stderr, "Waiting for response\n");
	//retrieve and handle message
	Buffer* response = socketGetLine(fd);
	if (response == NULL) {
		fprintf(stderr, "Server failed to send response\n");
		return;
	}
	else if (response -> currentSize < 6) {
		fprintf(stderr, "Server failed to send valid response\n");
		delete response;
		return;
	}
	response -> data[response -> currentSize - 1] = '\0';

	fprintf(stderr, "Received response %s\n", response -> data);
	//if a valid response
	if (response -> data[0] == 'O' && response -> data[1] == 'K' && response -> data[2] == ' ') {
		//process filename
		char* filename = response -> data + 3;
		if (strcmp(filename, get_name) != 0) {
			fprintf(stderr, "Did not get same file that was requested\n");
		}
		//handle request
		else if (GET(fd, save_name, useEncryption) == -1) {
			fprintf(stderr, "Server did not send proper informaiton\n");
		}
	}
	else if (response -> data[0] == 'O' && response -> data[1] == 'K' && response -> data[2] == 'C' && response -> data[3] == ' ') {
		//process filename
		char* filename = response -> data + 4;
		if (strcmp(filename, get_name) != 0) {
			fprintf(stderr, "Did not get same file that was requested\n");
		}
		//handle request
		else if (GETC(fd, save_name, useEncryption) == -1) {
			fprintf(stderr, "Server did not send proper informaiton\n");
		}
	}
	delete response;
}

int writeFile(FILE* fp, char* data, const unsigned long long size, const bool useEncryption) {
	if (useEncryption) {
		EVP_CIPHER_CTX *ctx = get_aes_context("AESKEY", false);
		bool retval = aes_decrypt(ctx, fp, (unsigned char*)data, size);
		EVP_CIPHER_CTX_cleanup(ctx);
		if (retval) {
			return 0;
		}
		return -1;
	}
	else if (fwrite(data, sizeof(char), size, fp) != size) {
        fprintf(stderr, "Could not write to file\n");
        return -1;
    }
    return 0;
}

//GET
int GET(const int socketid, const char* filename, const bool useEncryption) {
    //Process the incoming # bytes == fileSize
    long long fileSize = getIncomingBytes(socketid);
    if (fileSize == -1) {
        return -1;
    }
    
    //Attempt to read the requested # bytes + \n
    char* data = socketRead(socketid, fileSize + 1);
    if (data == NULL) {
        return -1; 
    }

    //attempt to open file
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Error opening requested file\n");
        free(data);
        return -1;
    }

    //attempt to write all of the data to the file
    int retval = writeFile(fp, data, fileSize, useEncryption);
    fclose(fp);
    free(data);
    return retval;
}

//GETC
int GETC(const int socketid, const char* filename, const bool useEncryption) {
    //Process the incoming # bytes == fileSize
    long long fileSize = getIncomingBytes(socketid);
    if (fileSize == -1) {
        return -1;
    }
    
    //Process the checksum
    char* message = socketRead(socketid, MD5_DIGEST_LENGTH + 1);
    if (message == NULL) {
        return -1;
    }
    //replace \n character with null character
    message[MD5_DIGEST_LENGTH] = '\0';

    //Attempt to read the requested # bytes + \n
    char* data = socketRead(socketid, fileSize + 1);
    if (data == NULL) { 
        free(message);
        return -1; 
    }

    //create and validate the checksum to ensure correctness
    unsigned char hash[MD5_DIGEST_LENGTH + 1];
    MD5_CTX md;
    MD5_Init(&md);
    MD5_Update(&md, data, fileSize);
    MD5_Final(hash, &md);
    hash[MD5_DIGEST_LENGTH] = '\0';
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        if (hash[i] != (unsigned char)message[i]) {
            free(message);
            free(data);
            fprintf(stderr, "Invalid hash\n");
            return -1;
        }
    }
    free(message);

    //attempt to open file
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Error opening requested file\n");
        free(data);
        return -1;
    }

    //attempt to write all of the data to the file
    int retval = writeFile(fp, data, fileSize, useEncryption);
    fclose(fp);
    free(data);
    return retval;
}

int PUTS(const int socketid, const char* filename, const char* protocol, const bool checksum, const bool useEncryption) {
    //Open file
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file: %s\n", filename);
        return -1;
    }
	
	char* data;
	long long size;
	if (useEncryption) {
		EVP_CIPHER_CTX *ctx = get_aes_context("AESKEY", true);
		Buffer* encBuffer = aes_encrypt(ctx, fp);
		if (encBuffer == NULL) {
			fclose(fp);
			EVP_CIPHER_CTX_cleanup(ctx);
			return -1;
		}
		size = encBuffer -> currentSize;
		data = (char*)malloc(size * sizeof(char));
		memcpy(data, encBuffer -> data, size);
		delete encBuffer;
		EVP_CIPHER_CTX_cleanup(ctx);
	}
	else {
		  //Get file size
    	struct stat fileInfo;
    	if (stat(filename, &fileInfo) == -1) {
        	fprintf(stderr, "Couldn't read size information\n");
        	fclose(fp);
        	return -1;
    	}
    	size = fileInfo.st_size;
		  data = (char*)malloc(size * sizeof(char));
		  if (fread(data, sizeof(char), size, fp) != size) {
        	fprintf(stderr, "Error reading from file\n");
        	fclose(fp);
        	free(data);
        	return -1;
    	}
	}
	fclose(fp);

    //Create header content
    const int headerLength = strlen(protocol) + strlen(filename) + 2; //strlen(protocol) + _ + strlen(filename) + \n
    
    //If checksum is true, then create room for hashing in the message
    int hashLength = 0;
    if (checksum) {
        hashLength = MD5_DIGEST_LENGTH + 1; //hash + \n character
    }

    //Create size content
    char fileSize[32];
    sprintf(fileSize, "%lld\n", size);
    const int fileSizeLength = strlen(fileSize); //strlen(filesize) which includes the \n character
    
    //Prepare Message
    const unsigned long long messageLength = headerLength + fileSizeLength + hashLength + size + 1; //headerLength + fileSizeLength + hashLength + #bytes in file + \n
    char* message = (char*)malloc(messageLength * sizeof(char));
    if (message == NULL) {
        fprintf(stderr, "Malloc request for %d message size failed\n", messageLength);
        return -1;
    }

    //Copy protocol + filename into messdataage
    size_t split = strlen(protocol);
    memcpy(message, protocol, split);
    message[split] = ' ';
    memcpy(message + split + 1, filename, headerLength - split - 2);
    message[headerLength - 1] = '\n';

    //Copy #bytes into message at the end of header message
    memcpy(message + headerLength, fileSize, fileSizeLength);
    
    //Copy in the file content after header and file size
    memcpy(message + headerLength + fileSizeLength + hashLength, data, size);
    message[messageLength - 1] = '\n';
    free(data);

    //copy hash into file if checksum is true
    if (checksum) {
        MD5_CTX md;
        MD5_Init(&md);
        MD5_Update(&md, message + headerLength + fileSizeLength + hashLength, size);
        MD5_Final((unsigned char*)message + headerLength + fileSizeLength, &md);
        message[headerLength + fileSizeLength + hashLength - 1] = '\n';
    }

    //send information to client, and clear data
    streamData(socketid, message, messageLength);
    free(message);
    return 0;
}


//Encryption
//Credit These Functions + Descriptions to Professor Spear's CSE303 Tutorial's Pages,
//Only Slight Modifications Applied Here
/**
 * Produce an AES context that can be used for encrypting or decrypting.  Set
 * the AES key from the provided file.
 *
 * @param keyfile The name of the file holding the AES key
 * @param encrypt true if the context will be used to encrypt, false otherwise
 *
 * @return A properly configured AES context
 */
EVP_CIPHER_CTX *get_aes_context(std::string keyfile, bool encrypt) {
 	//Open the key file and read the key and iv
  	FILE *file = fopen(keyfile.c_str(), "rb");
  	if (!file) {
    	perror("Error opening keyfile");
    	exit(0);
  	}
  	unsigned char key[AES_256_KEY_SIZE], iv[BLOCK_SIZE];
  	int num_read = fread(key, sizeof(unsigned char), sizeof(key), file);
  	num_read += fread(iv, sizeof(unsigned char), sizeof(iv), file);
  	fclose(file);
  	if (num_read != AES_256_KEY_SIZE + BLOCK_SIZE) {
    	fprintf(stderr, "Error reading keyfile");
    	exit(0);
  	}

  	//Create and initialize a context for the AES operations we are going to do
  	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  	if (ctx == nullptr) {
    	fprintf(stderr, "Error: OpenSSL couldn't create context: %s\n", 
    				 	 ERR_error_string(ERR_get_error(), nullptr));
    	exit(0);
  	}

  	//Make sure the key and iv lengths we have up above are valid
  	if (!EVP_CipherInit_ex(ctx, EVP_aes_256_cbc(), nullptr, nullptr, nullptr, 1)) {
    	fprintf(stderr, "Error: OpenSSL couldn't initialize context: %s\n", 
    				 	 ERR_error_string(ERR_get_error(), nullptr));
    	exit(0);
  	}
  	OPENSSL_assert(EVP_CIPHER_CTX_key_length(ctx) == AES_256_KEY_SIZE);
  	OPENSSL_assert(EVP_CIPHER_CTX_iv_length(ctx) == BLOCK_SIZE);

  	//Set the key and iv on the AES context, and set the mode to encrypt or decrypt
  	if (!EVP_CipherInit_ex(ctx, nullptr, nullptr, key, iv, encrypt ? 1 : 0)) {
    	fprintf(stderr, "Error: OpenSSL couldn't re-init context: %s\n",
            		 	ERR_error_string(ERR_get_error(), nullptr));
    	EVP_CIPHER_CTX_cleanup(ctx);
    	exit(0);
  	}

  	return ctx;
}

/**
 * Take an input file and run the AES algorithm on it to produce an output file.
 * This can be used for either encryption or decryption, depending on how ctx is
 * configured.
 *
 * @param ctx A fully-configured symmetric cipher object for managing the
 *            encryption or decryption
 * @param in  the file to read
 * @param out the file to populate with the result of the AES algorithm
 */
Buffer* aes_encrypt(EVP_CIPHER_CTX *ctx, FILE *in) {
 	//figure out the block size that AES is going to use
  	int cipher_block_size = EVP_CIPHER_block_size(EVP_CIPHER_CTX_cipher(ctx));
  	Buffer* buffer = new Buffer(1024);

  	//Set up a buffer where AES puts crypted bits. Since the last block is
  	//special, we need this outside the loop.
  	unsigned char out_buf[BUFSIZE + cipher_block_size];
  	int out_len;

  	//Read blocks from the file and crypt them:
  	while (true) {
    	//read from file
    	unsigned char in_buf[BUFSIZE];
    	int num_bytes_read = fread(in_buf, sizeof(unsigned char), BUFSIZE, in);
    	if (ferror(in)) {
      		perror("Error in fread()");
      		delete buffer;
      		return NULL;
    	}
    	//crypt in_buf into out_buf
    	if (!EVP_CipherUpdate(ctx, out_buf, &out_len, in_buf, num_bytes_read)) {
      		fprintf(stderr, "Error in EVP_CipherUpdate: %s\n",
            				 ERR_error_string(ERR_get_error(), nullptr));
      		delete buffer;
      		return NULL;
    	}
    	//write crypted bytes to buffer
    	for (int i = 0; i < out_len; i++) {
    		buffer -> add(out_buf[i]);
    	}
    	//stop on EOF
    	if (num_bytes_read < BUFSIZE) {
      		break;
    	}
  	}

  	// The final block needs special attention!
  	if (!EVP_CipherFinal_ex(ctx, out_buf, &out_len)) {
    	fprintf(stderr, "Error in EVP_CipherFinal_ex: %s\n",
            			 ERR_error_string(ERR_get_error(), nullptr));
    	delete buffer;
    	return NULL;
  	}
  	//write crypted bytes to buffer
  	for (int i = 0; i < out_len; i++) {
    	buffer -> add(out_buf[i]);
  	}
  	return buffer;
}

inline long long min(long long a, long long b) {
	if (a < b) {
		return a;
	}
	return b;
}

bool aes_decrypt(EVP_CIPHER_CTX *ctx, FILE *out, unsigned char* data, long long size) {
  	//Figure out the block size that AES is going to use
  	int cipher_block_size = EVP_CIPHER_block_size(EVP_CIPHER_CTX_cipher(ctx));

  	//Set up a buffer where AES puts crypted bits.  Since the last block is
  	//special, we need this outside the loop.
  	unsigned char out_buf[BUFSIZE + cipher_block_size];
  	int out_len;
  	long long totalBytesRead = 0;
  	//Read blocks from the buffer and decrypt them:
  	while (totalBytesRead < size) {
    	//read from file
    	unsigned char in_buf[BUFSIZE];
    	long long num_bytes_read = min(BUFSIZE, size - totalBytesRead);
    	memcpy(in_buf, data + totalBytesRead, num_bytes_read);
    	totalBytesRead += num_bytes_read;
    	// crypt in_buf into out_buf
    	if (!EVP_CipherUpdate(ctx, out_buf, &out_len, in_buf, num_bytes_read)) {
      		fprintf(stderr, "Error in EVP_CipherUpdate: %s\n",
              				 ERR_error_string(ERR_get_error(), nullptr));
      		return false;
    	}
    	// write crypted bytes to file
    	fwrite(out_buf, sizeof(unsigned char), out_len, out);
    	if (ferror(out)) {
      		perror("Error in fwrite()");
      		return false;
    	}
  	}

  	//The final block needs special attention!
  	if (!EVP_CipherFinal_ex(ctx, out_buf, &out_len)) {
    	fprintf(stderr, "Error in EVP_CipherFinal_ex: %s\n",
            			 ERR_error_string(ERR_get_error(), nullptr));
    	return false;
  	}
  	fwrite(out_buf, sizeof(unsigned char), out_len, out);
  	if (ferror(out)) {
    	perror("Error in fwrite");
    	return false;
  	}
  	return true;
}

/*
 * main() - parse command line, open a socket, transfer a file
 */
int main(int argc, char **argv) {
	/* for getopt */
	long  opt;
	char *server = NULL;
	char *put_name = NULL;
	char *get_name = NULL;
	int   port;
	char *save_name = NULL;
	bool  useChecksum = false;
	bool  useEncryption = false;

	check_team(argv[0]);

	/* parse the command-line options. */
	while((opt = getopt(argc, argv, "hces:P:G:S:p:")) != -1) {
		switch(opt) {
			case 'h': help(argv[0]); break;
			case 'c': useChecksum = true; break;
			case 'e': useEncryption = true; break;
			case 's': server = optarg; break;
			case 'P': put_name = optarg; break;
			case 'G': get_name = optarg; break;
			case 'S': save_name = optarg; break;
			case 'p': port = atoi(optarg); break;
		}
	}

	if (server == NULL) {
		fprintf(stderr, "Did not provide a server name / address!\n");
		exit(1);
	}
  if (useEncryption) {
    struct stat fileInfo;
    if (stat("AESKEY", &fileInfo) == -1) {
        fprintf(stderr, "No AESKEY File for encryption, can't proceed\n");
        exit(1);
    }
  }
	//open a connection to the server
	int fd = connect_to_server(server, port);

	//put or get, as appropriate
	if (put_name) {
		put_file(fd, put_name, useChecksum, useEncryption);
	}
	else if (get_name) {
		if (save_name == NULL) {
			fprintf(stderr, "You dind't provide a save name, so we will use the get_name by default\n");
			save_name = get_name;
		}
		get_file(fd, get_name, save_name, useChecksum, useEncryption);
	}

	/* close the socket */
	int rc;
	if((rc = close(fd)) < 0) {
		die("Close error: ", strerror(errno));
	}
	exit(0);
}
