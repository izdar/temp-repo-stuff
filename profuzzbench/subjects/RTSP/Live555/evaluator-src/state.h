#ifndef STATE_H_
#define STATE_H_

# include <string> 
# include <vector> 
# include <iostream>
# include <cassert>
# include <cstdlib>
# include <cstring>
# include <algorithm>
# include <map>
# include <set>
# include "ast.h"
# include "typechecker.h" 
using namespace std ; 


class State 
{
private: 
    TypeChecker *Tchecker ; 
public: 
    State(TypeChecker *tc);
    void addLabel(std::string vname, std::string val);
    std::string getLabel(std::string vname); 
    std::map<std::string, std::string> LabelingFunction;
    std::pair<std::string, std::string> getType(std::string variable_name);
    bool IsSane() ; 
    void clearState();
    std::string printState();
};

#endif 