#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "support.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * Make sure that the student name and email fields are not empty.
 */
void check_team(char * progname)
{
	if(!strcmp("", team.names) || !strcmp("", team.emails))
	{
		printf("%s: Please fill in the team struct in team.c\n", progname);
		exit(1);
	}
	printf("Students: %s\n", team.names);
	printf("Emails  : %s\n", team.emails);
	printf("\n");
}

int openFileSystem(char* file) {
	return open(file, O_RDWR);
}

void initializeBootSector(char* filesys) {
  struct BootSector obj = getBootInfo();
  memcpy(filesys, &obj, sizeof(struct BootSector));
}

int veryifyBootSector(char* filesys) {
		struct BootSector* bootImage = getBootSector(filesys);
		struct BootSector bootDefault = getBootInfo();
		if(bootDefault.reservedSectors == bootImage -> reservedSectors &&
			 bootDefault.bytesPerSector == bootImage -> bytesPerSector &&
			 bootDefault.numberOfPages == bootImage -> numberOfPages &&
			 bootDefault.numberOfFatEntries == bootImage -> numberOfFatEntries &&
		   bootDefault.ptrToFAT == bootImage -> ptrToFAT &&
			 bootDefault.ptrToData == bootImage -> ptrToData &&
			 bootDefault.totalSize == bootImage -> totalSize) {
			 return 1;
		}
		return 0;
}

void initializeRootDirectory(char* filesystem) {
	char name[11];
	memset(name, '\0', 11);
	name[0] = 'r';
	name[1] = 'o';
	name[2] = 'o';
	name[3] = 't';
	setAttributes(getAllocStructure(filesystem, 0), (1 << 6) | 1);
	constructDirectoryStructure(filesystem, 0, name, 0, 0, DIRECTORY_OFFSET);
	//add the .
	addToDirectory(filesystem, 0, 0);
	//add the .. since its the root points to itself
	addToDirectory(filesystem, 0, 0);
}

int verifyRootSector(char* filesysptr) {
	struct Alloc* root = getAllocStructure(filesysptr, 0);
	struct Directory* rootDir = getDirectory(0, filesysptr);
	if (
		  //make all our comparisons
			strcmp(directoryGetName(rootDir, filesysptr), "root") == 0 &&
			isDirectory(root) &&
			isFree(root) == 0 &&
			rootDir -> clusterStart == 0 &&
			rootDir -> directorySize >= DIRECTORY_OFFSET
			//end all our comparisons
		)
		{ return 1; }
	return 0;
}

int verifyFileSystem(char* filesystem) {
	return veryifyBootSector(filesystem) && verifyRootSector(filesystem);
}

