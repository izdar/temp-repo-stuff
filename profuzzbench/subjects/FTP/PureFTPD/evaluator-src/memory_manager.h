// memory_manager.h
#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "ast.h"

class MemoryManager {
public:
    static void freeAST(ASTNode* node);
    static void freeSpec(const Spec& spec);
};

#endif // MEMORY_MANAGER_H