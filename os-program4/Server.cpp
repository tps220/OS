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
#include "support.h"
#include "Server.h"
#include <thread>
#include <vector>

LRU_Cache *cache;
std::queue<int> jobs;
std::mutex q_lock;
std::condition_variable q_cv;

void help(char *progname) {
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("Initiate a network file server\n");
    printf("  -m    number of threads to use (multithreading mode)\n");
    printf("  -c    enable checksum mode\n");
    printf("  -e    use AES encryption\n");
    printf("  -l    number of entries in the LRU cache\n");
    printf("  -p    port on which to listen for connections\n");
}

void die(const char *msg1, char *msg2) {
    fprintf(stderr, "%s, %s\n", msg1, msg2);
    exit(0);
}

//Doubly Linked List Implementation
DoublyLinkedList::DoublyLinkedList() {
    this -> head = NULL;
    this -> tail = NULL;
}

DoublyLinkedList::~DoublyLinkedList() {
    Node* runner = this -> head;
    while (runner != NULL) {
        Node* temp = runner;
        runner = runner -> next;
        delete temp;
    }
}

void DoublyLinkedList::push(Node* element) {
    if (this -> head == NULL && this -> tail == NULL) {
        this -> head = this -> tail = element;
        return;
    }
    element -> previous = NULL;
    element -> next = this -> head;
    this -> head -> previous = element;
    this -> head = element;
}

void DoublyLinkedList::moveToFront(Node* element) {
     if (this -> head == element) {
        return;
    }
    else if (element == this -> tail) {
        this -> tail = this -> tail -> previous;
        this -> tail -> next = NULL;
    }
    else {
        element -> next -> previous = element -> previous;
        element -> previous -> next = element -> next;
    }
    push(element);
}

void DoublyLinkedList::pop() {
    if (this -> head == NULL && this -> tail == NULL) {
        return;
    }
    else if (this -> head == this -> tail) {
        delete this -> head;
        this -> head = this -> tail = NULL;
        return;
    }
    Node* temp = this -> tail;
    this -> tail = this -> tail -> previous;
    this -> tail -> next = NULL;
    delete temp;
}

Node* DoublyLinkedList::peek() {
    return this -> tail;
}

//LRU Cache Implementation
LRU_Cache::LRU_Cache(unsigned int capacity) : maxSize(capacity) {
    this -> currentSize = 0;
    this -> ll = new DoublyLinkedList();
}

LRU_Cache::~LRU_Cache() {
    delete this -> ll;
}

std::pair<char*, long long> LRU_Cache::get(std::string key) {
    std::lock_guard<std::mutex> lk(this -> m);
    //if the key doesn't exist
    if (this -> map.find(key) == this -> map.end()) {
        return std::pair<char*, long long>(NULL, -1);
    }
    Node* candidate = this -> map[key];
    //copy data into the retval
    char* data = (char*)malloc(candidate -> fileSize * sizeof(char));
    memcpy(data, candidate -> data, candidate -> fileSize);
    std::pair<char*, long long> retval(data, candidate -> fileSize);
    //move to front
    this -> ll -> moveToFront(candidate);
    return retval;
}

void LRU_Cache::put(std::string key, char* data, long long fileSize) {
    std::lock_guard<std::mutex> lk(this -> m);
    //if the key does exist, then place the new data 
    //into the cache and move the Node to the head of the list
    if (this -> map.find(key) != this -> map.end()) {
        Node* candidate = this -> map[key];
        free(candidate -> data);
        candidate -> data = data;
        candidate -> fileSize = fileSize;
        this -> ll -> moveToFront(candidate);
        return;
    }
    //if we've hit the threshold of the cache, then evict the 
    //last element in preparation of pushing the new created node
    else if (this -> currentSize == this -> maxSize) {
        std::string evictionKey = this -> ll -> peek() -> key;
        this -> map.erase(evictionKey);
        this -> ll -> pop();
        this -> currentSize--;
    }
    //create the new node and place it at the front
    Node* candidate = new Node(key, data, fileSize);
    this -> ll -> push(candidate);
    this -> map[key] = candidate;
    this -> currentSize++;
}

//----------Unbouned Queue Implementation-------//
int push(int socketfd) {
    std::lock_guard<std::mutex> lk(q_lock);
    jobs.push(socketfd);
    q_cv.notify_one();
    return 1;
}

void connection_handler() {
    while (1) {
        std::unique_lock<std::mutex> lk(q_lock);
        q_cv.wait(lk, []{ return !jobs.empty(); });
        int connectionfd = jobs.front();
        jobs.pop();
        q_cv.notify_one();
        lk.unlock();
        file_server(connectionfd, 0);
        if (close(connectionfd) < 0) {
            die("Error in thread close(): ", strerror(errno));
        }
    }
}

