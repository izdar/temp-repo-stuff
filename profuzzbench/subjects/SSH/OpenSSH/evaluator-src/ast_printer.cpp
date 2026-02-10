#include "ast_printer.h"
#include <iostream>
# include <cassert>

void ASTPrinter::printIndentation(int indent) {
    for (int i = 0; i < indent; i++) {
        std::cout << "  ";
    }
}


std::string ASTPrinter::printStuff(const ASTNode*node)
{
    assert(node); 
    switch (node->kind)
    {
        case AST_ID:
            return  node->id_name ;
            // break;
        case AST_INT:
            return std::to_string(node->int_value);
            // break;
        case AST_BOOL:
            return (node->bool_value ? "true" : "false");
            // std::cout << "Boolean: " << (node->bool_value ? "true" : "false") << std::endl;
            // break;
        case AST_H:
            return "H(" + printStuff(node->unary_child) + ")";
            // break;
        case AST_ARROW:
            return  printStuff(node->binary_left) + " -> " + printStuff(node->binary_right)  ;
            // break;
        case AST_O:
            return "O(" + printStuff(node->unary_child) + ")";
            // break;
        case AST_Y:
            return "Y(" + printStuff(node->unary_child) + ")";
            // break;
        case AST_S:
            return "(" + printStuff(node->binary_left) + " S " + printStuff(node->binary_right) + ")";
            // break;
        case AST_AND:
            return  printStuff(node->binary_left) + " & " + printStuff(node->binary_right) ;
            // break;
        case AST_OR:
            return printStuff(node->binary_left) + " | " + printStuff(node->binary_right) ;
            // break;
        case AST_NOT:
            return "!(" + printStuff(node->unary_child) + ")";
            // break;
        case AST_EQ:
            return  printStuff(node->binary_left) + "==" + printStuff(node->binary_right) ;
            // break;
        case AST_NEQ:
            return  printStuff(node->binary_left) + " != " + printStuff(node->binary_right)  ;
            // break;
        case AST_GT:
            return "(" + printStuff(node->binary_left) + " > " + printStuff(node->binary_right) + ")";
            // break;
        case AST_GTE:
            return "(" + printStuff(node->binary_left) + " >= " + printStuff(node->binary_right) + ")";
            // break;
        case AST_LT:
            return "(" + printStuff(node->binary_left) + " < " + printStuff(node->binary_right) + ")";
            // break;
        case AST_LTE:
            return "(" + printStuff(node->binary_left) + " <= " + printStuff(node->binary_right) + ")";
            // break;
        default:
            std::cerr << "Error: Unknown node type encountered during printing." << std::endl;
            assert(0);
            // break;
        return "ERROR";
    }

}

void traverser(ASTNode* node, std::map<int, std::string>& result) {
    // if (node == nullptr) return;

    // Store the serial number and formula string in the map
    assert(node != nullptr);
    assert(node->serial_number != -1);
    assert(result.find(node->serial_number) == result.end());
    result[node->serial_number] = ASTPrinter::printStuff(node);

    switch(node->kind){
        case AST_ID: 
        case AST_INT:
        case AST_BOOL:
        case AST_EQ : 
        case AST_NEQ:
        case AST_GT:
        case AST_GTE:
        case AST_LT:
        case AST_LTE:
            break; 
        case AST_H:
        case AST_NOT:
        case AST_O:
        case AST_Y:
            traverser(node->unary_child, result);
            break;
        case AST_ARROW:
        case AST_AND:
        case AST_OR:
        case AST_S:
            traverser(node->binary_left, result);
            traverser(node->binary_right, result);
            break;
        default: 
            std::cerr << "Error: Unknown node type encountered during traversal." << std::endl;
            assert(0);
            break;
        
    }
}


std::map<int, std::string> ASTPrinter::create_serial_to_formula_map(ASTNode* formulas)
{
    std::map<int, std::string> result ; 
    result.clear() ; 
    traverser(formulas, result); 
    return result ;

}


