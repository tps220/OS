#include<unordered_map>
#include<cassert>
#include<cstdio>
#include<cstring>
#include<map>
#include<iostream>
#include<string>
#include<vector>
#include<unistd.h>
#include<iomanip>
#include<fcntl.h>
#include<sys/wait.h>

using namespace std;

vector<size_t> *malloc_data = NULL;

//User Implementation of Data Structures
struct File {
    const char* name;
    long balance;
};

unordered_map<int, File> *files = NULL;

/* declare the following functions to have "C" linkage, so that
 * we can see them from C code without doing name demangling. */
extern "C"
{
	void so_deallocate()
	{
        delete files;
		delete malloc_data;
	}

	void so_allocate()
	{
        if (files == NULL) {
            files = new unordered_map<int, File>();
        }   
		if(malloc_data == NULL)
		{
			malloc_data = new vector<size_t>();
		}
	}

    /* dump() - output the contents of the malloc_data */
  //    void malloc_dump()
  //	{
  //		for(auto& data : *malloc_data)
  //		{
  //			printf("Allocations made: %ld.\n", data);
  //		}
  //	}

	/* insert() - when malloc() is called, the interpositioning library
     *            stores the size of the request.	*/
	int malloc_insert(size_t size)
	{
		so_allocate();
		malloc_data->push_back(size);
	}
    
    File createFile(const char* name, long balance) {
        File file;
        file.name = name;
        file.balance = balance;
        return file;
    }
    
    int insert(int fd, const char* name, long balance) {
        //if already another file descriptor with that number,
        //return an error
        if (files -> find(fd) != files -> end()) {
            return -1;
        }
        (*files)[fd] = createFile(name, balance);
        return 0;
    }

    //Update the balance of the file descriptor with the new
    //data provided
    int update(int fd, long balance) {
        //if there isn't a filedescriptor with that number,
        //return an error
        if (files -> find(fd) == files -> end()) {
            return -1;
        }
        File temp = (*files)[fd];
        temp.balance = balance;
        (*files)[fd] = temp;
        return 0;
    }

    int getFileBalance(int fd) {
        return (*files)[fd].balance;
    }

    const char* getFileName(int fd) {
        return (*files)[fd].name;
    }

    int erase(int fd) {
        if (files -> find(fd) == files -> end()) {
            return -1;
        }
        files -> erase(fd);
        return 0;
    }
    
    //O(n) solution to find the smallest file descriptor currently available
    //inside the map
    int findFd() {
        if (files -> empty()) {
            return 3;
        }

        int *arr = new int[files -> size()];
        int max = files -> size();
        int counter = 0;
        for (auto it = files -> begin(); it != files -> end(); ++it) {
            arr[counter] = it -> first - 3;
            counter++;
        }
        
        int found = 0;
        for (int i = 0; i < max; i++) {
            int val = abs(arr[i]);
            if (val < max) {
                if (arr[val] > 0) {
                    arr[val] *= -1;
                    found++;
                }
            }
        }

        if (found < max) {
            for (int i = 0; i < max; i++) {
                if (arr[i] > 0) {
                    free(arr);
                    return i + 3;
                }
            }
        }

        free(arr);
        return max + 3;
    }
}
