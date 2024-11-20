#pragma once

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <stack>
#include <vector>
#include "ast.hpp"
#include "lexer.hpp"

struct ScopedIdentifier {
    unsigned scopeDepth;
    std::string ident;
    bool isRegister;
    bool isFunctionParam;

    ScopedIdentifier(unsigned scopeDepth, std::string ident, bool isRegister = false,
                     bool isFunctionParam = false)
        : scopeDepth(isFunctionParam ? scopeDepth + 1 : scopeDepth), // no shadowing of function parameters in top block
          ident(ident),
          isRegister(isRegister),
          isFunctionParam(isFunctionParam) {}
};

class Scopes {
    // keep track of current scope while parsing to verify program semantics
    // maps identifier name to (scope depth, flags)
    std::unordered_map<std::string, std::stack<ScopedIdentifier>> scopes;
    unsigned currentDepth = 0;

   public:
    std::optional<ScopedIdentifier> getIdentifier(std::string name) {
        auto& stack = scopes[name];
        if (stack.empty()) {
          return std::nullopt;
        }
        
        return stack.top();
    }

    bool addIdentifier(std::string name, bool isRegister, bool isFunctionParam) {
        if (!scopes[name].empty()) {
            auto scope = scopes[name].top();

            if (!scope.isRegister && scope.scopeDepth >= currentDepth) {
                return false;
            }

            scopes[name].push(ScopedIdentifier(currentDepth, name));
            return true;
        }

        scopes[name].push(
            ScopedIdentifier(currentDepth, name, isRegister, isFunctionParam));
        return true;
    }

    void enterScope() { currentDepth++; }

    void leaveScope() {
        for (auto& e : scopes) {
            if (e.second.empty())
                continue;

            if (e.second.top().scopeDepth >= currentDepth) {
                e.second.pop();
            }
        }

        currentDepth--;
    }
};

class Parser {
    TokenStream& ts;

    Scopes scopes;

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
