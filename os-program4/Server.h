#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/md5.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include "support.h"
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

//Spear functions
void help(char *progname);
void die(const char *msg1, char *msg2);
int open_server_socket(int port);
void handle_requests(int listenfd, void (*service_function)(int, int), int param, bool multithread);
void file_server(int connfd, int lru_size);


//Student Functions
//handles incoming messages
void handleMessage(const int socketid);
//send an error message
void sendErrorMessage(const int socketid, const char* protocol);
//sends an OK message
void sendOkMessage(const int scoketid);
//write file
int writeFile(FILE* fp, char* data, const long long size, const char* filename);
//read file
int readFile(char* destination, const long long size, FILE* fp, const char* filename);
//handles PUT requset
int PUT(const int socketid, const char* filename);
//handles PUTC request
int PUTC(const int socketid, const char* filename);
//handle both GET and GETC requests
int GETS(const int socketid, const char* filename, const char* protocol, bool checksum);


//--------------------LRU Cache Design-------------//
//In order to create O(1) lookup time and O(1) insertion/deletion,
//this LRU cache couples a Doubly Linked List and a Hash Map of Nodes
//
//When we access a pre-existing element, we move it to the front of the list in O(1) time
//
//When we want to add a new element to the place it at the front of the list
//
//When we want to remove an element from the Cache because the size has become too large
//we evict the data by either popping it off the list
//
//When we want to update an element, and it already exists, we copy in the new information
//and move it to the front of the list


//Node Definiton for the LRU Cache
struct Node {
	std::string key;
	char* data;
	long long fileSize;
	Node* next;
	Node* previous;
	Node(std::string k, char* d, long long size) : key(k), data(d), fileSize(size), next(NULL), previous(NULL) {}
	~Node() { free(this -> data); }
};


//Doubly Linked List Helper
class DoublyLinkedList {
public:
	//constructor + destructor
	DoublyLinkedList();
	~DoublyLinkedList();

	//drvier functions
	void push(Node* element);
	void moveToFront(Node* element);
	void pop();
	Node* peek();

private:
	//data fields
	Node* head;
	Node* tail;
};


//LRU Cache Object
class LRU_Cache {
public:
	//constructor and destructor
	LRU_Cache(unsigned int maxSize);
	~LRU_Cache();

	//driver functions
	std::pair<char*, long long> get(std::string key);
	void put(std::string key, char* data, long long fileSize);

private:
	const unsigned int maxSize;
	unsigned int currentSize;
	DoublyLinkedList* ll;
	std::unordered_map<std::string, Node*> map;
	std::mutex m;
};


//---------Unbounded, Single Producer Multiple Consumer Queue----------//
int push(int socketfd);
void connection_handler();