#ifndef EVALUATOR_H_
#define EVALUATOR_H_

# include <iostream> 
# include <vector>
# include <cassert>
# include <cstdlib>
# include <cstring>
# include <algorithm>
# include <map>
# include <set>
# include "ast.h"
# include "typechecker.h"
# include "state.h"
# include "bitvector.h"
# include "memory_manager.h"
# include "ast_printer.h"
using namespace std ;

# define NODE_NOT_NULL(node) ((node) != NULL)
# define BOTH_CHILD_PRESENT(node) (NODE_NOT_NULL(node->binary_left)  && NODE_NOT_NULL((node)->binary_right))
# define LEFT_CHILD_FIXED_TYPE(node, type) ((node)->binary_left->kind == type)
# define RIGHT_CHILD_FIXED_TYPE(node, type) ((node)->binary_right->kind == type)

class Evaluator
{

private: 
    vector<BitVector> new_bv, old_bv ; 
    vector<ASTNode*> formulas ;
    vector<int> serial_numbers ;
    // TypeChecker *Tchecker ;
    int index ; 
    bool EvaluateFormula(ASTNode* node, State *state, size_t iter);
    bool EvaluatePredicate(ASTNode* node, State *state);
    // void Bootstrap(ASTNode * f, int iter) ; 

public:
    Evaluator(vector<ASTNode*> &formulas, vector<int> &snums);
    void reset_evaluator();
    vector<bool> EvaluateOneStep(State *state);
    int get_index() const { return index; }
    void set_index(int idx) { index = idx; }
    
    vector<BitVector> get_old_bv() const { return old_bv; }
    vector<BitVector> get_new_bv() const { return new_bv; }
    
    void set_old_bv(const vector<BitVector>& bv) { old_bv = bv; }
    void set_new_bv(const vector<BitVector>& bv) { new_bv = bv; }

};

#endif 