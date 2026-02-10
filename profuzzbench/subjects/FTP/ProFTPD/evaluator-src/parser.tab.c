/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



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




/* Copy the first part of user declarations.  */
#line 1 "parser.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
typedef struct ast_node ASTNode;
typedef struct TypeAnnotation TypeAnnotation;

/* Type definitions */
typedef enum {
    AST_ENUM,
    AST_INT_TYPE,
    AST_BOOL_TYPE
} TypeAnnotationKind;

typedef struct TypeAnnotation {
    TypeAnnotationKind kind;
    union {
        struct {
            char *id_name;
            char **comma_separated_id_list;  
        } enum_data;
        struct {
            char *id_name;
        } int_type_data;
        struct {
            char *id_name;
        } bool_type_data;
    } u;
} TypeAnnotation;

typedef struct ast_node {
    enum { 
        AST_ID, AST_INT, AST_BOOL, AST_H, AST_ARROW, AST_O, AST_Y, AST_S, AST_AND, AST_NOT, AST_OR, 
        AST_EQ, AST_NEQ, AST_GT, AST_GTE, AST_LT, AST_LTE, AST_SPEC
    } kind;
    union {
        int bool_value;
        int int_value;
        char* id_name;
        struct {struct ast_node* child; } unary;
        struct {struct ast_node* left; struct ast_node* right; } binary;
        struct {
            TypeAnnotation **type_annotations;
            struct ast_node **formulas;
        } spec_data;
    } u;
} ASTNode;

void yyerror(const char *s);
int yylex(void);
extern FILE *yyin;
ASTNode *root;



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 58 "parser.y"
{
    ASTNode *ast;
    char *str;
    int val;
    TypeAnnotation *type_ast;
    TypeAnnotation **type_list;
    char **str_list;
    ASTNode **ast_list;
}
/* Line 193 of yacc.c.  */
#line 221 "parser.tab.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 234 "parser.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  10
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   66

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  30
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  9
/* YYNRULES -- Number of rules.  */
#define YYNRULES  31
/* YYNRULES -- Number of states.  */
#define YYNSTATES  62

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   284

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     6,     8,    11,    18,    22,    26,    29,
      33,    37,    41,    44,    48,    52,    54,    56,    58,    61,
      64,    68,    71,    75,    79,    83,    87,    91,    95,    97,
     101,   103
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      31,     0,    -1,    32,    34,    -1,    33,    -1,    33,    32,
      -1,     7,     3,    12,    37,    13,    14,    -1,     8,     3,
      14,    -1,     9,     3,    14,    -1,    35,    14,    -1,    35,
      14,    34,    -1,    10,    35,    11,    -1,    35,    16,    35,
      -1,    17,    35,    -1,    35,    18,    35,    -1,    35,    19,
      35,    -1,     5,    -1,     6,    -1,    36,    -1,    20,    35,
      -1,    21,    35,    -1,    35,    22,    35,    -1,    23,    35,
      -1,     3,    24,    38,    -1,     3,    26,    38,    -1,     3,
      25,    38,    -1,     3,    27,    38,    -1,     3,    28,    38,
      -1,     3,    29,    38,    -1,     3,    -1,     3,    15,    37,
      -1,     3,    -1,     4,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   107,   107,   117,   122,   142,   149,   155,   164,   169,
     189,   193,   201,   208,   216,   224,   230,   236,   239,   245,
     251,   258,   267,   279,   291,   303,   315,   327,   342,   347,
     367,   373
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "ID", "INT", "TRUE", "FALSE", "ENUM",
  "INT_TYPE", "BOOL_TYPE", "LPAREN", "RPAREN", "LBRACE", "RBRACE",
  "SEMICOLON", "COMMA", "ARROW", "NOT", "AND", "OR", "O", "H", "S", "Y",
  "GT", "LT", "GTE", "LTE", "EQ", "NEQ", "$accept", "Spec",
  "TypeAnnotationList", "TypeAnnotation", "Formulas", "Formula",
  "Predicates", "comma_separated_id_list", "TERM", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    30,    31,    32,    32,    33,    33,    33,    34,    34,
      35,    35,    35,    35,    35,    35,    35,    35,    35,    35,
      35,    35,    36,    36,    36,    36,    36,    36,    37,    37,
      38,    38
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     1,     2,     6,     3,     3,     2,     3,
       3,     3,     2,     3,     3,     1,     1,     1,     2,     2,
       3,     2,     3,     3,     3,     3,     3,     3,     1,     3,
       1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     0,     0,     0,     3,     0,     0,     0,
       1,     0,    15,    16,     0,     0,     0,     0,     0,     2,
       0,    17,     4,     0,     6,     7,     0,     0,     0,     0,
       0,     0,     0,    12,    18,    19,    21,     8,     0,     0,
       0,     0,    28,     0,    30,    31,    22,    24,    23,    25,
      26,    27,    10,     9,    11,    13,    14,    20,     0,     0,
      29,     5
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     4,     5,     6,    19,    20,    21,    43,    46
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -15
static const yytype_int8 yypact[] =
{
      51,     3,     9,    12,    29,    11,    51,    21,    22,    25,
     -15,    23,   -15,   -15,    11,    11,    11,    11,    11,   -15,
      -9,   -15,   -15,    41,   -15,   -15,    15,    15,    15,    15,
      15,    15,    19,    42,    42,    42,    24,    11,    11,    11,
      11,    11,    30,    49,   -15,   -15,   -15,   -15,   -15,   -15,
     -15,   -15,   -15,   -15,    -8,    -8,    24,     4,    41,    50,
     -15,   -15
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -15,   -15,    57,   -15,    28,   -14,   -15,     8,    26
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      32,    33,    34,    35,    36,    37,     7,    38,    38,    39,
      40,    40,     8,    41,    11,     9,    12,    13,    44,    45,
      38,    14,    39,    40,    54,    55,    56,    57,    15,    10,
      52,    16,    17,    23,    18,    38,    24,    39,    40,    25,
      38,    41,    39,    40,    42,    58,    41,    26,    27,    28,
      29,    30,    31,    47,    48,    49,    50,    51,     1,     2,
       3,    40,    59,    22,    61,    53,    60
};