std::string ASTPrinter::getNodeTypeStr(const ASTNode* node) {
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

std::string ASTPrinter::getTypeAnnotationStr(const TypeAnnotation& type) {
    switch (type.kind) {
        case AST_ENUM: return "ENUM_TYPE";
        case AST_INT_TYPE: return "INTEGER_TYPE";
        case AST_BOOL_TYPE: return "BOOLEAN_TYPE";
        default: return "UNKNOWN_TYPE";
    }
}

void ASTPrinter::printTypeAnnotations(const std::vector<TypeAnnotation>& annotations, int indent) {
    for (const auto& annotation : annotations) {
        printIndentation(indent);
        std::cout << "Type Annotation: " << getTypeAnnotationStr(annotation) << std::endl;
        
        printIndentation(indent + 1);
        if (annotation.kind == AST_ENUM) {
            std::cout << "Name: " << annotation.enum_name << std::endl;
        } else if (annotation.kind == AST_INT_TYPE) {
            std::cout << "Name: " << annotation.int_type_name << std::endl;
        } else if (annotation.kind == AST_BOOL_TYPE) {
            std::cout << "Name: " << annotation.bool_type_name << std::endl;
        }
        
        if (annotation.kind == AST_ENUM) {
            printIndentation(indent + 1);
            std::cout << "Values: ";
            
            const auto& values = annotation.enum_values;
            for (size_t j = 0; j < values.size(); j++) {
                std::cout << values[j];
                if (j < values.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;
        }
    }
}

void ASTPrinter::printFormulas(const std::vector<ASTNode*>& formulas, int indent) {
    for (size_t i = 0; i < formulas.size(); i++) {
        printIndentation(indent);
        std::cout << "Formula " << (i+1) << ":" << std::endl;
        printAST(formulas[i], indent + 1);
    }
}

void ASTPrinter::printAST(const ASTNode* node, int indent) {
    if (!node) {
        printIndentation(indent);
        std::cout << "Null Node" << std::endl;
        return;
    }
    
    printIndentation(indent);
    std::cout << "Node Type: " << getNodeTypeStr(node) << std::endl;
    
    switch (node->kind) {
        case AST_ID:
            printIndentation(indent + 1);
            std::cout << "Identifier: " << node->id_name << std::endl;
            printIndentation(indent + 1);
            std::cout << "Serial Number: " << node->serial_number << std::endl;
            break;
        case AST_INT:
            printIndentation(indent + 1);
            std::cout << "Value: " << node->int_value << std::endl;
            printIndentation(indent + 1);
            std::cout << "Serial Number: " << node->serial_number << std::endl;
            break;
        case AST_BOOL:
            printIndentation(indent + 1);
            std::cout << "Value: " << (node->bool_value ? "true" : "false") << std::endl;
            printIndentation(indent + 1);
            std::cout << "Serial Number: " << node->serial_number << std::endl;
            break;
        case AST_SPEC:
            printIndentation(indent + 1);
            std::cout << "Type Annotations:" << std::endl;
            printTypeAnnotations(node->spec_type_annotations, indent + 2);
            
            printIndentation(indent + 1);
            std::cout << "Formulas:" << std::endl;
            printFormulas(node->spec_formulas, indent + 2);
            break;
        case AST_NOT:
        case AST_H:
        case AST_O:
        case AST_Y:
            printIndentation(indent + 1);
            std::cout << "Serial Number: " << node->serial_number << std::endl;
            printIndentation(indent + 1);
            std::cout << "Child:" << std::endl;
            printAST(node->unary_child, indent + 2);

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
            printIndentation(indent + 1);
            std::cout << "Serial Number: " << node->serial_number << std::endl;
            printIndentation(indent + 1);
            std::cout << "Left:" << std::endl;
            printAST(node->binary_left, indent + 2);
            
            printIndentation(indent + 1);
            std::cout << "Right:" << std::endl;
            printAST(node->binary_right, indent + 2);
            break;
        default:
            printIndentation(indent + 1);
            std::cout << "Unknown node type" << std::endl;
    }
}

void ASTPrinter::printSpec(const Spec& spec, int indent) {
    printIndentation(indent);
    std::cout << "SPECIFICATION:" << std::endl;
    
    printIndentation(indent + 1);
    std::cout << "Type Annotations:" << std::endl;
    printTypeAnnotations(spec.first, indent + 2);
    
    printIndentation(indent + 1);
    std::cout << "Formulas:" << std::endl;
    printFormulas(spec.second, indent + 2);
}