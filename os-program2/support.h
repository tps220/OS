#ifndef SUPPORT_H__
#define SUPPORT_H__
#include "structs.h"
#include "stdio.h"

/*
 * Store information about the students who completed the assignment, to
 * simplify the grading process.  This is just a declaration.  The definition
 * is in team.c.
 */
extern struct team_t
{
	char *names;
	char *emails;
} team;
void check_team(char *);

//Open the initial file system, return file descriptor or failure
int openFileSystem(char* name);

//Initializes the boot sector, returns nothing
void initializeBootSector(char* filesys);

//Verify File System, return 1 if valud returns 0 if invalid
int veryifyBootSector(char* filesys);

//Initializes the root directory, returns nothing
void initializeRootDirectory(char* filesys);

//Verify Root Sector specfiically, return 1 if valud returns 0 if invalid
int verifyRootSector(char* filesys);

//Verify File System, return 1 if valud returns 0 if invalid
int verifyFileSystem(char* filesys);

//Create the initial file system and return the file descriptor or failure
char* createFileSystem(char* filename);

//Finds a single free entry in the file system, if successful returns the number, if not it returns 0
unsigned short getOneFreeEntry();

//adds a new file or directory into the contents of a directory by placing the id of the starting cluster number into the directory,
//returns -1 on failure otherwise returns 0 on success
int addToDirectory(char* filesystem, unsigned short directoryId, unsigned short insertId);

//creates a new directory in the filesystem,
//and returns the directory FAT index on success otherwise returns 0 on error
unsigned short createNewDirectory(char* filesystem, unsigned short parentDirectory, char* name, char attirbutes);

//returns a pointer to a certain page
unsigned char* getPagePointer(char* filesystem, int pageNumber);

//Dumps a page number to stdout
void dump(FILE* fp, int pageNumber, char* filesys);

//Dumps the page data into a file
void dumpBinary(FILE* fp, int pageNumber, char* filesys);

//skip n commas from the begining of a data section, used to get past the parent directories
int skipNCommas(char* data, int i, int size, int n);

//search inside a directory for a name, and return the FAT id of that file
unsigned short searchDirectory(unsigned short currentDirectoryId, char* name, char* filesys);

//returns a vector of the directory entries in a file directory
struct Vector* getDirectoryEntries(char* filesys, unsigned short directoryId);

//print the contents of a directory
void lsUser(char* filesys, unsigned short directoryId);

//makes a new directory insdie the given current directory
int mkdirUser(char* name, unsigned short currentDirectoryId, char* filesys);

//print the path to the current file
char* pwdUser(char* filesystem, unsigned short directoryId);

//attempt to change directories, return -1 if name not found, return -2 if name is a file, return 0 otherwise
int cdUser(char* filesys, char* name, unsigned short *currentDirectoryId);

//get the parent directory of the current directory
unsigned short getParentDirectory(char* fileystem, unsigned short directoryId);

//prints total pages used and space ued for actual files
void usage(char* filesystem);

//if a file is found in the current directory, returns that file. otherwise returns 0
unsigned short findFile(char *filesystem, char *filename, unsigned short currentDirectoryId);

//write() function, returns -1 on error that there wasn't enough space left
int writeToFile(char *filename, int amt, char *data, char *fileSystem, unsigned short directory);

//append() function, returns -1 on error that there wasn't enough space left
int appendToFile(char *filename, int amt, char *data, char *fileSystem, unsigned short directory);

//helper method for appending data to the end of a file given the offset to its eof
int appendData(char* filesys, char* data, unsigned int amount, unsigned short id, unsigned int offset, struct File* file);

//print out contents of file
int cat(char* filename, char* filesys, unsigned short directoryId);

//removes an entry from a directory and updates
void updateDirectory(int removeId, unsigned short directoryId, char* filesys);

//resets a directory by saving its header content but deleting all of its data except for . and ..
void resetDirectory(unsigned short directoryId, char* filesys);

//rmdir() function, returns -1 on error
int rmdirUser(char* name, unsigned short* currentDirectoryId, char* filesys);

//recursively removes all contents of the directory or file through depth first search approach
void rmAllFiles(char* filesys, unsigned short current);

//clears all the data in some entry id
void clearAllInfo(char* filesys, unsigned short id);

//rm -rf function
unsigned short rmRF(char* filesys, unsigned short directoryId, char* name);

//removes a file
int rmUser(char* name, unsigned short currentDirectoryId, char* filesys);

//remove range
int rmRange(char* filesys, unsigned short currentDirectoryId, char* name, unsigned int start, unsigned int end);

//get pages used by file or directory
void getpages(char* name, unsigned short currentDirectoryId, char* filesys);

//get range of data from file
void getRange(char* filename, int start, int end, unsigned short currentDirectoryId, char* filesys);

//---------------------------DEBUG HELPER METHODS
void dumpAlloc(char* filesystem, unsigned int id);
void dumpDirectory(char* filesystem, unsigned int id);

//Data structure helpers for traversing
struct ListNode {
	char* name;
	struct ListNode* next;
};
struct ListNode* createAtHead(char* name, struct ListNode* head);
struct ListNode* destructList(struct ListNode* root);

struct Vector {
	unsigned int maxSize;
	unsigned int currentSize;
	unsigned short* data;
};
struct Vector* constructVector(unsigned int size);
void insertVector(struct Vector* array, unsigned short id);
int getIndex(struct Vector* array, unsigned short id);
void destructVector(struct Vector* array);

struct DataBlock {
	unsigned int maxSize;
	unsigned int currentSize;
	char* data;
};

struct DataBlock* constructDataBlock(unsigned int size);
void insertDataBlock(struct DataBlock* array, char* segment);
void destructDataBlock(struct DataBlock* array);

#endif
