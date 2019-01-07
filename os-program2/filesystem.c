#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "support.h"
#include "structs.h"
#include "filesystem.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * generateData() - Converts source from hex digits to
 * binary data. Returns allocated pointer to data
 * of size amt/2.
 */
char* generateData(char *source, size_t size)
{
	char *retval = (char *)malloc((size >> 1) * sizeof(char));

	for(size_t i=0; i<(size-1); i+=2)
	{
		sscanf(&source[i], "%2hhx", &retval[i>>1]);
	}
	return retval;
}



/*
 * filesystem() - loads in the filesystem and accepts commands
 */
void filesystem(char *file)
{
	/* pointer to the memory-mapped filesystem */
	char *filesys = NULL;

	/*
	 * open file, handle errors, create it if necessary.
	 * should end up with map referring to the filesystem.
	 */
	 int fd = openFileSystem(file);
	 if (fd == -1) {
		 fprintf(stderr, "File system not found, attempting to create a new one\n");
		 filesys = createFileSystem(file);
		 if (filesys == NULL) {
			 fprintf(stderr, "Error in creating the file\n");
			 exit(1);
		 }
	 }
	 else {
		 filesys = mmap(0, 4194304, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	 }
	 int result = verifyFileSystem(filesys);
	 if (result == 0) {
		 fprintf(stderr, "File system invalid\n");
		 exit(1);
	 }

	/* You will probably want other variables here for tracking purposes */
	unsigned short currentDirectory = 0;

	/*
	 * Accept commands, calling accessory functions unless
	 * user enters "quit"
	 * Commands will be well-formatted.
	 */
	char *buffer = NULL;
	size_t size = 0;
	while(getline(&buffer, &size, stdin) != -1)
	{
		/* Basic checks and newline removal */
		size_t length = strlen(buffer);
		if(length == 0)
		{
			continue;
		}
		if(buffer[length-1] == '\n')
		{
			buffer[length-1] = '\0';
		}

		/* TODO: Complete this function */
		/* You do not have to use the functions as commented (and probably can not)
		 *	They are notes for you on what you ultimately need to do.
		 */

		if(!strcmp(buffer, "quit"))
		{
			break;
		}
		else if(!strncmp(buffer, "dump ", 5))
		{
			if(isdigit(buffer[5]))
			{
				dump(stdout, atoi(buffer + 5), filesys);
			}
			else
			{
				char *filename = buffer + 5;
				char *space = strstr(buffer+5, " ");
				*space = '\0';
				//open and validate filename
				FILE* fp = fopen(filename, "wb");
				if (fp == NULL) {
					fprintf(stderr, "Could not dump to file\n");
				}
				else {
					dumpBinary(fp, atoi(space + 1), filesys);
				}
			}
		}
		else if(!strncmp(buffer, "usage", 5))
		{
			usage(filesys);
		}
		else if(!strncmp(buffer, "pwd", 3))
		{
			char* path = pwdUser(filesys, currentDirectory);
			fprintf(stderr, "%s\n", path);
			free(path);
		}
		else if(!strncmp(buffer, "cd ", 3))
		{
			int result = cdUser(filesys, buffer+3, &currentDirectory);
			if (result == -1) {
				fprintf(stderr, "Directory does not exist\n");
			}
			else if (result == -2) {
				fprintf(stderr, "Not a diretory, you gave me a file\n");
			}
		}
		else if(!strncmp(buffer, "ls", 2))
		{
			lsUser(filesys, currentDirectory);
		}
		else if(!strncmp(buffer, "mkdir ", 6))
		{
			mkdirUser(buffer+6, currentDirectory, filesys);
		}
		else if(!strncmp(buffer, "cat ", 4))
		{
			cat(buffer + 4, filesys, currentDirectory);
		}
		else if(!strncmp(buffer, "write ", 6))
		{
			char *filename = buffer + 6;
			char *space = strstr(buffer+6, " ");
			*space = '\0';
			size_t amt = atoi(space + 1);
			space = strstr(space+1, " ");

			char *data = generateData(space+1, amt<<1);
			writeToFile(filename, amt, data, filesys, currentDirectory);
			free(data);
		}
		else if(!strncmp(buffer, "append ", 7))
		{
			char *filename = buffer + 7;
			char *space = strstr(buffer+7, " ");
			*space = '\0';
			size_t amt = atoi(space + 1);
			space = strstr(space+1, " ");

			char *data = generateData(space+1, amt<<1);
			appendToFile(filename, amt, data, filesys, currentDirectory);
			free(data);
		}
		else if(!strncmp(buffer, "getpages ", 9))
		{
			char *name = buffer + 9;
			getpages(name, currentDirectory, filesys);
		}
		else if(!strncmp(buffer, "get ", 4))
		{
			char *filename = buffer + 4;
			char *space = strstr(buffer+4, " ");
			*space = '\0';
			size_t start = atoi(space + 1);
			space = strstr(space+1, " ");
			size_t end = atoi(space + 1);
			getRange(filename, start, end, currentDirectory, filesys);
		}
		else if(!strncmp(buffer, "rmdir ", 6))
		{
			rmdirUser(buffer+6, &currentDirectory, filesys);
		}
		else if(!strncmp(buffer, "rm -rf ", 7))
		{
			currentDirectory = rmRF(filesys, currentDirectory, buffer + 7);
		}
		else if(!strncmp(buffer, "rm ", 3))
		{
			rmUser(buffer + 3, currentDirectory, filesys);
		}
		else if (!strncmp(buffer, "remove ", 7)) {
			char *filename = buffer + 7;
			char *space = strstr(buffer + 7, " ");
			*space = '\0';
			size_t start = atoi(space + 1);
			space = strstr(space+1, " ");
			size_t end = atoi(space + 1);
			rmRange(filesys, currentDirectory, filename, start, end);
		}
		else if(!strncmp(buffer, "scandisk", 8))
		{
			fprintf(stderr, "Our fileystem is valid, always :)\n");
		}
		else if(!strncmp(buffer, "undelete ", 9))
		{
			fprintf(stderr, "Redo logging is not yet a feature\n");
		}
		free(buffer);
		buffer = NULL;
	}
	free(buffer);
	buffer = NULL;

}

/*
 * help() - Print a help message.
 */
void help(char *progname)
{
	printf("Usage: %s [FILE]...\n", progname);
	printf("Loads FILE as a filesystem. Creates FILE if it does not exist\n");
	exit(0);
}

/*
 * main() - The main routine parses arguments and dispatches to the
 * task-specific code.
 */
int main(int argc, char **argv)
{
	/* for getopt */
	long opt;

	/* run a team name check */
	check_team(argv[0]);

	/* parse the command-line options. For this program, we only support */
	/* the parameterless 'h' option, for getting help on program usage. */
	while((opt = getopt(argc, argv, "h")) != -1)
	{
		switch(opt)
		{
		case 'h':
			help(argv[0]);
			break;
		}
	}

	if(argv[1] == NULL)
	{
		fprintf(stderr, "No filename provided, try -h for help.\n");
		return 1;
	}

	filesystem(argv[1]);
	return 0;
}
