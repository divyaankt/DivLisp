#include <stdio.h>

//Input buffer
static char input[2048];

int main(int argc, char **argv)
{

    /*Print version and Exit Option*/
    puts("DivLisp version 0.1");
    puts("Press Ctrl+C to Exit\n");

    /*REPL Loop*/
    while (1)
    {

        /*REPL Output Prompt*/
        fputs("DivLisp> ", stdout);

        /*Read user input of maximum length 2048*/
        fgets(input, 2048, stdin);

        /*Echo back to the user*/
        printf("No you're a %s", input);
    }

    return 0;
}