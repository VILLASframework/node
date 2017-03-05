%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void log_expression_error(const char *s);
int log_expression_lex();
int log_expression_parse();
long lookup_token(char *token);
%}

%union {
	char *token;
	unsigned long constant;
}

%token <token> TOKEN
%token <constant> CONSTANT

%type <constant> exp comp


%left '&' '|' '^'
%precedence NEG

%define api.prefix {log_expression_}

%%

input:
	| exp '\n'		{ printf("\n\nresult = %#lx\n", $1); }
;

exp :
	comp
	| exp '&' exp	{ $$ = $1 & $3; }
	| exp '|' exp	{ $$ = $1 | $3; }
	| exp '^' exp	{ $$ = $1 ^ $3; }
	| '~' exp %prec NEG { $$ = ~$2; printf("neg\n"); }
	| '(' exp ')'	{ $$ = $2; printf("subex"); }	
;

comp : 
	  CONSTANT	{ $$ = $1;   printf("const %ld", $1); }
	| TOKEN		{ $$ = lookup_token($1); printf("token '%s'", $1); }
;

%%

long lookup_token(char *token)
{
	if (!strcmp(token, "a"))
		return 201;
	else if (!strcmp(token, "b"))
		return 202;
	if (!strcmp(token, "c"))
		return 203;
	if (!strcmp(token, "d"))
		return 204;
	else
		return 1111;

}

int main(int argc, char *argv[]) {
	do {
		log_expression_parse();
	} while (!feof(stdin));	
}

void log_expression_error(const char *s) {
	printf("EEK, parse error!  Message: %s", s);
	exit(-1);
}