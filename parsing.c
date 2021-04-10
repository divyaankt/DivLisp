#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

//If we are compiling on a Windows, include these functions
#ifdef _WIN32
#include <string.h>

//Custom implementation of readline()
char *readline(char *prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);

    char *cpy = malloc(strlen(buffer) + 1);

    strcpy(cpy, buffer);

    cpy[strlen(cpy) - 1] = '\0';

    return cpy;
}

//Custom implementation of add_history()
void add_history(char *unused) {}

#else
//readline is used to read input from some prompt, while allowing for editing of that input.
#include <editline/readline.h>
//add_history lets us record the history of inputs so that they can be retrieved with the up and down arrows.
#include <editline/history.h>
#endif

int main(int argc, char **argv)
{

    /*Print version and Exit Option*/
    puts("DivLisp version 0.1");
    puts("Press Ctrl+C to Exit\n");

    /*REPL Loop*/
    while (1)
    {

        /*Output REPL prompt and get input*/
        char *input = readline("DivLisp> ");

        /*Add input to history*/
        add_history(input);

        /*Echo back to the user*/
        printf("No you're a %s\n", input);

        /*Free retrieved input*/
        free(input);
    }

    return 0;
}