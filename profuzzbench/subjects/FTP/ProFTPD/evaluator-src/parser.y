%{
#include <iostream>
#include <string>
#include <vector>
#include "ast.h"

void yyerror(const char *s);
int yylex(void);
extern FILE *yyin;
Spec root;
%}

%union {
    ASTNode* ast;
    char* str;
    int val;
    TypeAnnotation* type_ast;
    std::vector<TypeAnnotation>* type_list;
    std::vector<std::string>* str_list;
    std::vector<ASTNode*>* ast_list;
    Spec* spec_val; 
}

%token <str> ID 
%token <val> INT
%token TRUE FALSE
%token ENUM INT_TYPE BOOL_TYPE
%token LPAREN RPAREN LBRACE RBRACE SEMICOLON COMMA
%token ARROW NOT AND OR O H S Y
%token GT LT GTE LTE EQ NEQ

%type <spec_val> Spec
%type <ast_list> Formulas
%type <ast> Formula
%type <ast> TERM
%type <ast> Predicates
%type <type_ast> TypeAnnotation
%type <type_list> TypeAnnotationList
%type <str_list> comma_separated_id_list

%right ARROW
%left OR
%left AND
%right H
%nonassoc S
%left Y
%right O
%right NOT

%nonassoc GT
%nonassoc GTE
%nonassoc LT
%nonassoc LTE
%nonassoc NEQ
%nonassoc EQ

%start Spec

%%

Spec :
    TypeAnnotationList Formulas {
        std::vector<TypeAnnotation> typeAnnotations;
        for (const auto& ta : *$1) {
            typeAnnotations.push_back(ta);
        }
        
        $$ = new Spec(typeAnnotations, *$2);
        root = *$$;
        
        // Cleanup
        delete $1;
        delete $2;
        delete $$;
    }
;

TypeAnnotationList :
    TypeAnnotation {
        $$ = new std::vector<TypeAnnotation>();
        $$->push_back(*$1);
        delete $1;
    }
    | TypeAnnotation TypeAnnotationList {
        $2->push_back(*$1);
        $$ = $2;
        delete $1;
    }
;

TypeAnnotation :
    ENUM ID LBRACE comma_separated_id_list RBRACE SEMICOLON {
        $$ = new TypeAnnotation(AST_ENUM);
        $$->enum_name = $2;
        $$->enum_values = *$4;
        free($2);
        delete $4;
    }
    | INT_TYPE ID SEMICOLON {
        $$ = new TypeAnnotation(AST_INT_TYPE);
        $$->int_type_name = $2;
        free($2);
    }
    | BOOL_TYPE ID SEMICOLON {
        $$ = new TypeAnnotation(AST_BOOL_TYPE);
        $$->bool_type_name = $2;
        free($2);
    }
;

Formulas :
    Formula SEMICOLON {
        $$ = new std::vector<ASTNode*>();
        $$->push_back($1);
    }
    | Formula SEMICOLON Formulas {
        $3->push_back($1);
        $$ = $3;
    }
;

Formula :
    LPAREN Formula RPAREN {
        $$ = $2;
    }
    | Formula ARROW Formula {
        ASTNode* node = new ASTNode(AST_ARROW);
        node->binary_left = $1;
        node->binary_right = $3;
        $$ = node;
    }
    | NOT Formula {
        ASTNode* node = new ASTNode(AST_NOT);
        node->unary_child = $2;
        $$ = node;
    }
    | Formula AND Formula {
        ASTNode* node = new ASTNode(AST_AND);
        node->binary_left = $1;
        node->binary_right = $3;
        $$ = node;
    }
    | Formula OR Formula {
        ASTNode* node = new ASTNode(AST_OR);
        node->binary_left = $1;
        node->binary_right = $3;
        $$ = node;
    }
    | TRUE {
        ASTNode* node = new ASTNode(AST_BOOL);
        node->bool_value = true;
        $$ = node;
    }
    | FALSE {
        ASTNode* node = new ASTNode(AST_BOOL);
        node->bool_value = false;
        $$ = node;
    }
    | Predicates {
        $$ = $1;
    }
    | O Formula {
        ASTNode* node = new ASTNode(AST_O);
        node->unary_child = $2;
        $$ = node;
    }
    | H Formula {
        ASTNode* node = new ASTNode(AST_H);
        node->unary_child = $2;
        $$ = node;
    }
    | Formula S Formula {
        ASTNode* node = new ASTNode(AST_S);
        node->binary_left = $1;
        node->binary_right = $3;
        $$ = node;
    }
    | Y Formula {
        ASTNode* node = new ASTNode(AST_Y);
        node->unary_child = $2;
        $$ = node;
    }
;

Predicates :
    ID GT TERM {
        ASTNode* left_node = new ASTNode(AST_ID);
        left_node->id_name = $1;

        ASTNode* node = new ASTNode(AST_GT);
        node->binary_left = left_node;
        node->binary_right = $3;
        $$ = node;
        free($1);
    }
    | ID GTE TERM {
        ASTNode* left_node = new ASTNode(AST_ID);
        left_node->id_name = $1;

        ASTNode* node = new ASTNode(AST_GTE);
        node->binary_left = left_node;
        node->binary_right = $3;
        $$ = node;
        free($1);
    }
    | ID LT TERM {
        ASTNode* left_node = new ASTNode(AST_ID);
        left_node->id_name = $1;

        ASTNode* node = new ASTNode(AST_LT);
        node->binary_left = left_node;
        node->binary_right = $3;
        $$ = node;
        free($1);
    }
    | ID LTE TERM {
        ASTNode* left_node = new ASTNode(AST_ID);
        left_node->id_name = $1;

        ASTNode* node = new ASTNode(AST_LTE);
        node->binary_left = left_node;
        node->binary_right = $3;
        $$ = node;
        free($1);
    }
    | ID EQ TERM {
        ASTNode* left_node = new ASTNode(AST_ID);
        left_node->id_name = $1;

        ASTNode* node = new ASTNode(AST_EQ);
        node->binary_left = left_node;
        node->binary_right = $3;
        $$ = node;
        free($1);
    }
    | ID NEQ TERM {
        ASTNode* left_node = new ASTNode(AST_ID);
        left_node->id_name = $1;

        ASTNode* node = new ASTNode(AST_NEQ);
        node->binary_left = left_node;
        node->binary_right = $3;
        $$ = node;
        free($1);
    }
;

comma_separated_id_list :
    ID {
        $$ = new std::vector<std::string>();
        $$->push_back($1);
        free($1);
    }
    | ID COMMA comma_separated_id_list {
        $3->push_back($1);
        $$ = $3;
        free($1);
    }
;

TERM : 
    ID {
        ASTNode* node = new ASTNode(AST_ID);
        node->id_name = $1;
        $$ = node;
        free($1);
    }
    | INT {
        ASTNode* node = new ASTNode(AST_INT);
        node->int_value = $1;
        $$ = node;
    }
    | TRUE {
        ASTNode* node = new ASTNode(AST_BOOL);
        node->bool_value = true;
        $$ = node;
    }
    | FALSE {
        ASTNode* node = new ASTNode(AST_BOOL);
        node->bool_value = false;
        $$ = node;
    }
;

%%

void yyerror(const char *s) {
    extern int yylineno;
    std::cerr << "Error: " << s << std::endl;
}