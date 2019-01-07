#ifndef STRUCTS_H
#define STRUCTS_H
//MACRO definitions, try to create as many of these for platform-wide variables
#define DIRECTORY_OFFSET 32
#define PAGE_SIZE 512
#define NUMBER_OF_PAGES 8192
#define NUMBER_OF_ENTRIES 8127 //8192 (total pages) - 1(root) - 64(fatEntries)
#define FILE_OFFSET 32
#define LONGFILENAMEOFFSET 32

/*
 * Define page/sector structures here as well as utility structures
 * such as directory entries.
 * ----------------------------------------
 * Sectors/Pages are 512 bytes
 * The filesystem is 4 megabytes in size.
 * You will have 8K pages total.
 * ----------------------------------------
 * Genearal Structure Information
 *
 * 0:         Root Sector       Starting Location: 0,     Ending Location: 511 bytes
 * 1 - 65:    FAT Table         Starting Locatin: 512,    Ending Location: 33791
 * 66: Root   Directory         Staring Location: 33792,  Ending Location: 34303
 * 67 - 8191: Data              Starting Location: 34303, Ending Location: ----> last number
 * ----------------------------------------------------------------------------------------
 */

//-------------------------------Boot Sector------------------------------------//
struct BootSector {
  unsigned int reservedSectors; //number of reserved sectors
  unsigned short bytesPerSector; //bytes per each sector in the FAT
  unsigned int numberOfPages; //number of pages used by file system
  unsigned int numberOfFatEntries; //number of entries in the FAT
  unsigned int ptrToFAT; //root goes from 0 - 511, then alloc begins at 512
  unsigned int ptrToData; //equals the sum of the sizes of ROOT + FAT
  unsigned int totalSize; //equals the total size of the file system
}; //16 byte



//Returns the default BootSector info into a local BootSector variable on the stack
struct BootSector getBootInfo();
//Constructor for the BootSector data structure
struct BootSector constructBootDataStructure(unsigned int reservedSectors,
                                             unsigned int bytesPerSector,
                                             unsigned int numberOfPages,
                                             unsigned int numberOfFatEntries,
                                             unsigned int ptrToFAT,
                                             unsigned int ptrToData,
                                             unsigned int totalSize);

struct BootSector* getBootSector(char* filesystem);
/* Notes:
The entire first sector is reserved for this boot sector, which holds system information
Total amount of space file system uses = (reservedBlocks + fileAllocationBlocks) * bytesPerSector
*/





//-------------------------------FAT Entry Struture---------------------------//
struct Alloc {
  unsigned short nextId; //represents the number for the next node in the FAT
  unsigned short attributes;
}; //4 bytes

//Returns a pointer to the Alloc Structure in the FAT entry at the index id
struct Alloc* getAllocStructure(char* fileSystem, unsigned short id);
//Returns the offset to the data location from the beginning of the filesystem
unsigned int getDataLocation(unsigned short id);
//sets the nextId of a Alloc structure, equivalent to the pointer to the next in a Linked List
void setNextId(struct Alloc*, unsigned short nextId);
//Sets the attributes of a alloc structure
void setAttributes(struct Alloc*, unsigned short attributes);
//returns 1 if a file, otherwise returns 0
int isFile(struct Alloc*);
//returns 1 if a directory, otherwise returns 0
int isDirectory(struct Alloc*);
//returns 1 if full, otherwise returns 0
int isFull(struct Alloc*);
//returns 1 if a free, otherwise returns 0
int isFree(struct Alloc* entry);
//returns the offset to the FAT entry location from the beginning of the filesystem
unsigned int getEntryLocation(unsigned short id);
//get the name of an entry
char* getName(char* filesys, unsigned short id);
/* Notes: This data structure will be used to contain the information in each FAT entry

Attribute definitions
0000 0000: free entry
0000 0001: taken entry
0000 0010: taken entry thats for data only
0000 0100:
0000 1000: full entry
0001 0000:
0010 0000:
0100 0000: is a directory
1000 0000: is a file

nextId cases:
value = 0xFF: points to null, at the end of the linked list or has not been allocated.
however, the attributes will determine whether or not the block is free so make sure
to check that when determining if its a free block

determining where the information is stored in the table:
offset = end of FAT table + id * 512

//Alloc takes up 64 pages, 64 * 512 = 32768
*/






//------------------------------ File Structure ------------------------------//
struct File {
    char filename[8]; //single short filename, handle long names later
    char extension[3]; //extension
    char attributes; //attributes (byte wise)
    unsigned short clusterStart; //first start of the cluster
    unsigned int fileSize; //size of the file
}; //20 bytes

//Helper Methods
//Gets the full name of a file, make sure to free!
char* fileGetName(struct File *file, char* filesys);
//Gets the pointer to the location of a file's data at the FAT id
struct File* getFile(unsigned short id, char* fileSystem);
//Createst file data in the file system
struct File* createFile(char* filesystem, unsigned short id, char* filename, char* extension, char attirbutes, unsigned short clusterStart, unsigned int fileSize);

/*
Notes:
The requirements for the use case of the file system boils down to simply storing and extracting information;
we do not care for this assignment about timing/date information. We need to be able to locate, read, write,
and dump the contents of the file. herefore, the filename, extension, attributes, and cluster start are the
most important aspect

Attributes Definitions:
0:
1:
2: Long file name
3:
4:
5:
6:
7:
8:
9:
10:
11:
12:
13:
14:
15:
*/







//------------------------------ Directory Structure -------------------------//
struct Directory {
  char name[11]; //single short filename, handle long names later
  char attributes; //attributes (byte wise) ---> could be read-only
  unsigned short clusterStart; //first start of the cluster
  unsigned int directorySize;
}; //20 bytes

//Helper Methods
//Gets the name of a directory, again make sure too free after done using
char* directoryGetName(struct Directory* directory, char* filesys);
//Gets the pointer to the Directory data for the FAT id
struct Directory* getDirectory(unsigned short id, char* fileSystem);
//Creates data inside the file system for a directory structure for given the FAT id
struct Directory* constructDirectoryStructure(char* filesystem, unsigned short id, char* name, char attirbutes, unsigned short clusterStart, unsigned int directorySize);
char* getDirectoryData(unsigned short currentDirectoryId, char* filesys);
unsigned int getDirectorySize(unsigned short directorySize, char* filesys);

/*
Notes:
Each directory has two files in it to begin with: a parentDir and one to itself,
therefore make sure to initialize it with two entries
The contents of the directory will be stored in the same way as a file -> in a page and we read in the infromation

*/

//-------------------Long File Name Structure---------------//
struct LongFileName {
  unsigned int size;
  unsigned short clusterStart;
  unsigned short FAT;
}; //8bytes
int hasLongDirectoryName(struct Directory* directory);
int hasLongFileName(struct File* file);
char* getLongName(unsigned short id, char* filesys);
int makeLongFileName(struct File* file, char* data, char* filesys);
int makeLongDirectoryName(struct Directory* directory, char* data, char* filesys);
int min(int a, int b);
//Gets an array of entries in the file system for a requested size,
//if the request is succesful, then it chains them and returns the first one in the new chain
//if no room left and an error, then it returns 0
unsigned short getEntry(char* fileSystem, int entrysize);
struct LongFileName* getLongFileNameStructure(char* filesys, unsigned short idx);

#endif
