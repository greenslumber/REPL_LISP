#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include "mpc.h"

//this section is to allow portability by using preprocessor commands
#ifdef _WIN32

static char buffer[2048];

//fake readline function 
char* readline(char* prompt) {
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer)+1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\n';
	return cpy;
}

//fake add history function
void add_history(char* unsused){}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

//version number - only held here
static char* versionNumber = "0.0.0.1";

//forward declarations
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;


// *** LISP VALUE ***

//possible lval types for lval.type field
enum{LVAL_NUM, LVAL_ERR, LVAL_SYM, 
  LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR};

//function pointer
typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval{

	int type;

  //basic info
	long num;
	char* err;
	char* sym;

  //functions
  lbuiltin builtin;
	
  //count and Pointer to list of lval
	int count;
	lval** cell;
};

//create lval of number type
lval* lval_num(long x){
	lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
	v->num = x;
	return v;
}

//create lval of error type
lval* lval_err(char* m){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(m)+1); //adding null byte
	strcpy(v->err, m);
	return v;
}

//create pointer to new symbol lval
lval* lval_sym(char* s){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s)+1); //adding null byte
	strcpy(v->sym, s);
	return v;
}

//enumerate error types
enum{LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

lval* lval_builtin(lbuiltin func){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;
  return v;
}

//pointer to new empty QEXPR lval
lval* lval_qexpr(void){
	lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

//create pointer to new empty Sexpr lval
lval* lval_sexpr(void){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

void lval_del(lval* v) {

  switch (v->type) {
    /* Do nothing special for number or function pointer type */
    case LVAL_FUN:
    case LVAL_NUM: break;


    /* For Err or Sym free the string data */
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;

    /* If Qexpr or Sexpr then delete all elements inside */
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      /* Also free the memory allocated to contain the pointers */
      free(v->cell);
    break;
  }

  /* Free the memory allocated for the "lval" struct itself */
  free(v);
}

lval* lval_copy(lval* v){
  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch(v->type){

    //copy functions and numbers directly
    case LVAL_FUN: x->builtin = v->builtin; break;
    case LVAL_NUM: x->num = v->num; break;

    //Copy strings using malloc and strcpy
    case LVAL_ERR:
      x->err = malloc(strlen(v->err)+1);
      strcpy(x->err, v->err); break;

    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym)+1);
      strcpy(x->sym, v->sym); break;

    //copy lists by copying each subexpression
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for(int i = 0; i < x->count; i++){
        x->cell[i] = lval_copy(v->cell[i]);
      }
      break;
  }

  return x;
}

//add element to sexpr
lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_pop(lval* v, int i){
	//find item at i
	lval* x = v->cell[i];

	//shift memory after the item at i over top
	memmove(&v->cell[i], &v->cell[i+1], 
		sizeof(lval*)*(v->count-i-1));

	//decrease the count of items in list
	v->count--;

	//realloc mem used
	v->cell = realloc(v->cell, sizeof(lval*)*v->count);
	return x;	
}

lval* lval_join(lval* x, lval* y){
  //for each cell in y add to x, remember 0 = false in C
  while(y->count){
    x = lval_add(x, lval_pop(y, 0));
  }

  //delete empty y
  lval_del(y);
  
  return x;
}

lval* lval_take(lval* v, int i){
	lval* x = lval_pop(v, i);
	lval_del(v);
	return x;
}

void lval_print(lval* v);

void lval_print_expr(lval* v, char open, char close){
	
	putchar(open);

	for(int i = 0; i < v->count; i++){
		
		//print value contained
		lval_print(v->cell[i]);

		//don't print trailing space if last element
		if(i != (v->count-1)){
			putchar(' ');
		}
	}
	putchar(close);
}

//print the lval
void lval_print(lval* v){
	
	switch(v->type){
		
		case LVAL_NUM: printf("%li", v->num); break;

    case LVAL_FUN: printf("<function>"); break;

		case LVAL_ERR: printf("Error: %s", v->err); break;

		case LVAL_SYM: printf("%s", v->sym); break;

		case LVAL_SEXPR: lval_print_expr(v, '(', ')'); break;
	 
    case LVAL_QEXPR: lval_print_expr(v, '{', '}'); break;
  }
}

//print lval followed by newline
void lval_println(lval* v){
	lval_print(v);
	putchar('\n');
}



// *** LISP ENVIRONMENT ***
//used to create immutable variables, like String in Java

struct lenv{
	int count;
	char** syms;
	lval** vals;
};

lenv* lenv_new(void){
	lenv* e = malloc(sizeof(lenv));
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

void lenv_del(lenv* e){
	for(int i = 0; i < e->count; i++){
		free(e->syms[i]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

//get values from env
lval* lenv_get(lenv* e, lval* k){
	//iterate over all items in environ
	for(int i = 0; i < e->count; i++){
		//check if stored string matches symbol
		//if so, return copy of value
		if(strcmp(e->syms[i], k->sym) == 0){
			return lval_copy(e->vals[i]);
		}
	}
	//if no symbol return error
	return lval_err("unbounded symbol");
}

//put values in env
void lenv_put(lenv* e, lval* k, lval* v){
	//iterate through all items to see if variable already exists
	for(int i = 0; i < e->count; i++){

		//if var is found, delete item and replace with new item
		if(strcmp(e->syms[i], k->sym) == 0){
			lval_del(e->vals[i]);
			e->vals[i] = lval_copy(v);
			return;
		}
	}

	//if no existing entry found alloc space
	e->count++;
	e->vals = realloc(e->vals, sizeof(lval*) * e->count);
	e->syms = realloc(e->syms, sizeof(char*) * e->count);

	//copy contents of lval and symbol to new location
	e->vals[e->count-1] = lval_copy(v);
	e->syms[e->count-1] = malloc(strlen(k->sym)+1);
	strcpy(e->syms[e->count-1], k->sym);
}


// *** BUILTINS ***

//preprocessor command to aid error checking
#define LASSERT(args, cond, err)\
  if(!(cond)){lval_del(args); return lval_err(err);}

lval* builtin(lenv* e, lval* a, char* func);

lval* lval_eval(lenv* e, lval* v);

lval* builtin_op(lenv* e, lval* a, char* op) {

  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }

  /* Pop the first element */
  lval* x = lval_pop(a, 0);

  /* If no arguments and sub then perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  /* While there are still elements remaining */
  while (a->count > 0) {

    /* Pop the next element */
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "%") == 0) { x->num %= y->num; }
    
    if (strcmp(op, "^") == 0) {
    	int temp = x->num;
    	if(y->num >= 0){
    		for(int i = 0; i < y->num-1; i++){
    			x->num = x->num * temp;} 
    		} else {
    			for(int i = 0; i > y->num; i--){
    				x->num = x->num /= temp;
    			}
    		}
    	}

    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x); lval_del(y);
        x = lval_err("Division By Zero!"); break;
      }
      x->num /= y->num;
    }

