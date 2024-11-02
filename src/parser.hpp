#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include "lexer.hpp"
#include "debug.hpp"

enum ExpressionType {

};

class Expression {
    bool isUnOp();
    bool isBinOp();
    bool isFunctionCall();
    bool isLiteral();
};

class Identifier : Expression {
    std::string getName();
};

class IntLiteral : Expression {
    uint64_t getValue();
};

class BinOp : Expression {
    Expression getLeftExpression();
    Expression getRightExpression();
};

class UnOp : Expression {
    Expression getExpression();
};

enum StatementType {
    While, If
};

class Statement {

};

class Declaration {
    class Identifier getKey();
    Expression getExpressionValue();
    bool isAuto();
    bool isRegister(); 
};

class DeclStatement {
    bool isDeclaration();

};

class Block {
    std::vector<Statement> getStatements();
};

class Function {
    Block getBlock();
    class Identifier getParamAt(int idx);
};

class AbstractSyntaxTree {

};

class Parser {
    TokenStream& ts;
    AbstractSyntaxTree ast;
    DiagnosticsManager dm;

    Parser(TokenStream& ts) : ts(ts) {}

    void parsingError() {

    }

    void matchToken(TokenType type) {
        Token token = ts.next();

        if (token.type != type) {
            log("Did not match token types: %d %d \n", type, token.type);
            dm.parsingError(token);
        }
    }

    Expression parseExpression() {

    }

    Block parseBlock() {

    }

    Statement parseStatement() {
        Token next = ts.peek();

        switch (next.type) {
            case TokenType::KeyWhile: {
                parseWhileStatement();
            }
            default: {
                parsingError();
                exit(EXIT_FAILURE);
            }
        }

    }


    Statement parseWhileStatement() {
        matchToken(TokenType::KeyWhile);
        matchToken(TokenType::ParenthesisL);
        Expression expr = parseExpression();
        matchToken(TokenType::ParenthesisR);
        Block block = parseBlock();

        return Statement(StatementType::While, expr, block);
    }
};
