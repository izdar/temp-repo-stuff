#ifndef PREPROCESS_H
#define PREPROCESS_H

# include <iostream> 
# include <fstream>
# include <map>
# include "ast.h"
# include <vector>
# include <cassert> 
using namespace std; 

class Preprocessor
{
    public: 
    int snum ; 
    vector<int> DoPreProcess(vector<ASTNode*>& asts);
    void PreProcess(ASTNode* node);

};


#endif 