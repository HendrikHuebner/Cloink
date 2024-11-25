# Toy compiler project

C-Subset compiler without types, using LLVM IR.

### Grammar:

```
    program: function*
    function: ident "(" paramlist? ")" block
    paramlist: ident ("," ident)*
    block: "{" declstmt* "}"
    declstmt: ("auto" | "register") ident "=" expr ";"
            | statement
    statement: "return" expr? ";"
             | block
             | "while" "(" expr ")" statement
             | "if" "(" expr ")" statement ("else" statement)?
             | expr ";"
    expr: "(" expr ")"
        | ident "(" arglist? ")"
        | ident
        | number
        | "-" expr
        | "!" expr
        | "~" expr
        | "&" expr
        | expr "[" expr sizespec? "]"
        | expr "+" expr
        | expr "-" expr
        | expr "*" expr
        | expr "/" expr
        | expr "%" expr
        | expr "<<" expr
        | expr ">>" expr
        | expr "<" expr
        | expr ">" expr
        | expr "<=" expr
        | expr ">=" expr
        | expr "==" expr
        | expr "!=" expr
        | expr "&" expr
        | expr "|" expr
        | expr "^" expr
        | expr "&&" expr
        | expr "||" expr
        | expr "=" expr
    arglist: expr ("," expr)*
    sizespec: "@" number
    ident: r"[a-zA-Z_][a-zA-Z0-9_]*"
    number: r"[0-9]+"
```