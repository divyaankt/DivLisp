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

/* Create Enumeration of Possible Error Types */
enum
{
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

/* Create Enumeration of Possible lval Types */
enum
{
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,  //Operator of S-Expression
    LVAL_SEXPR //Actual S-Expression
};

/* Declare New lval Struct */
typedef struct lval
{
    int type;
    double num;

    /*Error and Symbol types contain strings*/
    char *err;
    char *sym;

    /*Count and Pointer to a list of "lval*" */
    int count;
    struct lval **cell;
} lval;

/* Construct a pointer to a new Number lval */
lval *lval_num(long x)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Construct a pointer to a new Error lval */
lval *lval_err(char *m)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

/* Construct a pointer to a new Symbol lval */
lval *lval_sym(char *s)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* A pointer to a new empty Sexpr lval */
lval *lval_sexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Print an "lval" */
void lval_print(lval v)
{
    switch (v.type)
    {
    /* In the case the type is a number print it */
    /* Then 'break' out of the switch. */
    case LVAL_NUM:
        printf("%lf", v.num);
        break;

    /* In the case the type is an error */
    case LVAL_ERR:
        /* Check what type of error it is and print it */
        if (v.err == LERR_DIV_ZERO)
        {
            printf("Error: Division By Zero!");
        }
        if (v.err == LERR_BAD_OP)
        {
            printf("Error: Invalid Operator!");
        }
        if (v.err == LERR_BAD_NUM)
        {
            printf("Error: Invalid Number!");
        }
        break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval v)
{
    lval_print(v);
    putchar('\n');
}

lval eval_op(lval x, char *op, lval y)
{

    /* If either value is an error return it */
    if (x.type == LVAL_ERR)
    {
        return x;
    }
    if (y.type == LVAL_ERR)
    {
        return y;
    }

    /* Otherwise do maths on the number values */
    if (strcmp(op, "+") == 0)
    {
        return lval_num(x.num + y.num);
    }
    if (strcmp(op, "-") == 0)
    {
        return lval_num(x.num - y.num);
    }
    if (strcmp(op, "*") == 0)
    {
        return lval_num(x.num * y.num);
    }
    if (strcmp(op, "/") == 0)
    {
        /* If second operand is zero return error */
        return y.num == 0
                   ? lval_err(LERR_DIV_ZERO)
                   : lval_num(x.num / y.num);
    }

    if (strcmp(op, "%") == 0)
    {
        return lval_num(fmod(x.num, y.num));
    }
    if (strcmp(op, "^") == 0)
    {
        return lval_num((double)(pow(x.num, y.num)));
    }
    if (strcmp(op, "min") == 0)
    {
        return lval_num((x.num < y.num ? x.num : y.num));
    }
    if (strcmp(op, "max") == 0)
    {
        return lval_num((x.num < y.num ? y.num : x.num));
    }

    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *t)
{

    if (strstr(t->tag, "number"))
    {
        /* Check if there is some error in conversion */
        errno = 0;
        double x = strtod(t->contents, NULL);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    char *op = t->children[1]->contents;
    lval x = eval(t->children[2]);

    //The number of children is 4, when there is only one operand.
    if (t->children_num == 4)
    {
        if (strcmp(op, "-") == 0)
            return lval_num(-1 * x.num);
        if (strcmp(op, "+") == 0)
            return lval_num(x.num);
    }
    else
    {
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
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Function = mpc_new("function");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *DivLisp = mpc_new("lispy");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                     \
    lispy    : /^/ (<symbol> | <function>) <expr>+ /$/;             \
    sexpr    : '(' <expr>* ')';                                 \
    expr     : <number> | '('(<symbol> | <function>)<expr>+')' ; \
    number   : /-?[0-9]+.?[0-9]*/;                                             \
    symbol : '+' | '-' | '*' | '/' | '%' | '^';                         \
    function : /[_a-zA-Z]+/;                                              \
            ",
              DivLisp, Sexpr, Expr, Number, Symbol, Function);

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
            lval result = eval(r.output);
            lval_println(result);
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
    mpc_cleanup(5, DivLisp, Sexpr, Expr, Number, Symbol, Function);

    return 0;
}