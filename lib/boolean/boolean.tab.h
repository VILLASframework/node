/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_LOG_EXPRESSION_BOOLEAN_TAB_H_INCLUDED
# define YY_LOG_EXPRESSION_BOOLEAN_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef LOG_EXPRESSION_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define LOG_EXPRESSION_DEBUG 1
#  else
#   define LOG_EXPRESSION_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define LOG_EXPRESSION_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined LOG_EXPRESSION_DEBUG */
#if LOG_EXPRESSION_DEBUG
extern int log_expression_debug;
#endif

/* Token type.  */
#ifndef LOG_EXPRESSION_TOKENTYPE
# define LOG_EXPRESSION_TOKENTYPE
  enum log_expression_tokentype
  {
    TOKEN = 258,
    CONSTANT = 259,
    NEG = 260
  };
#endif

/* Value type.  */
#if ! defined LOG_EXPRESSION_STYPE && ! defined LOG_EXPRESSION_STYPE_IS_DECLARED

union LOG_EXPRESSION_STYPE
{
#line 12 "boolean.y" /* yacc.c:1909  */

	char *token;
	unsigned long constant;

#line 73 "boolean.tab.h" /* yacc.c:1909  */
};

typedef union LOG_EXPRESSION_STYPE LOG_EXPRESSION_STYPE;
# define LOG_EXPRESSION_STYPE_IS_TRIVIAL 1
# define LOG_EXPRESSION_STYPE_IS_DECLARED 1
#endif


extern LOG_EXPRESSION_STYPE log_expression_lval;

int log_expression_parse (void);

#endif /* !YY_LOG_EXPRESSION_BOOLEAN_TAB_H_INCLUDED  */
