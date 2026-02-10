# include "typechecker.h"

TypeChecker::TypeChecker(Spec spec)
{
    // Load the type context from the specification
    LoadTypeContext(spec.first);
    size_t iter = 0 ; 
    // Type check each formula in the specification
    for (auto formula : spec.second) {
        if(!TypeCheck(formula).first){
            std::cerr << "Type check failed for the following formula." << std::endl;
            ASTPrinter::printAST(formula, 0);
            std::cerr << "Type check failed. for Formula ID: " << iter << std::endl;
            MemoryManager::freeSpec(spec);
            assert(0);  
        }
        ++iter ; 
    }
    std::cout << "Type check passed." << std::endl;
} 



void TypeChecker::LoadTypeContext(vector<TypeAnnotation> type_annotation_list)
{
    for (const auto& annotation : type_annotation_list) {
        if (annotation.kind == AST_ENUM) {
            // Load enum type
            
            if(TypeContext.find(annotation.enum_name) == TypeContext.end()){
                // Load enum type
                TypeContext[annotation.enum_name] = {"ENUM", annotation.enum_name};
                for (const auto& value : annotation.enum_values) {
                    constant_list.push_back(value);
                    TypeContext[value] = {"ENUM", annotation.enum_name};
                }
            } else {
                std::cerr << "Error: Duplicate enum type name: " << annotation.enum_name << std::endl;
            }


        } else if (annotation.kind == AST_INT_TYPE) {
            if(TypeContext.find(annotation.int_type_name) == TypeContext.end()){
                // Load int type
                TypeContext[annotation.int_type_name] = {"INT", ""};
            } else {
                std::cerr << "Error: Duplicate integer type name: " << annotation.int_type_name << std::endl;
            }
        } else if (annotation.kind == AST_BOOL_TYPE) {
            // Load bool type
            if(TypeContext.find(annotation.bool_type_name) == TypeContext.end()){
                // Load int type
                TypeContext[annotation.bool_type_name] = {"BOOL", ""};
            } else {
                std::cerr << "Error: Duplicate bool type name: " << annotation.bool_type_name << std::endl;
            }
        }
    }
    assert(TypeContext.find("true") == TypeContext.end());
    assert(TypeContext.find("false") == TypeContext.end());
    TypeContext["true"] = {"BOOL", ""};
    TypeContext["false"] = {"BOOL", ""};
}



std::pair<bool, std::pair<std::string, std::string>> TypeChecker::TypeCheck(ASTNode* node)
{
    if (!node) {
        std::cerr << "Error: Null node encountered during type checking." << std::endl;
        return FALSE_VALUE; 
    }
    switch (node->kind)
    {
        case AST_ID:
        {
            // Check if the identifier is in the type context
            if (TypeContext.find(node->id_name) == TypeContext.end()) {
                std::cerr << "Error: Identifier not found in type context: " << node->id_name << std::endl;
                return FALSE_VALUE;
            }
            return MAKE_TRIPLE(true, TypeContext[node->id_name].first, TypeContext[node->id_name].second);
        }    // break;
        case AST_INT:
        {
            return MAKE_TRIPLE(true, "INT", ""); 
        }
        case AST_BOOL:{
            return MAKE_TRIPLE(true, "BOOL", "");
        }
        case AST_ARROW:
        case AST_AND:
        case AST_OR:
        case AST_S:{            
            std::pair<bool, std::pair<std::string, std::string>> leftType = TypeCheck(node->binary_left);
            std::pair<bool, std::pair<std::string, std::string>> rightType = TypeCheck(node->binary_right);
            if (!leftType.first || !rightType.first) {
                return FALSE_VALUE;
            }
            return MAKE_TRIPLE(true, "", ""); 
        }            
        case AST_EQ:
        case AST_NEQ:
        {
            std::pair<bool, std::pair<std::string, std::string>> leftType = TypeCheck(node->binary_left);
            std::pair<bool, std::pair<std::string, std::string>> rightType = TypeCheck(node->binary_right);
            if (!leftType.first || !rightType.first) {
                return FALSE_VALUE;
            }
            return MAKE_TRIPLE(CHECK_PAIR_EQUALITY(leftType.second, rightType.second), "", "");
        }

            // break; 
        case AST_GT:
        case AST_GTE:
        case AST_LT:
        case AST_LTE:
        {
            std::pair<bool, std::pair<std::string, std::string>> leftType = TypeCheck(node->binary_left);
            std::pair<bool, std::pair<std::string, std::string>> rightType = TypeCheck(node->binary_right);
            if (!leftType.first || !rightType.first) {
                return FALSE_VALUE;
            }
            return MAKE_TRIPLE(leftType.second.first == "INT" && CHECK_PAIR_EQUALITY(leftType.second, rightType.second), "", "");

        }    
        case AST_NOT:
        case AST_H:
        case AST_O:
        case AST_Y:
        {
            std::pair<bool, std::pair<std::string, std::string>> leftType = TypeCheck(node->unary_child);
            if (!leftType.first) {
                return FALSE_VALUE;
            }
            return MAKE_TRIPLE(true, "", ""); 
        }
        default:
            std::cerr << "Error: Unknown node type encountered during type checking." << std::endl;
            return FALSE_VALUE;
    }
    return FALSE_VALUE;
}

std::pair<std::string,std::string> TypeChecker::getType(std::string variable_name)
{
    auto it = TypeContext.find(variable_name);
    if (it != TypeContext.end()) {
        return it->second;
    } else {
        std::cerr << "Error: Variable not found in type context: " << variable_name << std::endl;
        assert(0);
        return std::make_pair("", "");
    }
}