void handleMessage(const int socketid) {
    Buffer* protocol = socketGetLine(socketid);
    
    //if there was an error reading from socket, return -1
    if (protocol == NULL) {
        sendErrorMessage(socketid, "Error: Could not read from socket");
        return;
    }
    //minimum size: GET/PUT(c) + _ + {name} + \n == 6 chars
    else if (protocol -> currentSize < 6) {
        sendErrorMessage(socketid, "Error: Invalid Message Size");
        delete protocol;
        return;
    }
    //must have space separating request from filename
    else if (protocol -> data[3] != ' ' && protocol -> data[4] != ' ') {
        sendErrorMessage(socketid, "Error: Invalid Protocol\n");
        delete protocol;
        return;
    }
    //replace the \n character at the end of the message with the null character
    protocol -> data[protocol -> currentSize - 1] = '\0';
    fprintf(stderr, "Received Protocol: %s\n", protocol -> data);

    //separate protocol (length of 3) and filename into two separate strings
    char* request = protocol -> data;
    char* filename;
    if (protocol -> data[3] == ' ') {
        protocol -> data[3] = '\0';
        filename = protocol -> data + 4;
    }
    else {
        protocol -> data[4] = '\0';
        filename = protocol -> data + 5;
    }

    //handle the clients requests
    //GET
    if (strcmp(request, "GET") == 0) {
        //if handleGetRequest succeeds, will have sent a message to client
        if (GETS(socketid, filename, "OK", false) == -1) {
            //else send an error message
            sendErrorMessage(socketid, "Error: Could not handle GET request\n");
        }
    }
    //PUT
    else if (strcmp(request, "PUT") == 0) {
        if (PUT(socketid, filename) == -1) {
            sendErrorMessage(socketid, "Error: Could not handle PUT request");
        }
        else {
            sendOkMessage(socketid);
        }
    }
    else if (strcmp(request, "GETC") == 0) {
        //if handleGetRequest succeeds, will have sent a message to client
        if (GETS(socketid, filename, "OKC", true) == -1) {
            //else send an error message
            sendErrorMessage(socketid, "Error: Could not handle GETC request\n");
        }
    }
    //PUT
    else if (strcmp(request, "PUTC") == 0) {
        if (PUTC(socketid, filename) == -1) {
            sendErrorMessage(socketid, "Error: Could not handle PUTC request");
        }
        else {
            sendOkMessage(socketid);
        }
    }
    //Invalid
    else {
        fprintf(stderr, "invalid protocol\n");
        sendErrorMessage(socketid, "Error: Invalid Message");
    }
    delete protocol;
}

void sendOkMessage(const int socketid) {
    printf("Sending back okay!\n");
    char response[BUFFER_SIZE];
    size_t length = sprintf(response, "OK\n");
    streamData(socketid, response, length);
}

void sendErrorMessage(const int socketid, const char* message) {
    char error[BUFFER_SIZE];
    size_t length = sprintf(error, "%s\n", message);
    streamData(socketid, error, length);
}

int readFile(char* destination, const long long size, FILE* fp, const char* filename) {
    //retrieve key and data
    std::string key(filename);
    char* data = (char*)malloc(size * sizeof(char));
    if (fread(data, sizeof(char), size, fp) != size) {
        free(data);
        return -1;
    }
    cache -> put(key, data, size);
    //finally copy the data into the destination
    memcpy(destination, data, size);
    return 0;
}

int writeFile(FILE* fp, char* data, const long long size, const char* filename) {
    //write data into the file
    if (fwrite(data, sizeof(char), size, fp) != size) {
        fprintf(stderr, "Could not write to file\n");
        return -1;
    }
    //put into cache
    char* cache_data = (char*)malloc(size * sizeof(char));
    memcpy(cache_data, data, size);
    std::string key(filename);
    cache -> put(key, cache_data, size);
    return 0;
}

//PUT
int PUT(const int socketid, const char* filename) {
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
    int retval = writeFile(fp, data, fileSize, filename);
    fclose(fp);
    free(data);
    return retval;
}

//PUTC
int PUTC(const int socketid, const char* filename) {
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
    int retval = writeFile(fp, data, fileSize, filename);
    fclose(fp);
    free(data);
    return retval;
}

