/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     ID = 258,
     INT = 259,
     TRUE = 260,
     FALSE = 261,
     ENUM = 262,
     INT_TYPE = 263,
     BOOL_TYPE = 264,
     LPAREN = 265,
     RPAREN = 266,
     LBRACE = 267,
     RBRACE = 268,
     SEMICOLON = 269,
     COMMA = 270,
     ARROW = 271,
     NOT = 272,
     AND = 273,
     OR = 274,
     O = 275,
     H = 276,
     S = 277,
     Y = 278,
     GT = 279,
     LT = 280,
     GTE = 281,
     LTE = 282,
     EQ = 283,
     NEQ = 284
   };
#endif
/* Tokens.  */
#define ID 258
#define INT 259
#define TRUE 260
#define FALSE 261
#define ENUM 262
#define INT_TYPE 263
#define BOOL_TYPE 264
#define LPAREN 265
#define RPAREN 266
#define LBRACE 267
#define RBRACE 268
#define SEMICOLON 269
#define COMMA 270
#define ARROW 271
#define NOT 272
#define AND 273
#define OR 274
#define O 275
#define H 276
#define S 277
#define Y 278
#define GT 279
#define LT 280
#define GTE 281
#define LTE 282
#define EQ 283
#define NEQ 284




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 13 "parser.y"
{
    ASTNode *ast;
    char *str;
    int val;
    TypeAnnotation *type_ast;
    TypeAnnotation **type_list;
    char **str_list;
    ASTNode **ast_list;
}
/* Line 1529 of yacc.c.  */
#line 117 "parser.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

