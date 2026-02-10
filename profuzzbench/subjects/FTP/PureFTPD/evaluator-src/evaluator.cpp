# include "evaluator.h"
#include "ast_printer.h"
#include <fstream>
#include <iostream>

Evaluator::Evaluator(vector<ASTNode*> &formulas, vector<int> &snums)
{
    this->formulas = formulas;
    this->serial_numbers = snums;
    index = 0;
    // Tchecker = tc ; 
    
    // Preallocate vector capacity to avoid reallocations
    new_bv.reserve(serial_numbers.size());
    old_bv.reserve(serial_numbers.size());
    
    for(auto serial : serial_numbers)
    {
        // Create BitVector objects directly in the vectors
        new_bv.emplace_back(serial);
        old_bv.emplace_back(serial);
        
        // Clear them after construction
        new_bv.back().clear_bv();
        old_bv.back().clear_bv();
    }
}

void Evaluator::reset_evaluator() {
    this->index = 0;
    
    // Clear existing vectors (this will properly call destructors)
    new_bv.clear();
    old_bv.clear();
    
    // Preallocate vector capacity to avoid reallocations
    new_bv.reserve(serial_numbers.size());
    old_bv.reserve(serial_numbers.size());
    
    for(auto serial : serial_numbers)
    {
        // Create BitVector objects directly in the vectors using emplace_back
        new_bv.emplace_back(serial);
        old_bv.emplace_back(serial);
        
        // Clear them after construction
        new_bv.back().clear_bv();
        old_bv.back().clear_bv();
    }
}

inline std::string boolToString(bool value) {
    return value ? "true" : "false";
}
inline bool stringToBool(const std::string& str) 
{
    return (str == "true" || str == "1");
}
bool Evaluator::EvaluatePredicate(ASTNode* node, State *state)
{
    // std::cout << "Evaluating predicate: " << ASTPrinter::printStuff(node) << std::endl;

    assert(NODE_NOT_NULL(node));
    // assert(BOTH_CHILD_PRESENT(node));


    if (!BOTH_CHILD_PRESENT(node)) {
        std::cout << "ERROR: Not a binary Predicate" << std::endl;

        std::ofstream out("ast_dump.txt");
        if (!out) {
            std::cerr << "Failed to open ast_dump.txt" << std::endl;
            return false;
        }

        // Save old buffer
        std::streambuf* old_buf = std::cout.rdbuf();

        // Redirect cout to file
        std::cout.rdbuf(out.rdbuf());

        ASTPrinter::printAST(node, 0);

        // Restore cout
        std::cout.rdbuf(old_buf);

        assert(false);
    }

    // assert(LEFT_CHILD_FIXED_TYPE(node, AST_ID));
    std::string varname = node->binary_left->id_name;
    int l_int_val ; 
    int r_int_val ; 
    std::string r_string_val ; 
    std::string  l_string_val ;

    switch(node->kind)
    {
        case AST_GT: 
        case AST_GTE:
        case AST_LT:
        case AST_LTE:
            // assert(RIGHT_CHILD_FIXED_TYPE(node, AST_INT));
            if(node->binary_right->kind == AST_INT){
                r_int_val = node->binary_right->int_value;
            }
            else if(node->binary_right->kind == AST_ID){
                r_int_val = stoi(state->getLabel(node->binary_right->id_name));    
            }
            else assert(0);
            if(node->binary_left->kind == AST_INT){
                l_int_val = node->binary_left->int_value;
            }
            else if(node->binary_left->kind == AST_ID){
                l_int_val = stoi(state->getLabel(node->binary_left->id_name));    
            }
            else assert(0);
            // r_int_val = node->binary_right->int_value;
            // l_int_val = stoi(state->getLabel(varname)); 
        break ;
        case AST_EQ:
        case AST_NEQ:
            
            switch(node->binary_left->kind)
            {
                case AST_INT:
                    l_string_val = to_string(node->binary_left->int_value);
                    break; 
                case AST_BOOL:
                    l_string_val = boolToString(node->binary_left->bool_value);
                    break; 
                case AST_ID:
                    l_string_val = state->getLabel(node->binary_left->id_name);
                    break; 
                default: 
                    assert(0);
                break ; 
            }
            switch(node->binary_right->kind)
            {
                case AST_INT:
                    r_string_val = to_string(node->binary_right->int_value);
                    break; 
                case AST_BOOL:
                    r_string_val = boolToString(node->binary_right->bool_value);
                    break; 
                case AST_ID:
                    r_string_val = state->getLabel(node->binary_right->id_name);
                    break; 
                default: 
                    assert(0);
                break ; 
            }            
            // if(node->binary_right->kind == AST_INT)
            // {
            //     // assert(LEFT_CHILD_FIXED_TYPE(node, AST_INT));
            //     l_string_val = state->getLabel(varname);
            //     r_string_val = to_string(node->binary_right->int_value);
            // }
            // else if(node->binary_right->kind == AST_BOOL)
            // {
            //     l_string_val = state->getLabel(varname);
            //     r_string_val = boolToString(node->binary_right->bool_value);
            // }
            // else if(node->binary_right->kind == AST_ID)
            // {
            //     l_string_val = state->getLabel(varname);
            //     r_string_val = node->binary_right->id_name;
            // }
            // else{
            //     assert(0);
            //     std::cerr << "Error: Unknown node type encountered during predicate evaluation." << std::endl;
            //     l_string_val = state->getLabel(varname);
            //     r_string_val = node->binary_right->id_name;
            // }
            break; 
        default:
            std::cerr << "Error: Unknown node type encountered during predicate evaluation." << std::endl;
            assert(0);
                    
    }

    // std::cout << "LVALUE = " << l_string_val << std::endl;
    // std::cout << "RVALUE = " << r_string_val << std::endl;

    switch(node->kind)
    {
        case AST_GT: 
            return l_int_val > r_int_val ;
        case AST_GTE:
            return l_int_val >= r_int_val ; 
        case AST_LT:
            return l_int_val < r_int_val ;
        case AST_LTE:
            return l_int_val <= r_int_val ;
        case AST_EQ: 
            return l_string_val == r_string_val ;
        case AST_NEQ:
            return l_string_val != r_string_val ;
        default: 
            std::cerr << "Error: Unknown node type encountered during predicate evaluation." << std::endl;
            assert(0); 
    }
    assert(0); 
    return false ; 
}



