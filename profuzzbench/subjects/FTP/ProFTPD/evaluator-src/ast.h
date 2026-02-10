#ifndef AST_H_
#define AST_H_

#include <string>
#include <vector>

// Forward declarations
enum ASTNodeKind {
    AST_SPEC,
    AST_ENUM,
    AST_INT_TYPE,
    AST_BOOL_TYPE,
    AST_ARROW,
    AST_NOT,
    AST_AND,
    AST_OR,
    AST_BOOL,
    AST_ID,
    AST_INT,
    AST_GT,
    AST_GTE,
    AST_LT,
    AST_LTE,
    AST_EQ,
    AST_NEQ,
    AST_O,
    AST_H,
    AST_S,
    AST_Y
};

// TypeAnnotation class
class TypeAnnotation {
public:
    ASTNodeKind kind;
    
    // Constructor with default parameter to make it default-constructible
    TypeAnnotation(ASTNodeKind k = AST_ENUM) : kind(k) {}
    ~TypeAnnotation() = default;
    
    // Data for enum type
    std::string enum_name;
    std::vector<std::string> enum_values;
    
    // Data for int type
    std::string int_type_name;
    
    // Data for bool type
    std::string bool_type_name;
};

// Forward declaration of ASTNode
class ASTNode;

// ASTNode class
class ASTNode {
public:
    ASTNodeKind kind;
    
    // Constructor
    ASTNode(ASTNodeKind k) : kind(k), processed(false), serial_number(-1) {}
    ~ASTNode() = default;
    
    // Data for specification
    std::vector<TypeAnnotation> spec_type_annotations;
    std::vector<ASTNode*> spec_formulas;
    
    // Data for binary operations
    ASTNode* binary_left = nullptr;
    ASTNode* binary_right = nullptr;
    
    // Data for unary operations
    ASTNode* unary_child = nullptr;
    
    // Data for identifier
    std::string id_name;
    
    // Data for integer value
    int int_value = 0;
    
    // Data for boolean value
    bool bool_value = false;
    bool processed ; 
    int serial_number = 0 ; 
    
};

// The root of our AST will be a pair of vectors
using Spec = std::pair<std::vector<TypeAnnotation>, std::vector<ASTNode*>>;

#endif // AST_H