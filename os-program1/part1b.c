#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include"support.h"
#include <dlfcn.h>

/* load_and_invoke() - load the given .so and execute the specified function */
//This function uses dynamic library system calls to open a shared library,
//find a specified function, and invoke the function.
void load_and_invoke(char *libname, char *funcname)
{
    char *error;
    void (*fnct)(void*);
    void *handle = dlopen(libname, RTLD_LAZY);
    if (!handle) {
        printf("Error in attempting to open shared library %s\n", dlerror());
        exit(1);
    }
    
    fnct = dlsym(handle, funcname);
    error = dlerror();
    if (error != NULL) {
        printf("Error in attempting to find function in shared library %s\n", error);
        exit(1);
    }
    
    (*fnct)(NULL);
    dlclose(handle);
}

/* help() - Print a help message. */
void help(const char *progname)
{
	printf("Usage: %s [OPTIONS]\n", progname);
	printf("Load the given .so and run the requested function from that .so\n");
    printf("You must provide both of these command line argument options: \n");
	printf("  -l [string] The name of the .so to load\n");
	printf("  -f [string] The name of the function within that .so to run\n");
}

/* main() - The main routine parses arguments and invokes hello */
int main(int argc, char **argv)
{
	/* for getopt */
	long opt;
	char* libname = NULL;
	char* funcname = NULL;
	
	/* parse the command-line options. For this program, we only support */
	/* the parameterless 'h' option, for getting help on program usage.  */
	while(((opt = getopt(argc, argv, "l:f:h")) != -1))
	{
		switch(opt)
		{
      		case 'l': 
                libname = optarg;
                break;
		    case 'f': 
                funcname = optarg;
                break;
            case 'h':
            default:
                help("Dynamically Load a Library");
                return 1;
		}
	}
    
    if (libname == NULL || funcname == NULL) {
        printf("Your arguments aren't in the proper format. Follow this guideline: \n\n");
        help("Dynamically Load a Library");
        return 1;
    }

	/* call load_and_invoke() to run the given function of the given library */
    load_and_invoke(libname, funcname);
	exit(0);
}

