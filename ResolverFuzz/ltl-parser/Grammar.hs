Grammar
======================================= SYNTAX =========================================
Specification ::= TypeAnnotationList Formulas
TypeAnnotationList ::= TypeAnnotation | TypeAnnotation TypeAnnotationList
TypeAnnotation ::=
                "Enum" Id "{" Comma_separated_Id_List "}" ";"
                "Int" Id ";"
                "Bool" Id ";"
Formulas ::= Formula ";" | Formula ";" Formulas                
Formula ::= "(" Formula ")" | Formula "->" Formula | "!" Formula | Formula "&" Formula | "true" | "false" | Predicates | "O" Formula | "H" Formula | Formula "S" Formula | "Y" Formula
Predicates ::= Id ">" Term | Id "<" Term | Id "=" Term | Id "!=" Term | Id ">=" Term | Id "<=" Term  
Comma_separated_Id_List ::= Id | Id "," Comma_separated_Id_List
Term ::= Id | Int | Bool
Id ::= [a-zA-Z_][a-zA-Z0-9_]*
Int ::= [0-9]+
Bool ::= "true" | "false"

Type_checker rules
======================================= TYPE CHECKING RULES =========================================

Bool = Bool
Bool != Bool
Int = Int
Int > Int
Int >= Int
Int < Int
Int <= Int
Int != Int
Enum(T) = Enum(T)
Enum(T) != Enum(T)
