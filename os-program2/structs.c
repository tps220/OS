#include "structs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//Boot helper methods
struct BootSector constructBootDataStructure(unsigned int reservedSectors,
                                             unsigned int bytesPerSector,
                                             unsigned int numberOfPages,
                                             unsigned int numberOfFatEntries,
                                             unsigned int ptrToFAT,
                                             unsigned int ptrToData,
                                             unsigned int totalSize)
{
  struct BootSector construct;
  construct.reservedSectors = reservedSectors;
  construct.bytesPerSector = bytesPerSector;
  construct.numberOfPages = numberOfPages;
  construct.numberOfFatEntries = numberOfFatEntries;
  construct.ptrToFAT = ptrToFAT;
  construct.ptrToData = ptrToData;
  construct.totalSize = totalSize;
  return construct;
};



/*
The boot sectors initial values should be as follows:
struct BootSector BootTable = { 512 //bytesPerSector, 8126 //numberOfTableEntires,
  64 //numberOfTableBlocks, 512 //ptrToAlloc, 1 * 512 + 64 * 512, //ptrToTable
  65 //reservedBlocks, 0 //fileAllocationBlocks
};
*/
/*
reservedSectors: 1 --> Boot Sector
bytesPerSector: 512
numberOfPages: 8192
numberOfFatEntries: 8127
ptrToFAT: 512 byte offest
ptrToData: 1 * 512 (root) + 64 * 512 (FAT entries)
totalSize: 4194304

 */
struct BootSector getBootInfo() {
  return constructBootDataStructure(1, 512, 8192, 8127, 512, 65 * 512, 4194304);
}

struct BootSector* getBootSector(char* filesystem) {
  return (struct BootSector*)filesystem;
}

//Alloc helper methods
unsigned int getDataLocation(unsigned short id) {
  return (65 + (unsigned int)id) * 512;
}

unsigned int getEntryLocation(unsigned short id) {
  return 512 + (unsigned int)id * 4;
}

char* getName(char* filesys, unsigned short id) {
  struct Alloc* entry = getAllocStructure(filesys, id);
  char* entryName;
  if (isFile(entry)) {
    entryName = fileGetName(getFile(id, filesys), filesys);
  }
  else {
    entryName = directoryGetName(getDirectory(id, filesys), filesys);
  }
  return entryName;
}

struct Alloc* getAllocStructure(char* fileSystem, unsigned short id){
  unsigned int entryLocation = getEntryLocation(id);
  struct Alloc *newAlloc = (struct Alloc *)(fileSystem + entryLocation);
  return newAlloc;
}

void setNextId(struct Alloc* newAlloc, unsigned short nextId) {
  newAlloc -> nextId = nextId;
}

void setAttributes(struct Alloc* newAlloc, unsigned short attributes) {
  newAlloc -> attributes = attributes;
}

int isFile(struct Alloc* newAlloc) {
  return newAlloc -> attributes & (1 << 7);
}

int isDirectory(struct Alloc* newAlloc) {
  return newAlloc -> attributes & (1 << 6);
}

int isFull(struct Alloc* entry) {
  return entry -> attributes & (1 << 3);
}

int isFree(struct Alloc* entry) {
  return entry -> attributes == 0;
}

//File helper methods
char* fileGetName(struct File *file, char* filesys) {
  if (hasLongFileName(file)) {
    return getLongName(atoi(file -> filename), filesys);
  }
  char* name = (char*)malloc(8 + 1 + 3 + 1); //length = filename + '.' + extensions + '\0'
  int filenameSize = 0;
  //Copy in the filename
  for (int i = 0; i < 8 && file -> filename[i] != 0; i++) {
    name[i] = file -> filename[i];
    filenameSize++;
  }
  if (file -> extension[0] != 0) {
    name[filenameSize] = '.';
    filenameSize++;
    for (int i = 0; i < 3 && file -> extension[i] != 0; i++) {
      name[filenameSize] = file -> extension[i];
      filenameSize++;
    }
  }
  name[filenameSize] = '\0';
  return name;
}

struct File* getFile(unsigned short id, char* fileSystem) {
  unsigned int location = getDataLocation(id);
  struct File *file = (struct File*)(fileSystem + location);
  return file;
}

