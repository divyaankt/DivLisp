#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#define LASSERT(args, cond, err) \
    if (!(cond))                 \
    {                            \
        lval_del(args);          \
        return lval_err(err);    \
    }

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

/*Forward Declarations*/
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Create Enumeration of Possible lval Types */
enum
{
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,   //Operator of S-Expression/Q-Expression
    LVAL_SEXPR, //Actual S-Expression
    LVAL_QEXPR, //Actual Q-Expression
    LVAL_FUN    //Function Type
};

typedef lval *(*lbuiltin)(lenv *, lval *);

/*lenv struct*/
struct lenv
{
    int count;
    char **syms;
    lval **vals;
};

/* Declare New lval Struct */
struct lval
{
    int type;
    double num;

    /*Error and Symbol types contain strings*/
    char *err;
    char *sym;
    lbuiltin fun;

    /*Count and Pointer to a list of "lval*" */
    int count;
    lval **cell;
};

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

/* A pointer to a new empty Qexpr lval */
lval *lval_qexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* A pointer to a new empty Fun lval */
lval *lval_fun(lbuiltin func)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
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

    /*If Sexpr or Qexpr, then delete all elements inside*/
    case LVAL_QEXPR:
    case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++)
        {
            lval_del(v->cell[i]);
        }

    case LVAL_FUN:
        break;

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

    /*If root(>), sexpr or qexpr then create empty list*/
    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0)
    {
        x = lval_sexpr();
    }
    if (strstr(t->tag, "sexpr"))
    {
        x = lval_sexpr();
    }
    if (strstr(t->tag, "qexpr"))
    {
        x = lval_qexpr();
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
        if (strcmp(t->children[i]->contents, "}") == 0)
        {
            continue;
        }
        if (strcmp(t->children[i]->contents, "{") == 0)
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

/*This function is going to be useful when we put things into, and take things out of, the environment.*/
lval *lval_copy(lval *v)
{

    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type)
    {
        /* Copy Functions and Numbers Directly */
    case LVAL_FUN:
        x->fun = v->fun;
        break;
    case LVAL_NUM:
        x->num = v->num;
        break;

    /* Copy Strings using malloc and strcpy */
    case LVAL_ERR:
        x->err = malloc(strlen(v->err) + 1);
        strcpy(x->err, v->err);
        break;

    case LVAL_SYM:
        x->sym = malloc(strlen(v->sym) + 1);
        strcpy(x->sym, v->sym);
        break;

    /* Copy Lists by copying each sub-expression */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
        x->count = v->count;
        x->cell = malloc(sizeof(lval *) * x->count);
        for (int i = 0; i < x->count; i++)
        {
            x->cell[i] = lval_copy(v->cell[i]);
        }
        break;
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
    case LVAL_QEXPR:
        lval_expr_print(v, '{', '}');
        break;
    case LVAL_FUN:
        printf("<function>");
        break;
    }
}
/* Print an "lval" followed by a newline */
void lval_println(lval *v)
{
    lval_print(v);
    putchar('\n');
}

/*
The lval_pop function extracts a single element from an S-Expression at index i 
and shifts the rest of the list backward so that it no longer contains that lval*. 
It then returns the extracted value. 
*/
lval *lval_pop(lval *v, int i)
{
    /*Find the item at "i"*/
    lval *x = v->cell[i];

    //size_t newSize = sizeof(lval *) * (v->count - i - 1);
    /*Shift memory after the item at "i" over the top*/
    memmove(&v->cell[i], &v->cell[i + 1],
            (v->count - i - 1) * sizeof(lval *));

    /*Decrease the count of the items in the list*/
    v->count--;

    /*Reallocate the memory used*/
    v->cell = realloc(v->cell, sizeof(lval *) * v->count);

    return x;
}

