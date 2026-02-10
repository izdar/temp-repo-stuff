CXX = g++
CXXFLAGS = -Wall -g -std=c++20

# Check OS and set appropriate flex library
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
# macOS might not need -lfl or might use -ll
 FLEXLIB =
else
 FLEXLIB = -lfl
endif

formula_parser: parser.o lexer.o ast_printer.o memory_manager.o main.o typechecker.o preprocess.o state.o evaluator.o bitvector.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(FLEXLIB)

parser.o: parser.cpp
	$(CXX) $(CXXFLAGS) -c parser.cpp -o parser.o

lexer.o: lexer.cpp
	$(CXX) $(CXXFLAGS) -c lexer.cpp -o lexer.o

ast_printer.o: ast_printer.cpp
	$(CXX) $(CXXFLAGS) -c ast_printer.cpp -o ast_printer.o

memory_manager.o: memory_manager.cpp
	$(CXX) $(CXXFLAGS) -c memory_manager.cpp -o memory_manager.o

state.o: state.cpp
	$(CXX) $(CXXFLAGS) -c state.cpp -o state.o

preprocess.o: preprocess.cpp
	$(CXX) $(CXXFLAGS) -c preprocess.cpp -o preprocess.o

typechecker.o: typechecker.cpp
	$(CXX) $(CXXFLAGS) -c typechecker.cpp -o typechecker.o

bitvector.o: bitvector.cpp
	$(CXX) $(CXXFLAGS) -c bitvector.cpp -o bitvector.o

evaluator.o: evaluator.cpp
	$(CXX) $(CXXFLAGS) -c evaluator.cpp -o evaluator.o

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

lexer.cpp: lexer.l
	flex -o lexer.cpp lexer.l

parser.cpp parser.hpp: parser.y
	bison -d -o parser.cpp parser.y

clean:
	rm -f formula_parser *.o lexer.cpp parser.cpp parser.hpp

.PHONY: clean