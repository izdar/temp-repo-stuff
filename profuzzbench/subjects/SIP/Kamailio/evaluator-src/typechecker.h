#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

# include <iostream>
# include <fstream>
# include <map> 
# include "ast.h"
# include "ast_printer.h"
# include "memory_manager.h"
# include <cassert> 
# include <cstdlib> 
using namespace std; 

# define FALSE_VALUE std::make_pair(false, std::make_pair(std::string(""), std::string("")))
# define MAKE_TRIPLE(a,b,c) std::make_pair((a), std::make_pair((b), (c)))
# define CHECK_PAIR_EQUALITY(a,b) ((a).first == (b).first && (a).second == (b).second)

class TypeChecker {
public: 
    TypeChecker(Spec spec);
    std::pair<std::string,std::string> getType(std::string variable_name);    
    std::vector<std::string> constant_list ;
private: 
   
    std::map<std::string, std::pair<std::string, std::string>> TypeContext ; 
    void LoadTypeContext(vector<TypeAnnotation> type_annotation_list);
    std::pair<bool, std::pair<std::string, std::string>> TypeCheck(ASTNode* node); 
    
};

#endif 