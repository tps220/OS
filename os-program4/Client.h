#pragma once
#include <iostream>
/*
 * help() - Print a help message
 */
void help(char *progname);

/*
 * die() - print an error and exit the program
 */
void die(const char *msg1, const char *msg2);

/*
 * connect_to_server() - open a connection to the server specified by the
 *                       parameters
 */
int connect_to_server(char *server, int port);

/*
 * echo_client() - this is dummy code to show how to read and write on a
 *                 socket when there can be short counts.  The code
 *                 implements an "echo" client.
 */
void echo_client(int fd);

/*
 * put_file() - send a file to the server accessible via the given socket fd
 */
void put_file(int fd, char *put_name);

/*
 * get_file() - get a file from the server accessible via the given socket
 *              fd, and save it according to the save_name
 */
void get_file(int fd, char *get_name, char *save_name);


//Receiving Files
int GET(const int socketid, const char* filename, const bool useEncryption);
int GETC(const int socketid, const char* filename, const bool useEncryption);

//Sending files
int PUTS(const int socketid, const char* filename, const char* protocol, const bool checksum, const bool useEncryption);

//Writing files locally
int writeFile(FILE* fp, char* data, unsigned long long size);

//Encryptoon Globals
const int AES_256_KEY_SIZE = 32; //size of AES key
const int BLOCK_SIZE = 16; //size of blocks that get encrypted
const int BUFSIZE = 1024; //chunk size for reading/writing from files

//Encrpytion Methods
EVP_CIPHER_CTX *get_aes_context(std::string keyfile, bool encrypt);
Buffer* aes_encrypt(EVP_CIPHER_CTX *ctx, FILE *in);
bool aes_decrypt(EVP_CIPHER_CTX *ctx, FILE *out, unsigned char* data, long long size);