static const yytype_uint8 yycheck[] =
{
      14,    15,    16,    17,    18,    14,     3,    16,    16,    18,
      19,    19,     3,    22,     3,     3,     5,     6,     3,     4,
      16,    10,    18,    19,    38,    39,    40,    41,    17,     0,
      11,    20,    21,    12,    23,    16,    14,    18,    19,    14,
      16,    22,    18,    19,     3,    15,    22,    24,    25,    26,
      27,    28,    29,    27,    28,    29,    30,    31,     7,     8,
       9,    19,    13,     6,    14,    37,    58
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     7,     8,     9,    31,    32,    33,     3,     3,     3,
       0,     3,     5,     6,    10,    17,    20,    21,    23,    34,
      35,    36,    32,    12,    14,    14,    24,    25,    26,    27,
      28,    29,    35,    35,    35,    35,    35,    14,    16,    18,
      19,    22,     3,    37,     3,     4,    38,    38,    38,    38,
      38,    38,    11,    34,    35,    35,    35,    35,    15,    13,
      37,    14
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 107 "parser.y"
    {
        ASTNode *node = malloc(sizeof(ASTNode));
        node->kind = AST_SPEC;
        node->u.spec_data.type_annotations = (yyvsp[(1) - (2)].type_list);
        node->u.spec_data.formulas = (yyvsp[(2) - (2)].ast_list);
        root = node;
    ;}
    break;

  case 3:
#line 117 "parser.y"
    {
          (yyval.type_list) = malloc(2 * sizeof(TypeAnnotation *));  // Allocate space for one element
          (yyval.type_list)[0] = (yyvsp[(1) - (1)].type_ast);
          (yyval.type_list)[1] = NULL;  // Null-terminate the list
      ;}
    break;

  case 4:
#line 122 "parser.y"
    {
          int list_size = 0;
          while ((yyvsp[(2) - (2)].type_list)[list_size] != NULL) list_size++;  // Count existing annotations
          
          TypeAnnotation **new_list = malloc((list_size + 2) * sizeof(TypeAnnotation *));  // Allocate space for new list
          
          // Copy existing list
          for (int i = 0; i < list_size; i++) {
              new_list[i] = (yyvsp[(2) - (2)].type_list)[i];
          }
          
          // Add new annotation
          new_list[list_size] = (yyvsp[(1) - (2)].type_ast);
          new_list[list_size + 1] = NULL;  // Null-terminate the new list
          free((yyvsp[(2) - (2)].type_list));  // Free the original list after copying
          (yyval.type_list) = new_list;
      ;}
    break;

  case 5:
#line 142 "parser.y"
    {
          TypeAnnotation *node = malloc(sizeof(TypeAnnotation));
          node->kind = AST_ENUM;
          node->u.enum_data.id_name = strdup((yyvsp[(2) - (6)].str));  // $2 is the ID
          node->u.enum_data.comma_separated_id_list = (yyvsp[(4) - (6)].str_list);  // $4 is the list of IDs
          (yyval.type_ast) = node;
      ;}
    break;

  case 6:
#line 149 "parser.y"
    {
          TypeAnnotation *node = malloc(sizeof(TypeAnnotation));
          node->kind = AST_INT_TYPE;
          node->u.int_type_data.id_name = strdup((yyvsp[(2) - (3)].str));  // $2 is the ID
          (yyval.type_ast) = node;
      ;}
    break;

  case 7:
#line 155 "parser.y"
    {
          TypeAnnotation *node = malloc(sizeof(TypeAnnotation));
          node->kind = AST_BOOL_TYPE;
          node->u.bool_type_data.id_name = strdup((yyvsp[(2) - (3)].str));  // $2 is the ID
          (yyval.type_ast) = node;
      ;}
    break;

  case 8:
#line 164 "parser.y"
    {
          (yyval.ast_list) = malloc(2 * sizeof(ASTNode *));  // Allocate space for one formula
          (yyval.ast_list)[0] = (yyvsp[(1) - (2)].ast);
          (yyval.ast_list)[1] = NULL;  // Null-terminate the list
      ;}
    break;

  case 9:
#line 169 "parser.y"
    {
          int list_size = 0;
          while ((yyvsp[(3) - (3)].ast_list)[list_size] != NULL) list_size++;  // Count existing formulas
          
          ASTNode **new_list = malloc((list_size + 2) * sizeof(ASTNode *));  // Allocate space for new list
          
          // Copy existing formulas
          for (int i = 0; i < list_size; i++) {
              new_list[i] = (yyvsp[(3) - (3)].ast_list)[i];
          }
          
          // Add new formula
          new_list[list_size] = (yyvsp[(1) - (3)].ast);
          new_list[list_size + 1] = NULL;  // Null-terminate the new list
          free((yyvsp[(3) - (3)].ast_list));  // Free the original list after copying
          (yyval.ast_list) = new_list;
      ;}
    break;

  case 10:
#line 190 "parser.y"
    {
            (yyval.ast) = (yyvsp[(2) - (3)].ast);
        ;}
    break;

  case 11:
#line 194 "parser.y"
    {
            ASTNode *node = malloc(sizeof(ASTNode));
            node->kind = AST_ARROW;
            node->u.binary.left = (yyvsp[(1) - (3)].ast);
            node->u.binary.right = (yyvsp[(3) - (3)].ast);
            (yyval.ast) = node;
        ;}
    break;

  case 12:
#line 202 "parser.y"
    {
            ASTNode *node = malloc(sizeof(ASTNode));
            node->kind = AST_NOT;
            node->u.unary.child = (yyvsp[(2) - (2)].ast);
            (yyval.ast) = node;
        ;}
    break;

  case 13:
#line 209 "parser.y"
    {
            ASTNode *node = malloc(sizeof(ASTNode));
            node->kind = AST_AND;
            node->u.binary.left = (yyvsp[(1) - (3)].ast);
            node->u.binary.right = (yyvsp[(3) - (3)].ast);
            (yyval.ast) = node; 
        ;}
    break;

  case 14:
#line 217 "parser.y"
    {
            ASTNode *node = malloc(sizeof(ASTNode));
            node->kind = AST_OR;
            node->u.binary.left = (yyvsp[(1) - (3)].ast);
            node->u.binary.right = (yyvsp[(3) - (3)].ast);
            (yyval.ast) = node; 
        ;}
    break;

  case 15:
#line 224 "parser.y"
    {
        ASTNode *node = malloc(sizeof(ASTNode));
        node->kind = AST_BOOL;
        node->u.bool_value = 1;
        (yyval.ast) = node;
    ;}
    break;

  case 16:
#line 230 "parser.y"
    {
        ASTNode *node = malloc(sizeof(ASTNode));
        node->kind = AST_BOOL;
        node->u.bool_value = 0;
        (yyval.ast) = node;
    ;}
    break;

  case 17:
#line 236 "parser.y"
    {
        (yyval.ast) = (yyvsp[(1) - (1)].ast);
    ;}
    break;

  case 18:
#line 239 "parser.y"
    {
        ASTNode *node = malloc(sizeof(ASTNode));
        node->kind = AST_O;
        node->u.unary.child = (yyvsp[(2) - (2)].ast);
        (yyval.ast) = node;
    ;}
    break;

  case 19:
#line 245 "parser.y"
    {
        ASTNode *node = malloc(sizeof(ASTNode));
        node->kind = AST_H;
        node->u.unary.child = (yyvsp[(2) - (2)].ast);
        (yyval.ast) = node;
    ;}
    break;

  case 20:
#line 251 "parser.y"
    {
        ASTNode *node = malloc(sizeof(ASTNode));
        node->kind = AST_S;
        node->u.binary.left = (yyvsp[(1) - (3)].ast);
        node->u.binary.right = (yyvsp[(3) - (3)].ast);
        (yyval.ast) = node; 
    ;}
    break;

  case 21:
#line 258 "parser.y"
    {
        ASTNode *node = malloc(sizeof(ASTNode));
        node->kind = AST_Y;
        node->u.unary.child = (yyvsp[(2) - (2)].ast);
        (yyval.ast) = node;
    ;}
    break;

  case 22:
#line 268 "parser.y"
    {
            ASTNode *left_node = malloc(sizeof(ASTNode));
            left_node->kind = AST_ID;
            left_node->u.id_name = strdup((yyvsp[(1) - (3)].str));

            ASTNode *node = malloc(sizeof(ASTNode));
            node->kind = AST_GT;
            node->u.binary.left = left_node;
            node->u.binary.right = (yyvsp[(3) - (3)].ast);
            (yyval.ast) = node;
        ;}
    break;

  case 23:
#line 280 "parser.y"
    {
            ASTNode *left_node = malloc(sizeof(ASTNode));
            left_node->kind = AST_ID;
            left_node->u.id_name = strdup((yyvsp[(1) - (3)].str));

            ASTNode *node = malloc(sizeof(ASTNode));
            node->kind = AST_GTE;
            node->u.binary.left = left_node;
            node->u.binary.right = (yyvsp[(3) - (3)].ast);
            (yyval.ast) = node;
        ;}
    break;

  case 24:
#line 292 "parser.y"
    {
            ASTNode *left_node = malloc(sizeof(ASTNode));
            left_node->kind = AST_ID;
            left_node->u.id_name = strdup((yyvsp[(1) - (3)].str));

            ASTNode *node = malloc(sizeof(ASTNode));
            node->kind = AST_LT;
            node->u.binary.left = left_node;
            node->u.binary.right = (yyvsp[(3) - (3)].ast);
            (yyval.ast) = node;
        ;}
    break;

  case 25:
#line 304 "parser.y"
    {
            ASTNode *left_node = malloc(sizeof(ASTNode));
            left_node->kind = AST_ID;
            left_node->u.id_name = strdup((yyvsp[(1) - (3)].str));

            ASTNode *node = malloc(sizeof(ASTNode));
            node->kind = AST_LTE;
            node->u.binary.left = left_node;
            node->u.binary.right = (yyvsp[(3) - (3)].ast);
            (yyval.ast) = node;
        ;}
    break;

  case 26:
#line 316 "parser.y"
    {
            ASTNode *left_node = malloc(sizeof(ASTNode));
            left_node->kind = AST_ID;
            left_node->u.id_name = strdup((yyvsp[(1) - (3)].str));

            ASTNode *node = malloc(sizeof(ASTNode));
            node->kind = AST_EQ;
            node->u.binary.left = left_node;
            node->u.binary.right = (yyvsp[(3) - (3)].ast);
            (yyval.ast) = node;
        ;}
    break;

  case 27:
#line 328 "parser.y"
    {
            ASTNode *left_node = malloc(sizeof(ASTNode));
            left_node->kind = AST_ID;
            left_node->u.id_name = strdup((yyvsp[(1) - (3)].str));

            ASTNode *node = malloc(sizeof(ASTNode));
            node->kind = AST_NEQ;
            node->u.binary.left = left_node;
            node->u.binary.right = (yyvsp[(3) - (3)].ast);
            (yyval.ast) = node;
        ;}
    break;

  case 28:
#line 342 "parser.y"
    { 
          (yyval.str_list) = malloc(2 * sizeof(char *));
          (yyval.str_list)[0] = strdup((yyvsp[(1) - (1)].str));
          (yyval.str_list)[1] = NULL;  // Null-terminate the list
      ;}
    break;

  case 29:
#line 347 "parser.y"
    {
          int list_size = 0;
          while ((yyvsp[(3) - (3)].str_list)[list_size] != NULL) list_size++;  // Count existing elements
          
          char **new_list = malloc((list_size + 2) * sizeof(char *));  // Allocate space for new list
          
          // Copy existing elements
          for (int i = 0; i < list_size; i++) {
              new_list[i] = (yyvsp[(3) - (3)].str_list)[i];
          }
          
          // Add new ID
          new_list[list_size] = strdup((yyvsp[(1) - (3)].str));
          new_list[list_size + 1] = NULL;  // Null-terminate the new list
          free((yyvsp[(3) - (3)].str_list));  // Free the original list after copying
          (yyval.str_list) = new_list;
      ;}
    break;

  case 30:
#line 367 "parser.y"
    { 
        ASTNode *node = malloc(sizeof(ASTNode));
        node->kind = AST_ID;
        node->u.id_name = strdup((yyvsp[(1) - (1)].str));
        (yyval.ast) = node; 
    ;}
    break;

  case 31:
#line 373 "parser.y"
    { 
        ASTNode *node = malloc(sizeof(ASTNode));
        node->kind = AST_INT;
        node->u.int_value = (yyvsp[(1) - (1)].val);
        (yyval.ast) = node;
    ;}
    break;


/* Line 1267 of yacc.c.  */
#line 1834 "parser.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 381 "parser.y"


void yyerror(const char *s) {
    fprintf(stderr, "Error: %s\n", s);
}

/* AST printing functions */
void print_indentation(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

const char* get_node_type_str(ASTNode *node) {
    switch (node->kind) {
        case AST_ID: return "IDENTIFIER";
        case AST_INT: return "INTEGER";
        case AST_BOOL: return "BOOLEAN";
        case AST_H: return "H_OPERATOR";
        case AST_ARROW: return "IMPLICATION";
        case AST_O: return "O_OPERATOR";
        case AST_Y: return "Y_OPERATOR";
        case AST_S: return "S_OPERATOR";
        case AST_AND: return "AND";
        case AST_OR: return "OR";
        case AST_NOT: return "NOT";
        case AST_EQ: return "EQUAL";
        case AST_NEQ: return "NOT_EQUAL";
        case AST_GT: return "GREATER_THAN";
        case AST_GTE: return "GREATER_THAN_EQUAL";
        case AST_LT: return "LESS_THAN";
        case AST_LTE: return "LESS_THAN_EQUAL";
        case AST_SPEC: return "SPECIFICATION";
        default: return "UNKNOWN";
    }
}

const char* get_type_annotation_str(TypeAnnotation *type) {
    switch (type->kind) {
        case AST_ENUM: return "ENUM_TYPE";
        case AST_INT_TYPE: return "INTEGER_TYPE";
        case AST_BOOL_TYPE: return "BOOLEAN_TYPE";
        default: return "UNKNOWN_TYPE";
    }
}

void print_type_annotations(TypeAnnotation **annotations, int indent) {
    if (!annotations) return;
    
    for (int i = 0; annotations[i] != NULL; i++) {
        print_indentation(indent);
        printf("Type Annotation: %s\n", get_type_annotation_str(annotations[i]));
        
        print_indentation(indent + 1);
        printf("Name: %s\n", annotations[i]->kind == AST_ENUM ? 
                            annotations[i]->u.enum_data.id_name : 
                            (annotations[i]->kind == AST_INT_TYPE ? 
                             annotations[i]->u.int_type_data.id_name : 
                             annotations[i]->u.bool_type_data.id_name));
        
        if (annotations[i]->kind == AST_ENUM) {
            print_indentation(indent + 1);
            printf("Values: ");
            
            char **values = annotations[i]->u.enum_data.comma_separated_id_list;
            if (values) {
                for (int j = 0; values[j] != NULL; j++) {
                    printf("%s", values[j]);
                    if (values[j+1] != NULL) printf(", ");
                }
            }
            printf("\n");
        }
    }
}

void print_formulas(ASTNode **formulas, int indent) {
    if (!formulas) return;
    
    for (int i = 0; formulas[i] != NULL; i++) {
        print_indentation(indent);
        printf("Formula %d:\n", i+1);
        print_ast(formulas[i], indent + 1);
    }
}

void print_ast(ASTNode *node, int indent) {
    if (!node) return;
    
    print_indentation(indent);
    printf("Node Type: %s\n", get_node_type_str(node));
    
    switch (node->kind) {
        case AST_ID:
            print_indentation(indent + 1);
            printf("Identifier: %s\n", node->u.id_name);
            break;
        case AST_INT:
            print_indentation(indent + 1);
            printf("Value: %d\n", node->u.int_value);
            break;
        case AST_BOOL:
            print_indentation(indent + 1);
            printf("Value: %s\n", node->u.bool_value ? "true" : "false");
            break;
        case AST_SPEC:
            print_indentation(indent + 1);
            printf("Type Annotations:\n");
            print_type_annotations(node->u.spec_data.type_annotations, indent + 2);
            
            print_indentation(indent + 1);
            printf("Formulas:\n");
            print_formulas(node->u.spec_data.formulas, indent + 2);
            break;
        case AST_NOT:
        case AST_H:
        case AST_O:
        case AST_Y:
            print_indentation(indent + 1);
            printf("Child:\n");
            print_ast(node->u.unary.child, indent + 2);
            break;
        case AST_ARROW:
        case AST_AND:
        case AST_OR:
        case AST_S:
        case AST_EQ:
        case AST_NEQ:
        case AST_GT:
        case AST_GTE:
        case AST_LT:
        case AST_LTE:
            print_indentation(indent + 1);
            printf("Left:\n");
            print_ast(node->u.binary.left, indent + 2);
            
            print_indentation(indent + 1);
            printf("Right:\n");
            print_ast(node->u.binary.right, indent + 2);
            break;
        default:
            print_indentation(indent + 1);
            printf("Unknown node type\n");
    }
}

/* Memory cleanup functions */
void free_type_annotations(TypeAnnotation **annotations) {
    if (!annotations) return;
    
    for (int i = 0; annotations[i] != NULL; i++) {
        if (annotations[i]->kind == AST_ENUM) {
            free(annotations[i]->u.enum_data.id_name);
            
            char **values = annotations[i]->u.enum_data.comma_separated_id_list;
            if (values) {
                for (int j = 0; values[j] != NULL; j++) {
                    free(values[j]);
                }
                free(values);
            }
        } else if (annotations[i]->kind == AST_INT_TYPE) {
            free(annotations[i]->u.int_type_data.id_name);
        } else if (annotations[i]->kind == AST_BOOL_TYPE) {
            free(annotations[i]->u.bool_type_data.id_name);
        }
        
        free(annotations[i]);
    }
    
    free(annotations);
}

void free_ast(ASTNode *node) {
    if (!node) return;
    
    switch (node->kind) {
        case AST_ID:
            free(node->u.id_name);
            break;
        case AST_SPEC:
            free_type_annotations(node->u.spec_data.type_annotations);
            
            if (node->u.spec_data.formulas) {
                for (int i = 0; node->u.spec_data.formulas[i] != NULL; i++) {
                    free_ast(node->u.spec_data.formulas[i]);
                }
                free(node->u.spec_data.formulas);
            }
            break;
        case AST_NOT:
        case AST_H:
        case AST_O:
        case AST_Y:
            free_ast(node->u.unary.child);
            break;
        case AST_ARROW:
        case AST_AND:
        case AST_OR:
        case AST_S:
        case AST_EQ:
        case AST_NEQ:
        case AST_GT:
        case AST_GTE:
        case AST_LT:
        case AST_LTE:
            free_ast(node->u.binary.left);
            free_ast(node->u.binary.right);
            break;
        default:
            break;
    }
    
    free(node);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    
    FILE *input_file = fopen(argv[1], "r");
    if (!input_file) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        return 1;
    }
    
    yyin = input_file;
    
    // Parse the input
    int parse_result = yyparse();
    fclose(input_file);
    
    if (parse_result != 0) {
        fprintf(stderr, "Parsing failed\n");
        return 1;
    }
    
    // Print the AST if parsing was successful
    if (root) {
        printf("Parse successful! AST:\n");
        print_ast(root, 0);
        
        // Clean up memory
        free_ast(root);
    } else {
        fprintf(stderr, "Error: No AST was generated\n");
        return 1;
    }
    
    return 0;
}