struct File* createFile(char* filesystem, unsigned short id, char* filename, char* extension, char attributes, unsigned short clusterStart, unsigned int fileSize) {
  struct File* newFile = (struct File*)(filesystem + getDataLocation(id));
  setAttributes(getAllocStructure(filesystem, id), 1 << 7 | 1); //temporarily lock this up
  newFile -> clusterStart = clusterStart;
  newFile -> fileSize = fileSize;
  if (strlen(filename) < 13) {
    int extensionStart = -1;
    int fileNameLength = 0;
    for (int i = 0; i < strlen(filename); i++) {
      if (filename[i] == '.') {
        extensionStart = i;
        break;
      }
      fileNameLength++;
    }
    int extensionLength = 0;
    if (extensionStart != -1) {
      for (int i = extensionStart + 1; i < strlen(filename); i++) {
        extensionLength++;
      }
    }

    //if there were more than 8 characters in filename w/out extension
    if (fileNameLength > 8 || extensionLength > 3) {
      makeLongFileName(newFile, filename, filesystem);
    }
    else if (extensionLength == 0) {
      memset(newFile -> filename, '\0', 8);
      memcpy(newFile -> filename, filename, fileNameLength);
      memset(newFile -> extension, '\0', 3);
    }
    else {
      memcpy(newFile -> filename, filename, fileNameLength);
      memcpy(newFile -> extension, filename + extensionStart + 1, extensionLength);
    }
  }
  else {
    makeLongFileName(newFile, filename, filesystem);
  }
  return newFile;
}

//Directory Helper Methods
char* directoryGetName(struct Directory* directory, char* filesys) {
  if (hasLongDirectoryName(directory)) {
    return getLongName(atoi(directory -> name), filesys);
  }
  char* name = (char*)malloc(11 + 1);
  int i = 0;
  while (i < 12 && directory -> name[i] != 0) {
    name[i] = directory -> name[i];
    i++;
  }
  name[i] = '\0';
  return name;
}

int hasLongDirectoryName(struct Directory* directory) {
  return directory -> attributes & 2;
}

struct Directory* getDirectory(unsigned short id, char* fileSystem) {
  unsigned int location = getDataLocation(id);
  struct Directory *directory = (struct Directory*)(fileSystem + location);
  return directory;
}

char* getDirectoryData(unsigned short currentDirectoryId, char* filesys) {
	return filesys + getDataLocation(currentDirectoryId) + 32;
}

unsigned int getDirectorySize(unsigned short directorySize, char* filesys) {
  return getDirectory(directorySize, filesys) -> directorySize;
}

struct Directory* constructDirectoryStructure(char* filesystem, unsigned short id, char* name, char attributes, unsigned short clusterStart, unsigned int directorySize) {
  struct Directory* newDirectory = getDirectory(id, filesystem);
  if (strlen(name) < 12) {
    //not a long file name
    memset(newDirectory -> name, 0, 11);
    memcpy(newDirectory -> name, name, strlen(name));
    newDirectory -> attributes = 0;
  }
  else {
    //handle long file name
    if (makeLongDirectoryName(newDirectory, name, filesystem) == -1) {
      return NULL;
    }
    newDirectory -> attributes = (1 << 1);
  }
  newDirectory -> clusterStart = clusterStart;
  newDirectory -> directorySize = directorySize;
  return newDirectory;
}

int min(int a, int b) {
	if (a < b) {
		return a;
	}
	return b;
}


unsigned short getEntry(char* fileSystem, int entrysize) {
	//TODO: CHECK OVERFLOW
	int numPages = entrysize / 512 + 1;
	if (numPages > NUMBER_OF_ENTRIES) {
		return 0;
	}
	unsigned short* freeSpaces = (unsigned short*)malloc(numPages * sizeof(unsigned short));
	unsigned short j = 0;
	for (unsigned short i = 0; i < NUMBER_OF_ENTRIES; i++) {
		struct Alloc *temp = getAllocStructure(fileSystem, i);
		if (isFree(temp)) {
			freeSpaces[j] = i;
			j++;
			if (j == numPages) {
				break;
			}
		}
	}

	if (j == numPages) {
		//if full, then connect all the ones together!
		for (int i = 0; i < numPages - 1; i++) {
			struct Alloc *newAlloc = getAllocStructure(fileSystem, freeSpaces[i]);
			newAlloc -> nextId = freeSpaces[i + 1];
		}
		unsigned short retval = freeSpaces[0];
		free(freeSpaces);
		return retval;
	}
	free(freeSpaces);
	return 0;
}

