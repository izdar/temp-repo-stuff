# include "preprocess.h"


vector<int> Preprocessor::DoPreProcess(vector<ASTNode*>& asts)
{
    vector<int> serial_numbers;
    for(auto ast : asts)
    {
        snum = 0 ; 
        PreProcess(ast);
        // std::cout << "Serial Number: " << snum << std::endl;
        serial_numbers.push_back(snum);
    }
    return serial_numbers;
}
void Preprocessor::PreProcess(ASTNode* node)
{
    if(!node){
        std::cerr << "Error: Null node encountered during preprocessing." << std::endl;
        return; 
    }
    node->serial_number = snum ; 
    ++snum ;
    node->processed = true ;
    switch (node->kind) {
        case AST_ID:
            // printIndentation(indent + 1);
            // std::cout << "Identifier: " << node->id_name << std::endl;
            // break;
        case AST_INT:
            // printIndentation(indent + 1);
            // std::cout << "Value: " << node->int_value << std::endl;
            // break;
        case AST_BOOL:
            // printIndentation(indent + 1);
            // std::cout << "Value: " << (node->bool_value ? "true" : "false") << std::endl;
            break;
        // case AST_SPEC:
        //     printIndentation(indent + 1);
        //     std::cout << "Type Annotations:" << std::endl;
        //     printTypeAnnotations(node->spec_type_annotations, indent + 2);
            
        //     printIndentation(indent + 1);
        //     std::cout << "Formulas:" << std::endl;
        //     printFormulas(node->spec_formulas, indent + 2);
        //     break;
        case AST_NOT:
        case AST_H:
        case AST_O:
        case AST_Y:
            // printIndentation(indent + 1);
            // std::cout << "Child:" << std::endl;
            // printAST(node->unary_child, indent + 2);
            PreProcess(node->unary_child);
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
            // printIndentation(indent + 1);
            // std::cout << "Left:" << std::endl;
            // printAST(node->binary_left, indent + 2);
            
            // printIndentation(indent + 1);
            // std::cout << "Right:" << std::endl;
            // printAST(node->binary_right, indent + 2);
            PreProcess(node->binary_left);
            PreProcess(node->binary_right);
            break;
        default:
            // printIndentation(indent + 1);
            std::cout << "Unknown node type" << std::endl;
    }
}

