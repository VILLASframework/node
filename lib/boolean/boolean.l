%{
#include <stdio.h>
#include <string.h>

#include "boolean.tab.h"  // to get the token types that we return

%}

%option prefix="log_expression_"
%option noyywrap

%%

[ \t\n]		; // ignore whitespace
[&]	; //operators
[a-b]+		{ log_expression_lval.token = strdup(log_expression_text);   return TOKEN; }
[0-9]+		{ log_expression_lval.constant = atoi(log_expression_text);  return CONSTANT; }

%%