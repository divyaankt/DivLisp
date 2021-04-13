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
lval *lval_num(double x)
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

/*Destructor for lval struct field*/
void lval_del(lval *v)
{

    switch (v->type)
    {
    /*Do nothing special for number type*/
    case LVAL_NUM:
        break;

    /*For Err or Sym free the string that they store*/
    case LVAL_ERR:
        free(v->err);
        break;
    case LVAL_SYM:
        free(v->sym);
        break;

    /*If Sexpr the delete all elements inside*/
    case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++)
        {
            lval_del(v->cell[i]);
        }

        /*Also free the memory allocated to contain the pointers*/
        free(v->cell);
        break;
    }

    /*Free the memory allocated for the "lval" struct itself*/
    free(v);
}

lval *lval_add(lval *v, lval *x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval *) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval *lval_read_num(mpc_ast_t *t)
{
    /* Check if there is some error in conversion */
    errno = 0;
    double x = strtof(t->contents, NULL);
    return errno != ERANGE ? lval_num(x) : lval_err("Invalid Number!!");
}

lval *lval_read(mpc_ast_t *t)
{

    /*If String or Number return conversion to that type*/
    if (strstr(t->tag, "number"))
    {
        return lval_read_num(t);
    }

    if (strstr(t->tag, "symbol"))
    {
        return lval_sym(t->contents);
    }

    /*If root(>) or sexpr then create empty list*/
    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0)
    {
        x = lval_sexpr();
    }
    if (strstr(t->tag, "sexpr"))
    {
        x = lval_sexpr();
    }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++)
    {
        if (strcmp(t->children[i]->contents, "(") == 0)
        {
            continue;
        }
        if (strcmp(t->children[i]->contents, ")") == 0)
        {
            continue;
        }
        if (strcmp(t->children[i]->tag, "regex") == 0)
        {
            continue;
        }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_print(lval *v);

void lval_expr_print(lval *v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++)
    {

        /* Print Value contained within */
        lval_print(v->cell[i]);

        /* Don't print trailing space if last element */
        if (i != (v->count - 1))
        {
            putchar(' ');
        }
    }
    putchar(close);
}

/* Print an "lval" */
void lval_print(lval *v)
{
    switch (v->type)
    {
    case LVAL_NUM:
        printf("%lf", v->num);
        break;
    case LVAL_ERR:
        printf("Error: %s", v->err);
        break;
    case LVAL_SYM:
        printf("%s", v->sym);
        break;
    case LVAL_SEXPR:
        lval_expr_print(v, '(', ')');
        break;
    }
}
/* Print an "lval" followed by a newline */
void lval_println(lval *v)
{
    lval_print(v);
    putchar('\n');
}

int main(int argc, char **argv)
{

    /* Create Some Parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Function = mpc_new("function");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *DivLisp = mpc_new("divlisp");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                             \
    number   : /-?[0-9]+/;                                                  \
    symbol : '+' | '-' | '*' | '/' | '%' | '^';                             \
    function : /[_a-zA-Z]+/;                                                \
    sexpr    : '(' <expr>* ')';                                             \
    expr     : <number> | <symbol> | <function> | <sexpr>;                  \
    divlisp    : /^/ <expr>* /$/;                                           \
    ",
              Number, Symbol, Function, Sexpr, Expr, DivLisp);

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
            /*lval result = eval(r.output);
            lval_println(result);
            //mpc_ast_print(r.output);
            mpc_ast_delete(r.output);*/
            lval *x = lval_read(r.output);
            lval_println(x);
            lval_del(x);
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