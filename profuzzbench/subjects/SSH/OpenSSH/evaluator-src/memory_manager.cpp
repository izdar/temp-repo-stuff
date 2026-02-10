#include "memory_manager.h"

void MemoryManager::freeAST(ASTNode* node) {
    if (!node) return;
    
    // Free children first
    switch (node->kind) {
        case AST_NOT:
        case AST_H:
        case AST_O:
        case AST_Y:
            freeAST(node->unary_child);
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
            freeAST(node->binary_left);
            freeAST(node->binary_right);
            break;
        case AST_SPEC:
            // Free formulas
            for (auto formula : node->spec_formulas) {
                freeAST(formula);
            }
            break;
        default:
            break;
    }
    
    // Free the node itself
    delete node;
}

void MemoryManager::freeSpec(const Spec& spec) {
    // Free formulas
    for (auto formula : spec.second) {
        freeAST(formula);
    }
    // TypeAnnotations are not dynamically allocated in this version
}