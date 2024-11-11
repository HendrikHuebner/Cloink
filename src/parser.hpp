#pragma once

#include <memory>
#include <vector>
#include "lexer.hpp"
#include "ast.hpp"

class DiagnosticsManager;

class Parser {
    
    TokenStream& ts;
    DiagnosticsManager& dm;

  public:
    Parser(TokenStream& ts, DiagnosticsManager& dm) : ts(ts), dm(dm) {}

    AbstractSyntaxTree parseProgram() {
        std::vector<std::unique_ptr<Function>> functions;

        while (!ts.empty()) {
            functions.push_back(parseFunction());
        }

        return AbstractSyntaxTree(functions);
    }
  
  private:
    void parsingError();

    void matchToken(TokenType type);

    Identifier matchIdentifier();

    std::unique_ptr<Expression> parseExpression();

    std::unique_ptr<Block> parseBlock();

    std::vector<Identifier> parseParamlist();

    std::unique_ptr<Function> parseFunction();

    std::unique_ptr<Statement> parseDeclStatement();

    std::unique_ptr<Statement> parseStatement();
};
