#ifndef BITVECTOR_H
#define BITVECTOR_H

# include <cstdio> 
# include <cassert> 
# include <iostream> 
# include "ast_printer.h"
# include <map> 
# include <string> 
using namespace std;

class BitVector{
    unsigned int * bv;
    unsigned int whole_sz;
    unsigned int sz;
public:
    BitVector(unsigned int size);
    ~BitVector();
    // Rule of Three implementation
    BitVector(const BitVector& other);
    BitVector& operator=(const BitVector& other);
    void print_bv(std::map<int, std::string> &serial_to_formula_str, const std::string &label);
    void set(unsigned int index);
    void clear(unsigned int index);
    bool test(unsigned int index);
    void clear_bv();
    unsigned int get_size();
};

#endif 