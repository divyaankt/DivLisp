#include "mpc.h"
#include <math.h>

#ifdef _WIN32

static char buffer[2048];

char *readline(char *prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

void add_history(char *unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/*Applying switch on operator to evaluate expression*/
long eval_op(long x, char *op, long y)
{

    if (strcmp(op, "+") == 0)
    {
        return x + y;
    }
    if (strcmp(op, "-") == 0)
    {
        return x - y;
    }
    if (strcmp(op, "*") == 0)
    {
        return x * y;
    }
    if (strcmp(op, "/") == 0)
    {
        return x / y;
    }
    if (strcmp(op, "%") == 0)
    {
        return x % y;
    }
    if (strcmp(op, "^") == 0)
    {
        return (long)(pow(x, y));
    }
    if (strcmp(op, "min") == 0)
    {
        return (x < y ? x : y);
    }
    if (strcmp(op, "max") == 0)
    {
        return (x < y ? y : x);
    }
    return 0;
}

long eval(mpc_ast_t *t)
{
    /*If node tagged as number, return value directly*/
    if (strstr(t->tag, "number"))
    {
        return atoi(t->contents);
    }

    /*Second child of the node is always the operator*/
    char *op = t->children[1]->contents;

    /*We store third child in x*/
    long x = eval(t->children[2]);

    //The number of children is 4, when there is only one operand.
    if (t->children_num == 4)
    {
        if (strcmp(op, "-") == 0)
            return -1 * x;
        if (strcmp(op, "+") == 0)
            return x;
    }
    else
    {
        /*Iterate the remaining children and evaluate recursively*/
        int i = 3;
        while (strstr(t->children[i]->tag, "expr"))
        {
            x = eval_op(x, op, eval(t->children[i]));
            i++;
        }
    }

    return x;
}

int main(int argc, char **argv)
{

    /* Create Some Parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Operator = mpc_new("operator");
    mpc_parser_t *Function = mpc_new("function");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *DivLisp = mpc_new("lispy");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                     \
    number   : /-?[0-9]+/ ;                             \
    operator : '+' | '-' | '*' | '/' | '%' | '^' ;                  \
    function : /[a-zA-Z]+/;                             \
    expr     : <number> | '(' <operator> <expr>+ ')' | '(' <function> <expr>+ ')';  \
    lispy    : /^/ (<operator> | <function>) <expr>+ /$/;             \
    ",
              Number, Operator, Function, Expr, DivLisp);

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

        /*Parse the user input*/
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, DivLisp, &r))
        {
            /*On success print result and delete the AST*/
            long result = eval(r.output);
            printf("%li\n", result);
            //mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
        }
        else
        {
            /*Otherwise print and delete the Error*/
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        /*Free retrieved input*/
        free(input);
    }

    /* Cleanup our parser*/
    mpc_cleanup(4, Number, Operator, Expr, DivLisp);

    return 0;
}