bool Evaluator::EvaluateFormula(ASTNode* node, State *state, size_t iter)
{
    assert(NODE_NOT_NULL(node));
    switch(node->kind)
    {
        case AST_NOT:
        {
            bool r = !EvaluateFormula(node->unary_child, state, iter);
            if(r) new_bv[iter].set(node->serial_number);    
            return r ;
        }             
        case AST_AND:
        {
            bool r1 = EvaluateFormula(node->binary_left, state,iter) ; 
            bool r2 = EvaluateFormula(node->binary_right, state,iter);
            if(r1 && r2) new_bv[iter].set(node->serial_number);
            return (r1 && r2);  
        }
        case AST_OR:
        {
            bool r1 = EvaluateFormula(node->binary_left, state,iter) ; 
            bool r2 = EvaluateFormula(node->binary_right, state,iter);
            if(r1 || r2) new_bv[iter].set(node->serial_number);
            return r1 || r2;
        }
        case AST_ARROW:
        {
            bool r1 = (!EvaluateFormula(node->binary_left, state,iter));
            bool r2 = EvaluateFormula(node->binary_right, state,iter);
            if(r1|| r2) new_bv[iter].set(node->serial_number);
            return r1 || r2;
        }
        case AST_ID:
        {
            std::string varname = node->id_name;
            std::string val = state->getLabel(varname);
            bool r ; 
            if(val == "true") r = true ;
            else if(val == "false") r = false ;
            else{
                std::cerr << "Error: Unknown value encountered during evaluation." << std::endl;
                assert(0);
            }
            if(r) new_bv[iter].set(node->serial_number);
            return r ;
        }
        case AST_S:
        {
            bool r1 = EvaluateFormula(node->binary_left, state,iter);
            bool r2 = EvaluateFormula(node->binary_right, state,iter);
            bool r3 = false ;
            if(old_bv[iter].test(node->serial_number)) r3 = true; 
            if(r2 || (r1 && r3))
            {
                new_bv[iter].set(node->serial_number);
            }
            return (r2 || (r1 && r3));
        }
        case AST_O:
        {
            bool r1 = EvaluateFormula(node->unary_child, state,iter);
            bool r2 = false ; 
            if(old_bv[iter].test(node->serial_number)) r2 = true ;
            if(r1 || r2)
            {
                new_bv[iter].set(node->serial_number);
            }
            return (r1 || r2);    
        }
        case AST_H:
        {
            bool r1 = EvaluateFormula(node->unary_child, state,iter);
            bool r2 = index == 0 ? true :  old_bv[iter].test(node->serial_number); 
            if(r1 && r2) new_bv[iter].set(node->serial_number);
            return (r1 && r2);
        }
            
        case AST_Y:
        {
            EvaluateFormula(node->unary_child, state,iter);
            if(index != 0)
            {
                if(old_bv[iter].test((node->unary_child)->serial_number)){ 
                     new_bv[iter].set(node->serial_number);
                    return true ;
                }
            }
            return false; 

        }

        default:
            return EvaluatePredicate(node, state);
    }
    return false ; 
}





vector<bool> Evaluator::EvaluateOneStep(State *state)
{
    vector<bool> result;
    size_t iter = 0;
    for (auto formula : formulas)
    {
        bool res = EvaluateFormula(formula, state, iter);
        result.push_back(res);

        if (!res) {
            std::string snapshot = state->printState();
            // std::string formula_str = ASTPrinter::formula_to_string(formula);
            // monitor_report_violation(formula_str, snapshot, index);
        }

        old_bv[iter] = new_bv[iter];
        new_bv[iter].clear_bv();
        ++iter;
    }
    ++index;
    return result;
}
