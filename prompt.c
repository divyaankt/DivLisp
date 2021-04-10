#include <stdio.h>
#include <stdlib.h>
//readline is used to read input from some prompt, while allowing for editing of that input.
#include <editline/readline.h>
//add_history lets us record the history of inputs so that they can be retrieved with the up and down arrows.
#include <editline/history.h>

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