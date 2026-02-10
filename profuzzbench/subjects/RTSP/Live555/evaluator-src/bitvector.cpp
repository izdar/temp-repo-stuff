# include "bitvector.h"

void BitVector::print_bv(std::map<int, std::string> &serial_to_formula_str, const std::string &label) 
{
    for(unsigned int i = 0 ; i < sz; ++i)
    {
        bool result = test(i); 
        cout << label << "[" << serial_to_formula_str[i] << "] = " << (result ? "true" : "false") << endl;
    }
}


BitVector::BitVector(unsigned int size) {
    sz = size;
    whole_sz = (size + 31) / 32;
    bv = new unsigned int[whole_sz]();
    if(!bv){
        std::cerr << "Error: Memory allocation failed for BitVector." << std::endl;
        std::exit(EXIT_FAILURE);
    }
    clear_bv();
}

BitVector::~BitVector() {
    delete[] bv;
    bv = nullptr; // Prevent use-after-free
}

// Add copy constructor
BitVector::BitVector(const BitVector& other) {
    sz = other.sz;
    whole_sz = other.whole_sz;
    bv = new unsigned int[whole_sz]();
    if(!bv){
        std::cerr << "Error: Memory allocation failed for BitVector copy." << std::endl;
        std::exit(EXIT_FAILURE);
    }
    // Copy the bits
    for(unsigned int i = 0; i < whole_sz; ++i) {
        bv[i] = other.bv[i];
    }
}

// Add copy assignment operator
BitVector& BitVector::operator=(const BitVector& other) {
    if(this != &other) { // Self-assignment check
        // Free existing resources
        delete[] bv;
        
        // Allocate new resources
        sz = other.sz;
        whole_sz = other.whole_sz;
        bv = new unsigned int[whole_sz]();
        if(!bv){
            std::cerr << "Error: Memory allocation failed for BitVector assignment." << std::endl;
            std::exit(EXIT_FAILURE);
        }
        
        // Copy the bits
        for(unsigned int i = 0; i < whole_sz; ++i) {
            bv[i] = other.bv[i];
        }
    }
    return *this;
}

void BitVector::set(unsigned int index) {
    if(index >= sz) {
        std::cerr << "Error: BitVector::set - Index out of bounds: " << index << " >= " << sz << std::endl;
        return;
    }
    bv[index / 32] |= (1 << (index % 32));
}

void BitVector::clear(unsigned int index) {
    if(index >= sz) {
        std::cerr << "Error: BitVector::clear - Index out of bounds: " << index << " >= " << sz << std::endl;
        return;
    }
    bv[index / 32] &= ~(1 << (index % 32));
}

bool BitVector::test(unsigned int index) {
    if(index >= sz) {
        std::cerr << "Error: BitVector::test - Index out of bounds: " << index << " >= " << sz << std::endl;
        return false;
    }
    return (bv[index / 32] & (1 << (index % 32))) != 0;
}

void BitVector::clear_bv() {
    for(unsigned int i = 0; i < whole_sz; ++i) {
        bv[i] = 0;
    }
}

unsigned int BitVector::get_size() {
    return sz;
}