#ifndef SUPPORT_H__
#define SUPPORT_H__

#define BUFFER_SIZE 8196

/*
 * Store information about the team who completed the assignment, to
 * simplify the grading process.  This is just a declaration.  The definition
 * is in team.c.
 */
extern struct team_t {
	const char *name1;
	const char *email1;
	const char *name2;
	const char *email2;
	const char *name3;
	const char *email3;
} team;
void check_team(char *);

//---------------Cross Domain Helper Functions-------------//

//The Object Buffer acts as an array for streams of information
class Buffer {
public:
	//Data fields
	char* data;
	unsigned long long currentSize;
	unsigned long long maxSize;
	
	//Constructors and destructors
	Buffer(unsigned long long size);
	~Buffer();

	//driver functions
	void add(const char ch);
};

//sends data over the socket
//returns 0 on success, otherwise -1 on failure
int streamData(const int socketid, const void* data, const long long size);

//receives and collects a line of data sent by a socket
//stops on \n char and places trust in client messages
//returs a valid Buffer object on success,
//otherwise returns NULL
Buffer* socketGetLine(const int socketid);

//Attempts to read size bytes from the socket
//returns a valid char* on succes
//otherwise returns NULL
char* socketRead(const int socketid, const unsigned long long size);

//gets the # incoming bytes from PUT/GET messages on server/client respectively
long long getIncomingBytes(const int socketid);


#endif