int GETS(const int socketid, const char* filename, const char* protocol, const bool checksum) {
    //Attempt to retrieve file size and data from the cache
    long long size = 0;
    std::pair<char*, long long> file = cache -> get(std::string(filename));
    if (file.first != NULL) {
        size = file.second;
    }
    else {
        struct stat fileInfo;
        if (stat(filename, &fileInfo) == -1) {
            fprintf(stderr, "File doesn't exist or user doesn't have correct permissions\n");
            return -1;
        }
        size = fileInfo.st_size;
    }

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

    //Copy protocol + filename into message
    size_t split = strlen(protocol);
    memcpy(message, protocol, split);
    message[split] = ' ';
    memcpy(message + split + 1, filename, headerLength - split - 2);
    message[headerLength - 1] = '\n';

    //Copy #bytes into message at the end of header message
    memcpy(message + headerLength, fileSize, fileSizeLength);
    
    //Copy in the file content after header, file size, and the hash data
    //If file contents are from the cache, read from there and free used memory
    if (file.first != NULL) {
        memcpy(message + headerLength + fileSizeLength + hashLength, file.first, size);
        free(file.first);
    }
    //Read from the file
    else {
        FILE* fp = fopen(filename, "rb");
        if (fp == NULL) {
            fprintf(stderr, "Could not open file: %s\n", filename);
            free(message);
            return -1;
        }
        //readFile() copies in data + places it into cache
        if (readFile(message + headerLength + fileSizeLength + hashLength, size, fp, filename) < 0) {
            fprintf(stderr, "Error reading from file\n");
            fclose(fp);
            free(message);
            return -1;
        }
        fclose(fp);
    }
    message[messageLength - 1] = '\n';

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

/*
 * open_server_socket() - Open a listening socket and return its file
 *                        descriptor, or terminate the program
 */
int open_server_socket(int port) {
    int                listenfd;    /* the server's listening file descriptor */
    struct sockaddr_in addrs;       /* describes which clients we'll accept */
    int                optval = 1;  /* for configuring the socket */

    /* Create a socket descriptor */
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        die("Error creating socket: ", strerror(errno));
    }

    /* Eliminates "Address already in use" error from bind. */
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
    {
        die("Error configuring socket: ", strerror(errno));
    }

    /* Listenfd will be an endpoint for all requests to the port from any IP
       address */
    bzero((char *) &addrs, sizeof(addrs));
    addrs.sin_family = AF_INET;
    addrs.sin_addr.s_addr = htonl(INADDR_ANY);
    addrs.sin_port = htons((unsigned short)port);
    if(bind(listenfd, (struct sockaddr *)&addrs, sizeof(addrs)) < 0)
    {
        die("Error in bind(): ", strerror(errno));
    }

    /* Make it a listening socket ready to accept connection requests */
    if(listen(listenfd, 1024) < 0)  // backlog of 1024
    {
        die("Error in listen(): ", strerror(errno));
    }

    return listenfd;
}

/*
 * handle_requests() - given a listening file descriptor, continually wait
 *                     for a request to come in, and when it arrives, pass it
 *                     to service_function.  Note that this is not a
 *                     multi-threaded server.
 */
void handle_requests(int listenfd, void (*service_function)(int, int), int lru_size, bool multithread, bool useChecksum, bool useEncryption) {
    
    //initialize LRU cache
    cache = new LRU_Cache(lru_size);

    while(1)
    {
        /* block until we get a connection */
        struct sockaddr_in clientaddr;
        memset(&clientaddr, 0, sizeof(sockaddr_in));
        socklen_t clientlen = sizeof(clientaddr);
        int connfd;
        if((connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen)) < 0)
        {
            die("Error in accept(): ", strerror(errno));
        }
	
        /* print some info about the connection */
        struct hostent *hp;
        hp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if(hp == NULL)
        {
            fprintf(stderr, "DNS error in gethostbyaddr() %d\n", h_errno);
            exit(0);
        }
        char *haddrp = inet_ntoa(clientaddr.sin_addr);
        printf("server connected to %s (%s)\n", hp->h_name, haddrp);

        if (multithread) {
            push(connfd);
        }
        else {
            service_function(connfd, lru_size);
            if (close(connfd) < 0) {
                die("Error in close(): ", strerror(errno));
            }
        }
    }
}

/*
 * file_server() - Read a request from a socket, satisfy the request, and
 *                 then close the connection.
 */

void file_server(int connfd, int lru_size) {
    handleMessage(connfd);
}

/*
 * main() - parse command line, create a socket, handle requests
 */
int main(int argc, char **argv)
{
    /* for getopt */
    long opt;
    int  lru_size = 10;
    int  port     = 9000;
    int  multithread = 1;
    bool useChecksum = false;
    bool useEncryption = false;

    check_team(argv[0]);

    /* parse the command-line options.  They are 'p' for port number,  */
    /* and 'l' for lru cache size, 'm' for multi-threaded.  'h' is also supported. */
    while((opt = getopt(argc, argv, "hcem:l:p:")) != -1)
    {
        switch(opt)
        {
          case 'h': help(argv[0]); break;
          case 'c': useChecksum = true; break;
          case 'e': useEncryption = true; break;
          case 'l': lru_size = atoi(optarg); break;
          case 'm': multithread = atoi(optarg); break;
          case 'p': port = atoi(optarg); break;
        }
    }

    std::vector<std::thread> threads;
    if (multithread > 1) {
        for (int i = 0; i < multithread; i++) {
            threads.push_back(std::thread(connection_handler));
        }
    }
    
    /* open a socket, and start handling requests */
    int fd = open_server_socket(port);
    handle_requests(fd, file_server, lru_size, multithread > 1, useChecksum, useEncryption);

    for (auto& thread : threads){
        thread.join();
    }
    
    if (close(fd) < 0) {
            die("Error in close(): ", strerror(errno));
    }
    
    exit(0);
}