char* createFileSystem(char* filename) {
	//create pointer
	char *str = (char*)malloc(4194304); //malloc file sys
	memset(str, 0, 4194304); //memset everything to 0

	//Create file to mmap
	int fd = open(filename,  O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
	if (fd == -1) {
		perror("Error opening file for writing");
		free(str);
		exit(EXIT_FAILURE);
	}
	//Place 4MB of 0s into the file
	write(fd, str, 4194304); //write pointer to file

	//mmap the file into a pointer
	char *filesysptr = mmap(0, 4194304, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (filesysptr == MAP_FAILED) {
 		close(fd);
 		perror("Error mmapping the file");
		free(str);
 		exit(EXIT_FAILURE);
 	}
	close(fd);
	free(str);
	//if all is sucessfull, initialze the boot sector and initialize the root directory
	initializeBootSector(filesysptr);
	initializeRootDirectory(filesysptr);
	//return the filesysptr
	return filesysptr;
}

unsigned short getOneFreeEntry(char* fileSystem) {
	for (int i = 1; i < NUMBER_OF_ENTRIES; i++) {
		struct Alloc* newAlloc = getAllocStructure(fileSystem, i);
		if (isFree(newAlloc)) {
			return i;
		}
	}
	return -1;
}


int addToDirectory(char* filesystem, unsigned short sourceId, unsigned short insertId) {
  struct Alloc* entry = getAllocStructure(filesystem, sourceId);
  struct Directory* directory = getDirectory(sourceId, filesystem);
  //while the entry is full and there exists a next entry, traverse the linked list
  while (isFull(entry) && entry -> nextId != 0) {
    sourceId = entry -> nextId;
    entry = getAllocStructure(filesystem, entry -> nextId);
  }

  //if we're full and are at the last entry, then we'll create a new entry to add onto the list
  if (isFull(entry) && entry -> nextId == 0) {
    unsigned short newEntryId = getOneFreeEntry(filesystem);
    if (newEntryId == 0) {
      //error no more free space
      return -1;
    }
    //update the next pointer on our linked list to the new entry
    entry -> nextId = newEntryId;
    sourceId = newEntryId;
    //go to the new entry and perform the needed operations
    entry = getAllocStructure(filesystem, newEntryId);
    unsigned short attributes = 3; //1 << 1 | 1, we're taken and are for data only
    setAttributes(entry, attributes);
  }

  //now we are at a free data block and have a situation where we can place the data
  char* startingLocation = filesystem + getDataLocation(sourceId) + (directory -> directorySize % 512);
  char* endingLocation = filesystem + getDataLocation(sourceId) + 512;
  char data[20];
  sprintf(data, "%d,", insertId);
  int dataSize = strlen(data);

  //we cant add it entirely to the directory, attempt to create new data block
  if (startingLocation + dataSize >= endingLocation) {
    unsigned short newEntryId = getOneFreeEntry(filesystem);
    if (newEntryId == 0) {
      //error no more free space
      return -1;
    }

    //set the current structure to full
    entry -> attributes |= (1 << 3);
    //update the next pointer on our linked list to the new entry
    entry -> nextId = newEntryId;
    //go to the new entry and perform the needed operations
    entry = getAllocStructure(filesystem, newEntryId);
    unsigned short attributes = 3; //1 << 1 | 1, we're taken and are for data only
    setAttributes(entry, attributes);

    //write to the end of the file and into the next file
    directory -> directorySize += dataSize;
		//write the first part to the old file, new part to the next file
		//first part = startingLocation -> endingLocation, endingLocation - startingLocaton
		int firstPageWrite = (unsigned long long)endingLocation - (unsigned long long)startingLocation;
		memcpy(startingLocation, data, firstPageWrite);
		//get the next files locaiton to write in the rest
    startingLocation = filesystem + getDataLocation(newEntryId);
    memcpy(startingLocation, data + firstPageWrite, dataSize - firstPageWrite);
  }
  else {
    memcpy(startingLocation, data, dataSize);
    directory -> directorySize += dataSize;
		//if we make it full, then set the attribute to full
    if (directory -> directorySize % 512 == 0) {
      entry -> attributes |= (1 << 3);
    }
  }
  return 0;
}

unsigned short createNewDirectory(char* filesystem, unsigned short parentDirectory, char* name, char attributes) {
	unsigned short entryId = getOneFreeEntry(filesystem);
	if (entryId == 0) {
		return 0;
	}
	//set the attributes of the FAT TABLE
	setAttributes(getAllocStructure(filesystem, entryId), attributes);
	//constrcut the new directory structure in the filesystem
	constructDirectoryStructure(filesystem, entryId, name, 0, entryId, DIRECTORY_OFFSET);
	//add the first two essential entries of . and ..
	addToDirectory(filesystem, entryId, entryId);
	addToDirectory(filesystem, entryId, parentDirectory);
	return entryId;
}

unsigned char* getPagePointer(char* filesystem, int pageNumber) {
	return (unsigned char*) (filesystem + pageNumber * PAGE_SIZE);
}

void dump(FILE* fp, int pageNumber, char* filesys) {
	if (pageNumber >= NUMBER_OF_PAGES || pageNumber < 0) {
		return;
	}
	unsigned char* runner = getPagePointer(filesys, pageNumber);
	//we create a 16 x 32 grid of hex output == 512 bytes of data
	int counter = 0;
	for (int row = 0; row < 16; row++) {
		for (int col = 0; col < 32; col++) {
			if (col == 16) {
				fprintf(fp, "    ");
			}
			fprintf(fp, "%02X ", runner[counter]);
			counter++;
		}
		fprintf(fp, "\n");
	}
}

void dumpBinary(FILE* fp, int pageNumber, char* filesys) {
	if (pageNumber >= NUMBER_OF_PAGES || pageNumber < 0 || fp == NULL) {
		fprintf(stderr, "Invalid request to dump binary\n");
		return;
	}
	unsigned char* runner = getPagePointer(filesys, pageNumber);
	for (int i = 0; i < PAGE_SIZE; i++) {
		fprintf(fp, "%c", runner[i]);
	}
}

int skipNCommas(char* data, int i, int size, int n) {
	int commaCount = 0;
	while (i < size) {
		if (data[i] == ',') {
			commaCount++;
			if (commaCount == n) {
				break;
			}
		}
		i++;
	}
	return i + 1;
}

unsigned short searchDirectory(unsigned short currentDirectoryId, char* name, char* filesys) {
	struct Vector* entries = getDirectoryEntries(filesys, currentDirectoryId);
	for (int i = 2; i < entries -> currentSize; i++) {
		unsigned int id = entries -> data[i];
		char* entryName = getName(filesys, id);
		if (strcmp(name, entryName) == 0) {
			free(entryName);
			destructVector(entries);
			return id;
		}
		free(entryName);
	}
	destructVector(entries);
	return 0;
}

struct Vector* getDirectoryEntries(char* filesys, unsigned short directoryId) {
	//Get the first entries information
	char* data = filesys + getDataLocation(directoryId);
	unsigned int size = getDirectorySize(directoryId, filesys);
	struct Alloc* currentPage = getAllocStructure(filesys, directoryId);

	//Construct a vector to hold the entries
	struct Vector *array = constructVector(32);

	//Read in all the information
	int pageCounter = DIRECTORY_OFFSET;
	int dataLeft = size - pageCounter;
	while (dataLeft > 0) {
		//if we're at the end of a page
		if (pageCounter == PAGE_SIZE) {
			//get the next entries information
			data = filesys + getDataLocation(currentPage -> nextId);
			currentPage = getAllocStructure(filesys, currentPage -> nextId);
			pageCounter = 0;
		}

		//read in the current entries information
		size_t bytesRead = 0;
		unsigned short num = 0;
		while (pageCounter < PAGE_SIZE && data[pageCounter] != ',') {
			num = num * 10 + data[pageCounter] - '0';
			pageCounter++;
			bytesRead++;
		}

		//if we still have more data to read
		if (pageCounter == PAGE_SIZE && data[pageCounter - 1] != ',') {
			//get the next entries information
			data = filesys + getDataLocation(currentPage -> nextId);
			currentPage = getAllocStructure(filesys, currentPage -> nextId);

			//continue reading from the next entry
			pageCounter = 0;
			while (data[pageCounter] != ',') {
				num = num * 10 + data[pageCounter] - '0';
				pageCounter++;
				bytesRead++;
			}
		}
		pageCounter++;
		bytesRead++;

		//we are now done reading in the information for the current id entry,
		//and we will store it inside the vector
		insertVector(array, num);

		//update the number of bytes left to read
		dataLeft -= bytesRead;
	}
	return array;
}

void lsFile(char* filesys, unsigned short id) {
	struct File* file = getFile(id, filesys);
	fprintf(stdout, "f ");
	fprintf(stdout, "%12d ", file -> fileSize - FILE_OFFSET);
	fprintf(stdout, "%s\n", fileGetName(file, filesys));
}

void lsDirectory(char* filesys, unsigned short id) {
	struct Directory* directory = getDirectory(id, filesys);
	fprintf(stdout,"d ");
	fprintf(stdout, "%12d ", (directory -> directorySize / 512 + 1));
	fprintf(stdout, "%s\n", directoryGetName(directory, filesys));
}

void lsParentDirectory(char* filesys, unsigned short directoryId) {
	struct Directory* directory = getDirectory(getParentDirectory(filesys, directoryId), filesys);
	fprintf(stdout, "d ");
	if (getParentDirectory(filesys, directoryId) == 0) {
		fprintf(stdout, "%12d ", 1);
	} else {
		fprintf(stdout, "%12d ", (directory -> directorySize / 512 + 1));
	}
	fprintf(stdout, "..\n");
}

void lsCurrentDirectory(char* filesys, unsigned short id) {
	struct Directory* directory = getDirectory(id, filesys);
	fprintf(stdout, "d ");
	if (id == 0) {
		fprintf(stdout, "%12d ", 1);
	} else {
		fprintf(stdout, "%12d ", (directory -> directorySize / 512 + 1));
	}
	fprintf(stdout, ".\n");
}

void lsUser(char* filesys, unsigned short directoryId) {
	struct Vector* entries = getDirectoryEntries(filesys, directoryId);
	lsCurrentDirectory(filesys, directoryId);
	lsParentDirectory(filesys, directoryId);
	for (int i = 2; i < entries -> currentSize; i++) {
		unsigned short id = entries -> data[i];
		struct Alloc* entry = getAllocStructure(filesys, id);
		if (isFile(entry)) {
			lsFile(filesys, id);
		}
		else if (isDirectory(entry)) {
			lsDirectory(filesys, id);
		}
	}
	destructVector(entries);
}

int mkdirUser(char* name, unsigned short currentDirectoryId, char* filesys) {
	struct Directory* directory = getDirectory(currentDirectoryId, filesys);
	unsigned short fileId = searchDirectory(currentDirectoryId, name, filesys);
	//fileanme already exists in directory
	if (fileId != 0) {
		fprintf(stderr, "Name already exists\n");
		return -1;
	}
	unsigned short attributes = (1 << 6) | 1;
	unsigned short newDirectoryId = createNewDirectory(filesys, currentDirectoryId, name, attributes);
	//wasn't enough space to create a new directory
	if (newDirectoryId == 0) {
		fprintf(stderr, "Not enough space to create new directory\n");
		return -1;
	}
	int result = addToDirectory(filesys, currentDirectoryId, newDirectoryId);
	//if no more space left in the file system to append to the current directory
	if (result == -1) {
		fprintf(stderr, "Not enough space to create new directory\n");
	}
	return result;
}

char* pwdUser(char* filesystem, unsigned short directoryId) {
	//create a linked list of directory names
	struct ListNode* head = NULL;
	int size = 0;
	while (directoryId != 0) {
		//get relevnt information of current directory
		struct Directory* directory = getDirectory(directoryId, filesystem);
		char* name = directoryGetName(directory, filesystem);
		unsigned short parentDirectory = getParentDirectory(filesystem, directoryId);

		//update relevant information to continue traversing backwards
		head = createAtHead(name, head);
		size += strlen(name) + 1; //length of name + /
		directoryId = parentDirectory;
	}

	//traverse the linked list and build the string
	char* str = (char*)malloc((size + 2) * sizeof(char)); // totalLength = / + length of strings + null
	struct ListNode* runner = head;
	unsigned int counter = 0;
	str[counter] = '/';
	counter++;
	while(runner != NULL) {
		strcpy(&str[counter], runner -> name);
		counter += strlen(runner -> name);
		str[counter] = '/';
		counter++;
		runner = runner -> next;
	}
	str[size + 1] = '\0';
	destructList(head);
	return str;
}

int cdUser(char* filesys, char* name, unsigned short *currentDirectoryId) {
	if (strcmp(name, ".") == 0) {
		return 0;
	}
	else if (strcmp(name, "..") == 0) {
		unsigned short id = getParentDirectory(filesys, *currentDirectoryId);
		*currentDirectoryId = id;
		return 0;
	}
	struct Vector* array = getDirectoryEntries(filesys, *currentDirectoryId);
	for (int i = 2; i < array -> currentSize; i++) {
		unsigned short id = array -> data[i];
		char* entryName = getName(filesys, id);
		if (strcmp(name, entryName) == 0) {
			struct Alloc* entry = getAllocStructure(filesys, id);
			//error when it's a file
			if (isFile(entry)) {
				free(entryName);
				destructVector(array);
				return -2;
			}
			//change to that directory
			free(entryName);
			destructVector(array);
			*currentDirectoryId = id;
			return 0;
		}
		free(entryName);
	}
	//could not find
	destructVector(array);
	return -1;
}

unsigned short getParentDirectory(char* filesystem, unsigned short directoryId) {
  char* data = getDirectoryData(directoryId, filesystem);
	int i = skipNCommas(data, 0, getDirectory(directoryId, filesystem) -> directorySize, 1);
	unsigned short id = 0;
	while (data[i] != ',' && data[i] != 0) {
		id = id * 10 + data[i] - '0';
		i++;
	}
	return id;
}

void usage(char* filesys) {
	int totalSpace = 0;
	int allocatedSpace = 0;

	//first add the number of reserved sectors to overhead (only the boot sector)
	totalSpace += 512 * 65;

	for (unsigned short id = 0; id < NUMBER_OF_ENTRIES; id++) {
		struct Alloc* entry = getAllocStructure(filesys, id);
		if (!isFree(entry)) {
			if (isFile(entry)) {
				int spaceUsed = getFile(id, filesys) -> fileSize;
				totalSpace += spaceUsed;
				allocatedSpace += spaceUsed - FILE_OFFSET;
			}
			else {
				//this includes the allocated entries for long filenames and directories
				totalSpace += PAGE_SIZE;
			}
		}
	}
	fprintf(stdout, "Total Space Used: %d\n", totalSpace);
	fprintf(stdout, "Total Space Taken Up By Files Only: %d\n", allocatedSpace);
}

unsigned short findFile(char *filesys, char *name, unsigned short currentDirectoryId) {
    //Need to only go through list of entries inside current directory
    struct Vector* entries = getDirectoryEntries(filesys, currentDirectoryId);
    for (int i = 2; i < entries -> currentSize; i++) {
        unsigned int id = entries -> data[i];
        struct Alloc *thisAlloc = getAllocStructure(filesys, id);
        if (isFile(thisAlloc)) {
            char* entryName = fileGetName(getFile(id, filesys), filesys);
        		if (strcmp(entryName, name) == 0) {
								destructVector(entries);
								free(entryName);
            		return id;
        		}
						free(entryName);
    		}
    }
    destructVector(entries);
    return 0;
}

int writeToFile(char *filename, int amt, char *data, char *fileSystem, unsigned short directoryId) {
	if (data == NULL) {
		fprintf(stderr, "Didn't give me any good data!\n");
		return -1;
	}

	//if not writing anything, don't waste anytime or thought handling it
	if (amt == 0) {
		return 0;
	}

  unsigned short id = findFile(fileSystem, filename, directoryId);
	unsigned short entryId = 0;
	struct File* fp = NULL;
	//if there doesnt exist a file with that name
	if (id == 0) {
		//size = header information + amount of data requested
		int amtNeeded = amt + FILE_OFFSET;
    //Get a bunch of pages to insert that entry
    entryId = getEntry(fileSystem, amtNeeded);
		//if there wasnt enough room, then return -1
		if (entryId == 0) {
			fprintf(stderr, "Not enough space left in file system\n");
      return -1;
    }

		struct Alloc* newAlloc = getAllocStructure(fileSystem, entryId);
		setAttributes(newAlloc, (1 << 7) | 1);
    fp = createFile(fileSystem, entryId, filename, NULL, 0, entryId, FILE_OFFSET);
		setAttributes(newAlloc, (1 << 7) | 1);

		//only add to directory when the file doesn't already exist
		int flag = addToDirectory(fileSystem, directoryId, entryId);
		if(flag == -1) {
				//if we can't add to directory, clear the data we were going to add
				//so we don't show an intermediate state
				clearAllInfo(fileSystem, entryId);
				setAttributes(newAlloc, 0);
				fprintf(stderr, "Unable to add to directory, out of space\n");
		  	return -2;
		}
  }
	//otherweise, we have to clear the old file
  else {
  	//CLEAR THE FILE
    clearAllInfo(fileSystem, id);
    entryId = getEntry(fileSystem, amt);
    if (entryId == 0) {
				fprintf(stderr, "Not enough space left in file system\n");
      	return -1;
		}
		struct Alloc* newAlloc = getAllocStructure(fileSystem, entryId);
    setAttributes(newAlloc, (1 << 7) | 1);
		fp = createFile(fileSystem, entryId, filename, NULL, 0, entryId, FILE_OFFSET);
    setAttributes(newAlloc, (1 << 7) | 1);
	}
	return appendData(fileSystem, data, amt, entryId, FILE_OFFSET, fp);
}

int appendData(char* filesys, char* data, unsigned int amount, unsigned short id, unsigned int offset, struct File* file) {
	struct Alloc* entry = getAllocStructure(filesys, id);
	int dataLeft = amount;
	int counter = 0;

	while (dataLeft > 0) {
		char* page = filesys + getDataLocation(id);
		//copy in data and update size variables
		int amountToWrite = min(PAGE_SIZE - offset, dataLeft);
		memcpy(page + offset, data + counter, amountToWrite);
		dataLeft -= amountToWrite;
		counter += amountToWrite;
		offset = 0;
		file -> fileSize += amountToWrite;

		//update FAT structure and get next linked list entry if there exists one
		if (dataLeft == 0 && file -> fileSize % 512 != 0) {
			setAttributes(entry, entry -> attributes | 1); //finished writing, dont set to full, but set to taken
		}
		else {
			setAttributes(entry, entry -> attributes | (1 << 3) | 1); //set to full
			id = entry -> nextId;
			entry = getAllocStructure(filesys, entry -> nextId);
		}
	}
  return 0;
}

int appendToFile(char *filename, int amt, char *data, char *filesys, unsigned short directoryId) {
		if (data == NULL) {
			return -1;
		}

		//if not writing anything, don't waste anytime or thought handling it
		if (amt == 0) {
			return 0;
		}

    unsigned short id = findFile(filesys, filename, directoryId);
		//if filename does not exist inside current directory or is a directory, return
    if (id == 0) {
				fprintf(stderr, "\nCan't append %s, provide a valid entry\n", filename);
        return -1;
    }
		struct File* file = getFile(id, filesys);
		struct Alloc* entry = getAllocStructure(filesys, id);

		//traverse to the last entry of the file
		while (entry -> nextId != 0) {
			id = entry -> nextId;
			entry = getAllocStructure(filesys, entry -> nextId);
		}

		//get amount of space left in file
    int amountLeft;
		//if its full, we need more entries
		if (isFull(entry) || file -> fileSize % 512 == 0) {
			amountLeft = 0;
		}
		//if its not full, get the amount left at the end
		else {
			amountLeft = PAGE_SIZE - (file -> fileSize % 512);
		}
    //If the current file needs more space, attempt to retrive more space (also works when file is full)
    if (amt > amountLeft) {
        int neededSpace = amt - amountLeft;
        unsigned short extraSpace = getEntry(filesys, neededSpace);
        if (extraSpace == 0) {
						fprintf(stderr, "Not enough space to append data\n");
            return -1;
        }
				//connect the nodes by setting the next pointer
				entry -> nextId = extraSpace;
    }
		//if we're not full at that location
		if (amountLeft != 0) {
			return appendData(filesys, data, amt, id, file -> fileSize % 512, file);
		}
		return appendData(filesys, data, amt, entry -> nextId, 0, file);
}

int cat(char* filename, char* filesys, unsigned short directoryId) {
    unsigned short id = findFile(filesys, filename, directoryId);
		//if not a file or a directory, return an error
    if (id == 0) {
        fprintf(stderr, "Invalid request\n");
        return -1;
    }
    struct File* file = getFile(id, filesys);
    struct Alloc* entry = getAllocStructure(filesys, id);
		char* data = filesys + getDataLocation(id);
		int offset = FILE_OFFSET;
		int dataLeft = file -> fileSize - FILE_OFFSET;

		while (dataLeft > 0) {
				int amtToPrint = min(dataLeft, PAGE_SIZE - offset);
				dataLeft -= amtToPrint;
				int i = offset;
				while (amtToPrint > 0) {
					fprintf(stdout, "%c", data[i]);
					i++;
					amtToPrint--;
				}
				//get the next data segment if it exists
				data = filesys + getDataLocation(entry -> nextId);
				entry = getAllocStructure(filesys, entry -> nextId);
				offset = 0;
		}
		return 0;
}

void resetDirectory(unsigned short directoryId, char* filesys) {
	unsigned short runner = directoryId;
	//clear the information in each directory
	do {
		//get the data location
		char* data = filesys + getDataLocation(runner);
		//if we're at the start, then don't remove the haeader information
		if (runner == directoryId) {
			memset(data + DIRECTORY_OFFSET, 0, PAGE_SIZE - DIRECTORY_OFFSET);
		}
		else {
			memset(data, 0, PAGE_SIZE);
		}
		//get the FAT entry
		struct Alloc* entry = getAllocStructure(filesys, runner);
		//store the next runner
		runner = entry -> nextId;
		//clear the entry
		entry -> attributes = 0;
		entry -> nextId = 0;
		//contine to the next alloced structure if there exists one
	} while(runner != 0);

	//reset the FAT table entry
	struct Alloc* entry = getAllocStructure(filesys, directoryId);
	entry -> nextId = 0;
	entry -> attributes = (1 << 6) | 1;

	//reset sizing information
	struct Directory* directory = getDirectory(directoryId, filesys);
	directory -> directorySize = DIRECTORY_OFFSET;
}

void updateDirectory(int removeId, unsigned short directoryId, char* filesys) {
	struct Vector* entries = getDirectoryEntries(filesys, directoryId);
	resetDirectory(directoryId, filesys);
	for (int i = 0; i < entries -> currentSize; i++) {
		unsigned short id = entries -> data[i];
		if (id != removeId) {
			addToDirectory(filesys, directoryId, id);
		}
	}
	destructVector(entries);
}

int rmdirUser(char* name, unsigned short *currentDirectoryId, char* filesys) {
	if (strcmp(name, ".") == 0) {
		if (*currentDirectoryId == 0) {
			fprintf(stdout, "Can not remove root directory!\n");
			return -1;
		}
		char* name = directoryGetName(getDirectory(*currentDirectoryId, filesys), filesys);
		unsigned short envelope = getParentDirectory(filesys, *currentDirectoryId);
		if (rmdirUser(name, &envelope, filesys) == 0) {
			*currentDirectoryId = envelope;
			return 0;
		}
		fprintf(stdout, "Directory has contents, cant remove it!\n");
		return -1;
	}
	else if (strcmp(name, "..") == 0) {
			if (getParentDirectory(filesys, *currentDirectoryId) == 0) {
				fprintf(stdout, "Can not remove root directory!\n");
				return -1;
			}
			fprintf(stdout, "Directory has contents, can't remove it\n");
			return -1;
	}
  unsigned short fileId = searchDirectory(*currentDirectoryId, name, filesys);
  if (fileId == 0) {
    fprintf(stderr, "Name does not exist in currrent directory\n");
    return -1;
  }
	struct Directory* directory = getDirectory(fileId, filesys);
	char* data = filesys + getDataLocation(fileId);

  //if more than two commas directory is not empty
  int numofcommas = 0;
	int i = DIRECTORY_OFFSET;
  while (i < directory -> directorySize && data[i] != 0) {
    if (data[i] == ',') {
      numofcommas++;
      if (numofcommas > 2) {
        fprintf(stderr, "Directory not empty\n");
        return -1;
      }
    }
    i++;
  }

  updateDirectory(fileId, *currentDirectoryId, filesys);
  fprintf(stderr, "Directory has been removed\n");
  return 0;
}

void clearAllInfo(char* filesys, unsigned short id) {
	//never clear the root folder
	struct Alloc* alloc = getAllocStructure(filesys, id);
	if (isFile(alloc)) {
		struct File* file = getFile(id, filesys);
		if (hasLongFileName(file)) {
			int nameId = atoi(file -> filename);
			while (nameId != 0) {
				//get the data and entry locations
				struct Alloc* entry = getAllocStructure(filesys, nameId);
				char* data = filesys + getDataLocation(nameId);
				//clear attributes and data
				entry -> attributes = 0;
				memset(data, 0, PAGE_SIZE);
				//go to the next entry
				nameId = entry -> nextId;
				//finish clearning the FAT entries information
				entry -> nextId = 0;
			}
		}
	}
	else if (isDirectory(alloc)) {
		struct Directory* directory = getDirectory(id, filesys);
		if (hasLongDirectoryName(directory)) {
			int directoryId = atoi(directory -> name);
			while (directoryId != 0) {
				//get the data and entry locations
				struct Alloc* entry = getAllocStructure(filesys, directoryId);
				char* data = filesys + getDataLocation(directoryId);
				//clear attributes and data
				entry -> attributes = 0;
				memset(data, 0, PAGE_SIZE);
				//go to the next entry
				directoryId = entry -> nextId;
				//finish clearning the FAT entries information
				entry -> nextId = 0;
			}
		}
	}
	while (id != 0) {
		//get the data and entry locations
		struct Alloc* entry = getAllocStructure(filesys, id);
		char* data = filesys + getDataLocation(id);
		//clear attributes and data
		entry -> attributes = 0;
		memset(data, 0, PAGE_SIZE);
		//go to the next entry
		id = entry -> nextId;
		//finish clearning the FAT entries information
		entry -> nextId = 0;
	}
}

unsigned short rmRF(char* filesys, unsigned short directoryId, char* name) {
	if (strcmp(name, ".") == 0) {
		if (directoryId == 0) {
			fprintf(stdout, "Can not remove root directory!\n");
			return directoryId;
		}
		unsigned short envelope = getParentDirectory(filesys, directoryId);
		rmRF(filesys, envelope, directoryGetName(getDirectory(directoryId, filesys), filesys));
		return envelope;
	}
	else if (strcmp(name, "..") == 0) {
		unsigned short parentDirectory = getParentDirectory(filesys, directoryId);
		if (parentDirectory == 0) {
			fprintf(stdout, "Can not remove root directory!\n");
			return directoryId;
		}
		unsigned short envelope = getParentDirectory(filesys, parentDirectory);
		rmRF(filesys, envelope, directoryGetName(getDirectory(parentDirectory, filesys), filesys));
		return envelope;
	}
	unsigned short id = searchDirectory(directoryId, name, filesys);
	if (id == 0) {
		fprintf(stdout, "Could not find that entry in the current directory\n");
		return directoryId;
	}
	rmAllFiles(filesys, id);
	updateDirectory(id, directoryId, filesys);
	return directoryId;
}

void rmAllFiles(char* filesys, unsigned short id) {
	struct Alloc* entry = getAllocStructure(filesys, id);
	if (isFile(entry)) {
		clearAllInfo(filesys, id);
	}
	else {
		struct Vector* entries = getDirectoryEntries(filesys, id);
		for (int i = 2; i < entries -> currentSize; i++) {
			rmAllFiles(filesys, entries -> data[i]);
		}
		destructVector(entries);
		clearAllInfo(filesys, id);
	}
}

int rmUser(char* name, unsigned short currentDirectoryId, char* filesys) {
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
		fprintf(stdout, "Can not remove directory with that command\n");
		return -1;
	}
	unsigned short fileId = searchDirectory(currentDirectoryId, name, filesys);
	if (fileId == 0) {
		fprintf(stderr, "Name does not exist in currrent directory\n");
		return -1;
	}
	struct Alloc* entry = getAllocStructure(filesys, fileId);
	if (isFile(entry)) {
		clearAllInfo(filesys, fileId);
		updateDirectory(fileId, currentDirectoryId, filesys);
		fprintf(stderr, "File removed\n");
		return 0;
	}
	else{
		fprintf(stderr, "Can not remove directory with that command\n");
		return -1;
	}
}

int rmRange(char* filesys, unsigned short currentDirectoryId, char* name, unsigned int start, unsigned int end) {
	unsigned short id = findFile(filesys, name, currentDirectoryId);
	if (id == 0) {
		fprintf(stderr, "Invalid filename input\n");
		return -1;
	}
	else if (start > end) {
		fprintf(stderr, "Invalid bound input, reverse order\n");
		return -1;
	}
	struct File* file = getFile(id, filesys);
	struct Alloc* entry = getAllocStructure(filesys, id);

	end += FILE_OFFSET; //exact byte locations
	start += FILE_OFFSET; //exact byte locations
	//store information in a buffer to rewrite for the remaining data, if it exists
	struct DataBlock* storage = constructDataBlock(4096);
	int dataLeft = file -> fileSize;
	unsigned short runner = id;
	int offset = FILE_OFFSET, counter = FILE_OFFSET;

	while (dataLeft > 0) {
		//get the current page
		char* page = filesys + getDataLocation(runner);
		//get how much to write in that page
		int amountToWrite = min(PAGE_SIZE - offset, dataLeft);
		//write to the insertion block only when inside the bounds
		for (int i = 0; i < amountToWrite; i++) {
			if (counter < start || counter >= end) {
				insertDataBlock(storage, page + offset + i);
			}
			counter++;
		}
		dataLeft -= amountToWrite;
		runner = entry -> nextId;
		entry = getAllocStructure(filesys, entry -> nextId);
		offset = 0;
	}
	rmUser(name, currentDirectoryId, filesys);
	writeToFile(name, storage -> currentSize, storage -> data, filesys, currentDirectoryId);
	destructDataBlock(storage);
	return 0;
}

void getpages(char* name, unsigned short currentDirectoryId, char* filesys) {
	unsigned short Id;
	if (strcmp(name, ".") == 0) {
		Id = currentDirectoryId;
	}
	else if (strcmp(name, "..") == 0) {
		Id = getParentDirectory(filesys, currentDirectoryId);
	}
	else {
		Id = searchDirectory(currentDirectoryId, name, filesys);
		//return if name not in directory
		if (Id == 0) {
			fprintf(stderr, "Name does not exist in currrent directory\n");
			return;
		}
	}
	struct Alloc* entry = getAllocStructure(filesys, Id);
	//check if directory
	if (isDirectory(entry)) {
		//get all directory entries
		struct Vector* entries = getDirectoryEntries(filesys, Id);
		for (int i = 0; i < entries -> currentSize; i++) {
			//get entry
			unsigned int pageId = entries -> data[i];
			struct Alloc* entry = getAllocStructure(filesys, pageId);
			//last entry to add
			if(i == entries -> currentSize - 1){
				if(isFile(entry)){
					while (pageId != 0) {
						//get the data and entry locations
						entry = getAllocStructure(filesys, pageId);
						unsigned int tempId = entry ->nextId;
						if(tempId == 0){
							fprintf(stdout, "%d\n", pageId+65);
							return;
						}
						else{
							fprintf(stdout, "%d,", pageId+65);
						}
						//go to the next entry
						pageId = entry -> nextId;
					}
				}
				else{
					fprintf(stdout, "%d\n", pageId+65);
					return;
				}
			}
			else if(isFile(entry)){
				while (pageId != 0) {
					//get the data and entry locations
					entry = getAllocStructure(filesys, pageId);
					fprintf(stdout, "%d,", pageId+65);
					//go to the next entry
					pageId = entry -> nextId;
				}
			}
			else{
				fprintf(stdout, "%d,", pageId+65);
			}
		}
	}
	else if(isFile(entry)){
		unsigned int pageId = Id;
		while (pageId != 0) {
			//get the data and entry locations
			entry = getAllocStructure(filesys, pageId);
			unsigned int tempId = entry ->nextId;
			if(tempId == 0){
				fprintf(stdout, "%d", pageId+65);
				break;
			}
			else{
				fprintf(stdout, "%d,", pageId+65);
			}
			//go to the next entry
			pageId = entry -> nextId;
		}
		fprintf(stdout, "\n");
		return;
	}
}

void getRange(char* filename, int start, int end, unsigned short currentDirectoryId, char* filesys){
	unsigned short fileId = searchDirectory(currentDirectoryId, filename, filesys);
	//return if name not in directory
	if(fileId == 0){
		fprintf(stderr, "Name does not exist in currrent directory\n");
		return;
	}
	struct Alloc* entry = getAllocStructure(filesys, fileId);
	if(isFile(entry)){
		//file to get range from
		struct File* file = getFile(fileId, filesys);
		unsigned int filesize = (file -> fileSize) - FILE_OFFSET;
		if(start > (filesize-1)){
			fprintf(stderr, "Start location is past data bound\n");
			return;
		}
		else if(end > filesize){
			char* data = (char*)malloc((filesize-start)*sizeof(char) + 1);
			char* location = filesys + getDataLocation(fileId) + start + FILE_OFFSET;
			memcpy(data, location, (filesize-start));
			data[(filesize-start)] = '\0';
			fprintf(stdout, "%s\n", data);
			free(data);
			return;
		}
		else{
			char* data = (char*)malloc((end-start)*sizeof(char) + 1);
			char* location = filesys + getDataLocation(fileId) + start + FILE_OFFSET;
			memcpy(data, location, (end-start));
			data[(end-start)] = '\0';
			fprintf(stdout, "%s\n", data);
			free(data);
			return;
		}
	}
	else{
		fprintf(stderr, "Entry is a directoy: File needed\n");
		return;
	}
}

//---------------------------DEBUG HELPER METHODS
void dumpAlloc(char* filesystem, unsigned int id) {
	struct Alloc* data = getAllocStructure(filesystem, id);
	printf("Next Id: %d\n Attributes: %d\n", data -> nextId, data -> attributes);
}

void dumpDirectory(char* filesystem, unsigned int id) {
	struct Directory* directory = getDirectory(id, filesystem);
	printf("Name: %s\n Information: %d %d %d\n", directory -> name, directory -> attributes, directory -> clusterStart, directory -> directorySize);
}


//Data structure helpers
struct ListNode* createAtHead(char* name, struct ListNode* head) {
	struct ListNode* newNode = (struct ListNode*)malloc(sizeof(struct ListNode));
	newNode -> name = name;
	newNode -> next = head;
	head = newNode;
	return head;
}

struct ListNode* destructList(struct ListNode* root) {
	if (root == NULL) {
		return NULL;
	}
	root -> next = destructList(root -> next);
	free(root -> name);
	free(root);
	return NULL;
}

struct Vector* constructVector(unsigned int size) {
	struct Vector *obj = (struct Vector*)malloc(sizeof(struct Vector));
	obj -> data = (unsigned short*)malloc(size * sizeof(unsigned short));
	obj -> currentSize = 0;
	obj -> maxSize = size;
	return obj;
}

void insertVector(struct Vector* array, unsigned short id) {
	if (array -> currentSize == array -> maxSize) {
		unsigned int newMaxSize = array -> maxSize * 2 + 1;
		unsigned short* newData = (unsigned short*)malloc(newMaxSize * sizeof(unsigned short));
		for (int i = 0; i < array -> currentSize; i++) {
			newData[i] = array -> data[i];
		}
		//delete old data
		free(array -> data);
		//set pointer to new data location
		array -> data = newData;
		//update max size
		array -> maxSize = newMaxSize;
	}
	array -> data[array -> currentSize] = id;
	array -> currentSize++;
}

int searchArray(struct Vector* array, unsigned short id) {
	for (int i = 0; i < array -> currentSize; i++) {
		if (array -> data[i] == id) {
			return i;
		}
	}
	return -1;
}

void destructVector(struct Vector* array) {
	free(array -> data);
	free(array);
}

//Data block
struct DataBlock* constructDataBlock(unsigned int size) {
	struct DataBlock *obj = (struct DataBlock*)malloc(sizeof(struct DataBlock));
	obj -> data = (char*)malloc(size * sizeof(char));
	obj -> currentSize = 0;
	obj -> maxSize = size;
	return obj;
}

void insertDataBlock(struct DataBlock* array, char* segment) {
	if (array -> currentSize == array -> maxSize) {
		unsigned int newMaxSize = array -> maxSize * 2 + 1;
		char* newData = (char*)malloc(newMaxSize * sizeof(char));
		for (int i = 0; i < array -> currentSize; i++) {
			newData[i] = array -> data[i];
		}
		//delete old data
		free(array -> data);
		//set pointer to new data location
		array -> data = newData;
		//update max size
		array -> maxSize = newMaxSize;
	}
	array -> data[array -> currentSize] = *segment;
	array -> currentSize++;
}

void destructDataBlock(struct DataBlock* array) {
	free(array -> data);
	free(array);
}