    lval_del(y);
  }

  lval_del(a); return x;
}

//Takes Q-Exp and returns Q-Exp with only the first element
lval* builtin_head(lenv* e, lval* a){
  //check error conditions
  LASSERT(a, a->count == 1,
    "Function 'head' passed too many args");

  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'head' passed incorrect type");
  
  LASSERT(a, a->cell[0]->count != 0,
    "Function 'head' passed {}");

  //if ok take first arg
  lval* v = lval_take(a, 0);

  //delete all elements not head and return
  while(v->count > 1){lval_del(lval_pop(v,1)); }
  return v;
}

//Takes Q-Exp and pops first item, leaving stack
lval* builtin_tail(lenv* e, lval* a){
  //Checking error conditions
  LASSERT(a, a->count == 1,
    "Function 'tail' passed too many args");

  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'tail' passed incorrect type");
  
  LASSERT(a, a->cell[0]->count != 0,
    "Function 'tail' passed {}");

  //if ok take first arg
  lval* v = lval_take(a, 0);

  //delete first element and return
  lval_del(lval_pop(v,0));
  return v;
}

//converts S-Exp to Q-Exp and returns 
lval* builtin_list(lenv* e, lval* a){
  a->type = LVAL_QEXPR;
  return a;
}

//converts Q-Exp to S-exp and then evaluates
lval* builtin_eval(lenv* e,lval* a){
  LASSERT(a, a->count == 1, 
    "Function 'eval' passed too many args");

  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, 
    "Function 'eval' passed wrong type");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

//takes one or more QEXP and joins them
lval* builtin_join(lenv* e, lval* a){
  
  for(int i = 0; i < a->count; i++){
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR, 
      "Function 'join' passed incorrect type");
  }

  lval* x = lval_pop(a, 0);

  while(a->count){
    x = lval_join(x, lval_pop(a,0)); //does not match codebase
  }
  lval_del(a);

  return x;
}

lval* builtin_add(lenv* e, lval* a){return builtin_op(e, a, "+");}
lval* builtin_sub(lenv* e, lval* a){return builtin_op(e, a, "-");}
lval* builtin_mul(lenv* e, lval* a){return builtin_op(e, a, "*");}
lval* builtin_div(lenv* e, lval* a){return builtin_op(e, a, "/");}
lval* builtin_mod(lenv* e, lval* a){return builtin_op(e, a, "%");}
lval* builtin_pow(lenv* e, lval* a){return builtin_op(e, a, "^");}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func){
	lval* k = lval_sym(name);
	lval* v = lval_builtin(func);
	lenv_put(e, k, v);
	lval_del(k); lval_del(v);
}

