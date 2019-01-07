#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"support.h"

/* hello() - print some output so we know the shared object loaded.
 *
 * The function signature takes an untyped parameter, and returns an untyped
 * parameter. In this way, the signature *could* support any behavior, by
 * passing in a struct and returning a struct. */
void *hello(void *input)
{
	printf("hello from a .so\n");
	return NULL;
}

//--------------------------Task 1a Implementation-------------------------//

//Returns whether the character provided is a lowercase letter
int isLowerCase(const char ch) {
    return ch >= 'a' && ch <= 'z';
}

//Copies the string into a new string, and makes all lower case letters upper case
char* copy(char *str) {
    if (str == NULL) {
        return NULL;
    }
    int size = strlen(str);
    char* new_param = (char*)malloc((size + 1) * sizeof(char));
    for (int i = 0; i < size; i++) {
        if (isLowerCase(str[i])) {
            new_param[i] = str[i] ^ 32;
        }
        else {
            new_param[i] = str[i];
        }
    }
    new_param[size] = '\0';
    return new_param;
}

//Creates a new team struct that copies the old team struct's data fields, but
//switches lowercase letters of the names and emails to uppercase letters
void* ucase(void *input) {
    if (input == NULL) {
        return NULL;
    }
    struct team_t *newTeam = (struct team_t*)malloc(sizeof(struct team_t));
    newTeam -> name1 = copy(((struct team_t*)input) -> name1);
    newTeam -> email1 = copy(((struct team_t*)input) -> email1);
    newTeam -> name2 = copy(((struct team_t*)input) -> name2);
    newTeam -> email2 = copy(((struct team_t*)input) -> email2);
    newTeam -> name3 = copy(((struct team_t*)input) -> name3);
    newTeam -> email3 = copy(((struct team_t*)input) -> email3);
    return newTeam;
}

//Destructor for the team object
void* destroyTeam(void* obj) {
    free(((struct team_t*)obj) -> name1); 
    free(((struct team_t*)obj) -> email1); 
    free(((struct team_t*)obj) -> name2); 
    free(((struct team_t*)obj) -> email2); 
    free(((struct team_t*)obj) -> name3); 
    free(((struct team_t*)obj) -> email3); 
    free((struct team_t*)obj);
    return NULL;
}
