#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include"support.h"

/* help() - Print a help message. */
void help(char *progname)
{
	printf("Usage: %s\n", progname);
	printf("(This is just a demonstration of the traditional way\n");
	printf(" to use .so files)\n");
}

/* declare the hello() function that lives in a shared library */
extern void *hello(void* param);

/* main() - The main routine parses arguments and invokes hello */
int main(int argc, char **argv)
{
	/* for getopt */
	long opt;

	/* run a student name check */
	check_team(argv[0]);

	/* parse the command-line options. For this program, we only support */
	/* the parameterless 'h' option, for getting help on program usage.  */
	while((opt = getopt(argc, argv, "h")) != -1)
	{
		switch(opt)
		{
		case 'h': 	help(argv[0]); 	break;
		}
	}

	hello(NULL);

	/* TODO: execute the new function "ucase" that you added to libpart1.c */
    struct team_t tester = {"boston1","newYork2","Chicago@3","SanFrancisco4","LA5","@%#$#$@6"};
    struct team_t *obj = ucase((void*)(&tester));
	printf("Student 1 : %s\n", obj -> name1);
	printf("Email 1   : %s\n", obj -> email1);
	printf("Student 2 : %s\n", obj -> name2);
	printf("Email 2   : %s\n", obj -> email2);
	printf("Student 3 : %s\n", obj -> name3);
	printf("Email 3   : %s\n", obj -> email3);
    destroyTeam((void*)obj);
	return 0;
}
