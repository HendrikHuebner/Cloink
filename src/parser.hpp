#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "ast.hpp"
#include "lexer.hpp"

namespace clonk {

class Parser {
    TokenStream& ts;

    SymbolTable<std::string> scopes;

    // function parameter counts
    std::unordered_map<std::string, size_t> paramCounts;

   public:
    Parser(TokenStream& ts) : ts(ts) {}

    AbstractSyntaxTree parseProgram() {
        std::vector<std::unique_ptr<Function>> functions;

        while (!ts.empty()) {
            functions.push_back(parseFunction());
        }

        return AbstractSyntaxTree(functions);
    }

   private:
    void parsingError();

    void matchToken(TokenType type, const std::string& expected = "");

    Identifier matchIdentifier();

    std::unique_ptr<Expression> parseExpression();

    std::unique_ptr<Expression> parseTerm();

    std::unique_ptr<Block> parseBlock();

    std::unique_ptr<Expression> parseValue(bool lvalue = false);

    std::vector<Identifier> parseParamlist();

    std::vector<std::unique_ptr<Expression>> parseFunctionCallParamList();

    std::unique_ptr<Function> parseFunction();

    std::unique_ptr<Statement> parseDeclStatement();

    std::unique_ptr<Statement> parseStatement();
};

} // end namespace clonk
