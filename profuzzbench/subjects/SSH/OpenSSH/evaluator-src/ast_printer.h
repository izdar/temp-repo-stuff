// ast_printer.h
#ifndef AST_PRINTER_H
#define AST_PRINTER_H

# include "ast.h"
# include <iostream>
# include <string>
# include <map> 
# include <cassert> 
using namespace std;

class ASTPrinter {
public:
    static void printAST(const ASTNode* node, int indent = 0);
    static void printSpec(const Spec& spec, int indent = 0);
    static std::string printStuff(const ASTNode*node);
    static std::map<int, std::string> create_serial_to_formula_map(ASTNode* formulas);
private:
    static void printIndentation(int indent);
    static std::string getNodeTypeStr(const ASTNode* node);
    static std::string getTypeAnnotationStr(const TypeAnnotation& type);
    static void printTypeAnnotations(const std::vector<TypeAnnotation>& annotations, int indent);
    static void printFormulas(const std::vector<ASTNode*>& formulas, int indent);
    
};

#endif // AST_PRINTER_H