struct LongFileName* getLongFileNameStructure(char* filesys, unsigned short id) {
  unsigned int location =  getDataLocation(id);
  struct LongFileName* obj = (struct LongFileName*)(filesys + location);
  return obj;
}

int hasLongFileName(struct File* file) {
  return (file -> attributes) & 2;
}

char* getLongName(unsigned short id, char* filesys) {
  struct Alloc* entry = getAllocStructure(filesys, id);
  struct LongFileName* obj = getLongFileNameStructure(filesys, id);
  char* name = (char*)malloc((obj -> size + 1) * sizeof(char));
  int dataLeft = obj -> size;

  int offset = LONGFILENAMEOFFSET;
  unsigned int counter = 0;
  while (dataLeft > 0) {
      char* data = filesys + getDataLocation(id);
      int amountToRead = min(dataLeft, PAGE_SIZE - offset);
      memcpy(name + counter, data + offset, amountToRead);
      dataLeft -= amountToRead;
      counter += amountToRead;
      offset = 0;
      id = entry -> nextId;
      entry = getAllocStructure(filesys, entry -> nextId);
  }
  name[obj -> size] = '\0';
  return name;
}

int makeLongDirectoryName(struct Directory* directory, char* data, char* filesys) {
  int length = strlen(data);
  unsigned short entries = getEntry(filesys, length + LONGFILENAMEOFFSET);
  if (entries == 0) {
    fprintf(stdout, "Name too long to store in system\n");
    return -1;
  }
  struct Alloc* entry = getAllocStructure(filesys, entries);
  struct LongFileName* obj = getLongFileNameStructure(filesys, entries);
  obj -> size = length;
  obj -> clusterStart = entries;
  obj -> FAT = directory -> clusterStart;

  int dataLeft = length;
  int offset = LONGFILENAMEOFFSET;
  int counter = 0;
  while (dataLeft > 0) {
    char* page = filesys + getDataLocation(entries);
    int amountToWrite = min(dataLeft, PAGE_SIZE - offset);
    memcpy(page + offset, data + counter, amountToWrite);

    offset = 0;
    dataLeft -= amountToWrite;
    counter += amountToWrite;
    entries = entry -> nextId;
    setAttributes(entry, entry -> attributes | 1);
    entry = getAllocStructure(filesys, entry -> nextId);
  }

  char location[20];
  sprintf(location, "%d", obj -> clusterStart);
  int locationSize = strlen(location);
  memset(directory -> name, '\0', 11);
  memcpy(directory -> name, location, locationSize);
  directory -> attributes = (1 << 1);
  return 0;
}

int makeLongFileName(struct File* file, char* data, char* filesys) {
  int length = strlen(data);
  unsigned short entries = getEntry(filesys, length + LONGFILENAMEOFFSET);
  if (entries == 0) {
    fprintf(stdout, "Name too long to store in system\n");
    return -1;
  }
  struct Alloc* entry = getAllocStructure(filesys, entries);
  struct LongFileName* obj = getLongFileNameStructure(filesys, entries);
  obj -> size = length;
  obj -> clusterStart = entries;
  obj -> FAT = file -> clusterStart;

  int dataLeft = length;
  int offset = 32;
  int counter = 0;
  while (dataLeft > 0) {
    char* page = filesys + getDataLocation(entries);
    int amountToWrite = min(dataLeft, PAGE_SIZE - offset);
    memcpy(page + offset, data + counter, amountToWrite);

    offset = 0;
    dataLeft -= amountToWrite;
    counter += amountToWrite;
    setAttributes(entry, entry -> attributes | 1);
    entries = entry -> nextId;
    entry = getAllocStructure(filesys, entry -> nextId);
  }

  char location[20];
  sprintf(location, "%d", (int) (obj -> clusterStart));
  int locationSize = strlen(location);
  memcpy(file -> filename, location, locationSize);
  file -> attributes = (1 << 1);
  return 0;
}
