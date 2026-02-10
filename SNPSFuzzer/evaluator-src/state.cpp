# include "state.h" 

State::State(TypeChecker *tc) : Tchecker(tc) {
    // Initialize the LabelingFunction map
    LabelingFunction = std::map<std::string, std::string>();
    for( auto consts : Tchecker->constant_list)
    {
        LabelingFunction[consts] = consts;
    }   
}

void State::addLabel(std::string vname, std::string val) {
    // Add a label to the LabelingFunction map
    if(LabelingFunction.find(vname) != LabelingFunction.end()) {
        std::cerr << "Error: Variable " << vname << " already has a label." << std::endl;
        assert(0);
        return;
    }
    LabelingFunction[vname] = val;
}

std::string State::printState()
{
    // Print the current state of the LabelingFunction map
    std::string result="";
    for (const auto& pair : LabelingFunction) {
        result += "<" + pair.first + " => " + pair.second + ">\n";
    }
    return result;
}

void State::clearState() {
    // Clear the LabelingFunction map
    LabelingFunction.clear();
}

std::string State::getLabel(std::string vname)
{
    // Get the label for a variable from the LabelingFunction map
    auto it = LabelingFunction.find(vname);
    if (it != LabelingFunction.end()) {
        return it->second;
    } else {
        std::cerr << "Error: Variable not found in labeling function: " << vname << std::endl;
        assert(0);
        return "";
    }
}

std::pair<std::string, std::string> State::getType(std::string variable_name)
{
    // Get the type of a variable from the TypeChecker
    return Tchecker->getType(variable_name);
}

bool isNumberFormat(std::string& str) {
    if(str.empty()) return false;
    std::string::iterator it = str.begin();
    if(str[0]=='-') ++it; // Skip the sign

    // Check if the string is a valid number format
    return !str.empty() && std::all_of(it, str.end(), ::isdigit);
}

bool State::IsSane()
{
    for(auto sub : LabelingFunction)
    {
        std::string varname = sub.first;
        std::string val = sub.second;

        std::pair<std::string, std::string> substitution_type = Tchecker->getType(varname);
        if(substitution_type.first == "ENUM" || substitution_type.first == "BOOL")
        {
            std::pair<std::string, std::string> val_type = Tchecker->getType(val);
            if(!CHECK_PAIR_EQUALITY(substitution_type, val_type))
            {
                std::cerr << "Error: Invalid substitution for variable " << varname << ": expected " << substitution_type.first << ", got " << val_type.first << std::endl;
                return false;
            }

        }
        else 
        {
            assert(substitution_type.first == "INT");
            if(!isNumberFormat(val))
            {
                std::cerr << "Error: Invalid substitution for variable " << varname << ": expected INT, got " << val << std::endl;
                return false;
            }
        }
        
    }
    return true;
}