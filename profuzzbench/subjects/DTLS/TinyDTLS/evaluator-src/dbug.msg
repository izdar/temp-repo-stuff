Parsing successful!
Type check passed.
Serial Number: 13
SPECIFICATION:
  Type Annotations:
    Type Annotation: INTEGER_TYPE
      Name: ap_status_code
    Type Annotation: INTEGER_TYPE
      Name: status_code
    Type Annotation: ENUM_TYPE
      Name: request
      Values: commit_bad, req_commit_success
    Type Annotation: ENUM_TYPE
      Name: response
      Values: commit_error, res_commit_success
  Formulas:
    Formula 1:
      Node Type: H_OPERATOR
        Serial Number: 0
        Child:
          Node Type: IMPLICATION
            Serial Number: 1
            Left:
              Node Type: AND
                Serial Number: 2
                Left:
                  Node Type: EQUAL
                    Serial Number: 3
                    Left:
                      Node Type: IDENTIFIER
                        Identifier: response
                        Serial Number: 4
                    Right:
                      Node Type: IDENTIFIER
                        Identifier: res_commit_success
                        Serial Number: 5
                Right:
                  Node Type: O_OPERATOR
                    Serial Number: 6
                    Child:
                      Node Type: EQUAL
                        Serial Number: 7
                        Left:
                          Node Type: IDENTIFIER
                            Identifier: status_code
                            Serial Number: 8
                        Right:
                          Node Type: INTEGER
                            Value: 126
                            Serial Number: 9
            Right:
              Node Type: EQUAL
                Serial Number: 10
                Left:
                  Node Type: IDENTIFIER
                    Identifier: request
                    Serial Number: 11
                Right:
                  Node Type: IDENTIFIER
                    Identifier: req_commit_success
                    Serial Number: 12