lval* builtin_def(lenv* e, lval* a){
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
		"Function 'def' passed incorrect type");

	//first argument is symbol list
	lval* syms = a->cell[0];

	//ensure all elements of first list are symbols
	for(int i = 0; i <syms->count; i++){
		LASSERT(a, syms->cell[i]->type == LVAL_SYM, 
			"Function 'def' cannot define non-symbol");
	}

	//check correct number of symbols and values
	LASSERT(a, syms->count == a->count-1, 
		"Function 'def' cannot define incorrect number of values to symbols");

	//Assign copies of values to symbols
	for(int i = 0; i < syms->count; i++){
		lenv_put(e, syms->cell[i], a->cell[i+1]);
	}
	lval_del(a);

	return lval_sexpr();
}

void lenv_add_builtins(lenv* e){
	//list functions
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);
	lenv_add_builtin(e, "def", builtin_def);

	//Math functions
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
	lenv_add_builtin(e, "%", builtin_mod);
	lenv_add_builtin(e, "^", builtin_pow);
}

lval* builtin(lenv* e, lval* a, char* func){
  if(strcmp("list", func) == 0){return builtin_list(e, a);}
  if(strcmp("head", func) == 0){return builtin_head(e, a);}
  if(strcmp("tail", func) == 0){return builtin_tail(e, a);}
  if(strcmp("join", func) == 0){return builtin_join(e, a);}
  if(strcmp("eval", func) == 0){return builtin_eval(e, a);}
  if(strstr("+-?*^%", func)){return builtin_op(e, a, func);}

  lval_del(a);
  return lval_err("Unknown function");
}



// *** EVALUATION ***

lval* lval_eval_sexpr(lenv* e, lval* v){
	//eval children
	for(int i = 0; i < v->count; i++){
		v->cell[i] = lval_eval(e, v->cell[i]);
	}

	//error checking
	for(int i = 0; i < v->count; i++){
		if(v->cell[i]->type == LVAL_ERR){
			return lval_take(v, i);
		}
	}

	//empty expression
	if(v->count == 0){return v;}

	//single expression
	if(v->count == 1){return lval_take(v,0);}

	//ensure first element is function after eval
	lval* f = lval_pop(v,0);
	if(f->type != LVAL_FUN){
		lval_del(f);
		lval_del(v);
		return lval_err("First element is not a function");
	}

	//call builtin with operator
	lval* result = f->builtin(e, v);
	lval_del(f);
	return result;	
}

lval* lval_eval(lenv* e, lval* v){
	if(v->type == LVAL_SYM){
		lval* x = lenv_get(e, v);
		lval_del(v);
		return x;
	}

	//eval s-expressions
	if(v->type == LVAL_SEXPR){return lval_eval_sexpr(e, v);}
	//all other lval types remain the same
	return v;
}

// *** READING ***

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
    lval_num(x) : lval_err("invalid number");
}

//eval lval tree
lval* lval_read(mpc_ast_t* t) {

  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  /* If root (>) or sexpr then create empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

// *** MAIN ***

int main (int argc, char** argv){
	
	//RPN parser
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr = mpc_new("sexpr");
	mpc_parser_t* Qexpr = mpc_new("qexpr");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Lispy = mpc_new("lispy");

	mpca_lang(MPCA_LANG_DEFAULT,
		" 													 												  \
		number   :  /-?[0-9]+/ 							    						; \
		symbol   :  /[a-zA-Z0-9_+\\-*\\/\\\\=<>%^!&]+/	    ; \
		sexpr    :  '(' <expr>* ')'  					    					; \
		qexpr    :  '{' <expr>* '}'  					    					; \
		expr     :  <number> | <symbol> | <sexpr> | <qexpr> ; \
		lispy    :  /^/ <expr>* /$/			  			    				; \
		",
		Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

	//print version and exit info
	printf("Lispy Version %s \n", versionNumber);
	printf("Press ctrl + c to EXIT\n\n");

	lenv* e = lenv_new();
	lenv_add_builtins(e);

	while(1){

		//output prompt and get input 
		char* input = readline("lispy >> ");
		
		//add input to history
		add_history(input);

		mpc_result_t r;
		
		if(mpc_parse("<stdin>", input, Lispy, &r)){

			lval* x = lval_eval(e, lval_read(r.output));
			lval_println(x);
			lval_del(x);

			// mpc_ast_print(r.output);
			mpc_ast_delete(r.output);

		} else {
			//failure - print error
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		//free retreieved input
		free(input);
	}
	lenv_del(e);

	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

	return 0;
}