/*
The lval_take function is similar to lval_pop but it deletes the list it has 
extracted the element from. This is like taking an element from the list and 
deleting the rest. It is a slight variation on lval_pop but it makes our code 
easier to read in some places. Unlike lval_pop, only the expression you take 
from the list needs to be deleted by lval_del.
*/
lval *lval_take(lval *v, int i)
{
    lval *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval *lval_join(lval *x, lval *y)
{

    /* For each cell in 'y' add it to 'x' */
    while (y->count)
    {
        x = lval_add(x, lval_pop(y, 0));
    }

    /* Delete the empty 'y' and return 'x' */
    lval_del(y);
    return x;
}

lval *builtin(lval *a, char *func);

lval *lval_eval_sexpr(lval *v);

/*Helper function to evaluate S-Expression*/
lval *lval_eval(lval *v)
{
    /*Evaluate Sexpressions*/
    if (v->type == LVAL_SEXPR)
        return lval_eval_sexpr(v);

    /*All other lval types remain the same*/
    return v;
}

/*Main function for evaluating S-Expressions*/
lval *lval_eval_sexpr(lval *v)
{

    /*Evaluate Children*/
    for (int i = 0; i < v->count; i++)
    {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    /*Error Checking*/
    for (int i = 0; i < v->count; i++)
    {
        if (v->cell[i]->type == LVAL_ERR)
            return lval_take(v, i);
    }

    /*Empty Expression*/
    if (v->count == 0)
        return v;

    /*Single Expression*/
    if (v->count == 1)
        return lval_take(v, 0);

    /*Ensure that first element is symbol*/
    lval *f = lval_pop(v, 0);

    if (f->type != LVAL_SYM)
    {
        lval_del(f);
        lval_del(v);
        return lval_err("S-Expression doesn't start with symbol!!");
    }

    /*Call builtin with operator*/
    lval *result = builtin(v, f->sym);

    lval_del(f);

    return result;
}

/*Evaluation function which performs switch on operator passed*/
lval *builtin_op(lval *a, char *op)
{
    /*First ensure all arguments are numbers*/
    for (int i = 0; i < a->count; i++)
    {
        if (a->cell[i]->type != LVAL_NUM)
        {
            lval_del(a);
            return lval_err("Cannot operate on non-numbers");
        }
    }

    /*Pop the first element*/
    lval *x = lval_pop(a, 0);

    /*If no arguments and sub then perform unary negation*/
    if ((strcmp(op, "-") == 0) && a->count == 0)
    {
        x->num = -x->num;
    }

    /* While there are still elements remaining */
    while (a->count > 0)
    {

        /* Pop the next element */
        lval *y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0)
        {
            x->num += y->num;
        }
        if (strcmp(op, "-") == 0)
        {
            x->num -= y->num;
        }
        if (strcmp(op, "*") == 0)
        {
            x->num *= y->num;
        }
        if (strcmp(op, "/") == 0)
        {
            if (y->num == 0)
            {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division By Zero!");
                break;
            }
            x->num /= y->num;
        }
        if (strcmp(op, "%") == 0)
        {
            x->num = fmod(x->num, y->num);
        }
        if (strcmp(op, "^") == 0)
        {
            x->num = (double)(pow(x->num, y->num));
        }
        lval_del(y);
    }

    lval_del(a);
    return x;
}

//Implementation of head function
lval *builtin_head(lval *a)
{
    /*Check Error Conditions*/
    LASSERT(a, a->count == 1,
            "Function 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'head' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
            "Function 'head' passed {}!");

    /*Otherwise take first argument*/
    lval *v = lval_take(a, 0);

    /* Delete all elements that are not head and return */
    while (v->count > 1)
    {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

//Implementation of tail function
lval *builtin_tail(lval *a)
{
    /* Check Error Conditions */
    LASSERT(a, a->count == 1,
            "Function 'tail' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'tail' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
            "Function 'tail' passed {}!");

    /* Take first argument */
    lval *v = lval_take(a, 0);

    /* Delete first element and return */
    lval_del(lval_pop(v, 0));
    return v;
}

//Implementation of list function
//It just converts the input S-Expression to a Q-Expression and returns it.
lval *builtin_list(lval *a)
{
    a->type = LVAL_QEXPR;
    return a;
}

//Implementation of eval function
//It takes as input some single Q-Expression, which it converts to an S-Expression, and evaluates using lval_eval.
lval *builtin_eval(lval *a)
{
    LASSERT(a, a->count == 1,
            "Function 'eval' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' passed incorrect type!");

    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

//Implementation of join function
lval *builtin_join(lval *a)
{

    for (int i = 0; i < a->count; i++)
    {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect type.");
    }

    lval *x = lval_pop(a, 0);

    while (a->count)
    {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

//Implemented the builtin function len
lval *builtin_len(lval *a)
{
    /* Check Error Conditions */
    LASSERT(a, a->count == 1,
            "Function 'len' passed too many arguments, only 1 is required!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'len' passed incorrect type!");

    int listLen = a->cell[0]->count;

    return lval_num(listLen);
}

//Implemented the builtin function init
//It returns all of a Q-Expression except the final element.
lval *builtin_init(lval *a)
{
    /* Check Error Conditions */
    LASSERT(a, a->count == 1,
            "Function 'init' passed too many arguments, only 1 is required!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'init' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
            "Function 'init' passed {}, operation undefined for passed argument!");

    int lastItemIndex = a->cell[0]->count - 1;

    lval *v = lval_take(a, 0);
    lval_pop(v, lastItemIndex);
    return v;
}

lval *builtin(lval *a, char *func)
{
    if (strcmp("list", func) == 0)
    {
        return builtin_list(a);
    }
    if (strcmp("head", func) == 0)
    {
        return builtin_head(a);
    }
    if (strcmp("tail", func) == 0)
    {
        return builtin_tail(a);
    }
    if (strcmp("join", func) == 0)
    {
        return builtin_join(a);
    }
    if (strcmp("eval", func) == 0)
    {
        return builtin_eval(a);
    }
    if (strcmp("len", func) == 0)
    {
        return builtin_len(a);
    }
    if (strcmp("init", func) == 0)
    {
        return builtin_init(a);
    }
    if (strstr("+-/*", func))
    {
        return builtin_op(a, func);
    }
    lval_del(a);
    return lval_err("Unknown Function!");
}

lenv *lenv_new(void)
{
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;

    return e;
}

void lenv_del(lenv *e)
{
    for (int i = 0; i < e->count; i++)
    {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }

    free(e->syms);
    free(e->vals);
    free(e);
}

lval *lenv_get(lenv *e, lval *k)
{

    /*Iterate over all items in the environment*/
    for (int i = 0; i < e->count; i++)
    {
        /* Check if the stored string matches the symbol string */
        /* If it does, return a copy of the value */
        if (strcmp(e->syms[i], k->sym) == 0)
        {
            return lval_copy(e->vals[i]);
        }
    }

    /*If no symbol found return error*/
    return lval_err("Unbound Symbol");
}

void lenv_put(lenv *e, lval *k, lval *v)
{

    /* Iterate over all items in environment */
    /* This is to see if variable already exists */
    for (int i = 0; i < e->count; i++)
    {

        /* if the variable is found delete the item at that position */
        /* And replace with variable supplied by the user */
        if (strcmp(e->syms[i], k->sym) == 0)
        {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    /* if no existing entry found, allocate space for new entry */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval *) * e->count);
    e->syms = realloc(e->syms, sizeof(lval *) * e->count);

    /* Copy contents of lval and symbol string into new location */
    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

int main(int argc, char **argv)
{

    /* Create Some Parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Qexpr = mpc_new("qexpr");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *DivLisp = mpc_new("divlisp");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                             \
    number   : /-?[0-9]+/ ;                                                  \
    symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                             \
    sexpr    : '(' <expr>* ')' ;                                             \
    qexpr    : '{' <expr>* '}' ;                                             \
    expr     : <number> | <symbol> | <sexpr> | <qexpr> ;                    \
    divlisp    : /^/ <expr>* /$/ ;                                           \
    ",
              Number, Symbol, Sexpr, Qexpr, Expr, DivLisp);

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
            lval *x = lval_eval(lval_read(r.output));
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
    mpc_cleanup(6, DivLisp, Sexpr, Qexpr, Expr, Number, Symbol);

    return 0;
}