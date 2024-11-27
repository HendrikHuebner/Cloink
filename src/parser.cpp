
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "diagnostics.hpp"
#include "debug.hpp"

using namespace clonk;

void Parser::matchToken(TokenType type, const std::string& expected) {
    Token token = ts.next();

    if (token.type != type) {
        DiagnosticsManager::get().unexpectedToken(ts, token, expected);
    }
}

Identifier Parser::matchIdentifier() {
    Token token = ts.next();

    if (token.type != TokenType::IdentifierType) {
        DiagnosticsManager::get().unexpectedToken(ts, token);
        exit(EXIT_FAILURE);
    }

    return Identifier(token.getIdentifier());
}

static int getBinOpPrecedence(TokenType op) {
    switch (op) {
        case TokenType::OpMultiply:
        case TokenType::OpDivide:
        case TokenType::OpModulo: return 12;
        case TokenType::OpPlus:
        case TokenType::OpMinus: return 11;
        case TokenType::OpShiftLeft:
        case TokenType::OpShiftRight: return 10;
        case TokenType::OpGreaterEq:
        case TokenType::OpGreaterThan:
        case TokenType::OpLessThan:
        case TokenType::OpLessEq: return 9;
        case TokenType::OpEquals:
        case TokenType::OpNotEquals: return 8;
        case TokenType::OpBitAnd: return 7;
        case TokenType::OpBitXor: return 6;
        case TokenType::OpBitOr: return 5;
        case TokenType::OpAnd: return 4;
        case TokenType::OpOr: return 3;
        case TokenType::OpAssign: return 1;
        default: return -1;
    }
}

std::vector<std::unique_ptr<Expression>> Parser::parseFunctionCallParamList() {
    matchToken(TokenType::ParenthesisL);
    std::vector<std::unique_ptr<Expression>> params;
    if (ts.peek().type != TokenType::ParenthesisR) {
        params.push_back(parseExpression());
    }

    while (ts.peek().type == TokenType::Comma) {
        ts.next();
        params.push_back(parseExpression());
    }
    
    matchToken(TokenType::ParenthesisR, "closing parenthesis of function call");

    return params;
}

std::unique_ptr<Expression> Parser::parseTerm() {
    TokenType next = ts.peek().type;
    std::unique_ptr<Expression> expr;

    switch (next) {
        case TokenType::ParenthesisL: {
            ts.next();
            expr = parseExpression();
            matchToken(TokenType::ParenthesisR, "closing parenthesis around expression");
            return expr;
        }
        case TokenType::OpBitAnd: {
            ts.next();
            Token token = ts.peek();
            expr = parseValue(true);
            
            auto scopedIdent = scopes.get(token.getIdentifier());
            if (scopedIdent->isRegister) {
                DiagnosticsManager::get().error(ts, "cannot reference register type \"" + token.getIdentifier() + "\"");
            }
            return std::make_unique<UnOp>(std::move(expr), TokenType::OpBitAnd);
        }
        case TokenType::OpNot: 
        case TokenType::OpMinus: 
        case TokenType::OpBitNot: {
            TokenType op = ts.next().type;
            TokenType next = ts.peek().type;
            expr = parseTerm();
            
            if (next != TokenType::ParenthesisL) {
                // check for binop
                BinOp* binop;

                if ((binop = dynamic_cast<BinOp*>(expr.get()))) {
                    if (binop->op == TokenType::OpAssign) {
                        DiagnosticsManager::get().unexpectedToken(ts, binop->op, "cannot assign to rvalue expression");
                    }

                    return std::make_unique<BinOp>(
                         std::make_unique<UnOp>(std::move(binop->leftExpr), op),
                        std::move(binop->rightExpr), binop->op);
                }
            }
            
            return std::make_unique<UnOp>(std::move(expr), op);
        }

        case TokenType::NumberLiteral: {
            Token num = ts.next();
            return std::make_unique<IntLiteral>(num.getValue());
        }
        case TokenType::IdentifierType: {
            return parseValue();
        }
        default: {
            ts.next();
            logger::warn("Unexpected Token: \n", next);
            DiagnosticsManager::get().unexpectedToken(ts, ts.peek());
            exit(EXIT_FAILURE);
        }
    }
}

std::unique_ptr<Expression> Parser::parseExpression() {
    std::unique_ptr<Expression> expr = parseTerm();
    
    while (true) {
        Token op = ts.peek();

        // binop
        if (op.type == TokenType::OpAssign && !dynamic_cast<LValue*>(expr.get())) {
            DiagnosticsManager::get().unexpectedToken(ts, op, "cannot assign to rvalue expression");
        }

        int precedence = getBinOpPrecedence(op.type);

        if (precedence == -1)
            return expr;

        if (precedence < 0) {
            logger::debug("Expected BinOp token: %d %d \n", op.type);
            DiagnosticsManager::get().unexpectedToken(ts, op);
            exit(EXIT_FAILURE);
        }
        
        ts.next();

        std::unique_ptr<Expression> right = parseTerm();

        BinOp* leftBinop;
        std::unique_ptr<Expression> result;

        if ((leftBinop = dynamic_cast<BinOp*>(expr.get()))) {
            
            if (op.type == TokenType::OpAssign) {
                DiagnosticsManager::get().unexpectedToken(ts, op, "cannot assign to rvalue expression");
            }

            int leftPrecedence = getBinOpPrecedence(leftBinop->op);

            if (precedence > leftPrecedence) {
                expr = std::make_unique<BinOp>(
                    std::move(leftBinop->leftExpr), 
                    std::make_unique<BinOp>(std::move(leftBinop->rightExpr), std::move(right), op.type),
                    leftBinop->op);

                continue;
            }
        }
        
        expr = std::make_unique<BinOp>(std::move(expr), std::move(right), op.type);
    }

    return expr;
}

std::unique_ptr<Block> Parser::parseBlock() {
    matchToken(TokenType::BraceL, "opening brace in block");
    scopes.enterScope();

    std::vector<std::unique_ptr<Statement>> statements;

    while(ts.peek().type != TokenType::BraceR) {
        statements.push_back(parseDeclStatement());
    }

    matchToken(TokenType::BraceR, "closing brace in block");

    scopes.leaveScope();
    return std::make_unique<Block>(std::move(statements));
}

std::vector<Identifier> Parser::parseParamlist() {
    std::vector<Identifier> params;

    if (ts.peek().type == TokenType::IdentifierType) {
        Identifier ident = matchIdentifier();
        scopes.insert(ident.name, ident.name, false, true);
        params.push_back(ident);
    }

    while (ts.peek().type == TokenType::Comma) {
        matchToken(TokenType::Comma, "parameter seperator comma");
        Identifier ident = matchIdentifier();
        if (!scopes.insert(ident.name, ident.name, false, true)) {
            DiagnosticsManager::get().error(ts, "duplicate function parameter: \"" + ident.name + "\"");
        }

        params.push_back(ident);
    }

    return params;
}

std::unique_ptr<Function> Parser::parseFunction() {
    Identifier ident = matchIdentifier();
    declaredFunctions.insert(ident.name);

    matchToken(TokenType::ParenthesisL, "parameter list opening parenthesis");
    std::vector<Identifier> params = parseParamlist();
    matchToken(TokenType::ParenthesisR, "parameter list closing parenthesis");
    std::unique_ptr<Block> block = parseBlock();
    
    return std::make_unique<Function>(ident, std::move(params), std::move(block));
}

std::unique_ptr<Expression> Parser::parseValue(bool lvalue) {
    Identifier ident = matchIdentifier();
    Expression* value;
    TokenType type = ts.peek().type;

    if (type == TokenType::ParenthesisL) {
        auto params = parseFunctionCallParamList();

        if (paramCounts.find(ident.name) == paramCounts.end()) {
            paramCounts[ident.name] = params.size();

        } else if (paramCounts[ident.name] != params.size()) {
            DiagnosticsManager::get().error(ts, "function \"" + ident.name 
            + "\" called with missmatching number of parameters: " 
            + std::to_string(params.size()) + " previously called with " 
            + std::to_string(paramCounts[ident.name]) + " parameters");
        }

        value = new FunctionCall(ident, std::move(params));

        if (lvalue && ts.peek().type != TokenType::BracketL) {
            DiagnosticsManager::get().error(ts, "expected lvalue");
            exit(EXIT_FAILURE);
        }

    } else {
        if (!scopes.get(ident.name)) {
            DiagnosticsManager::get().error(ts, "unknown identifier: \"" + ident.name + "\"");
        }

        value = new Identifier(ident);
    }

    // allow an arbitrary number of indexing expressions

    while (ts.peek().type == TokenType::BracketL) {
        ts.next();

        std::unique_ptr<Expression> idxExpr = parseExpression();

        if (ts.peek().type == TokenType::SizeSpec) {
            ts.next();
            Token sizeSpec = ts.next();

            if (sizeSpec.type != TokenType::NumberLiteral) {
                logger::warn("Invalid sizespec: \n", sizeSpec.type);
                DiagnosticsManager::get().unexpectedToken(ts, sizeSpec);
                exit(EXIT_FAILURE);

            } else if (sizeSpec.getValue() != 1 && sizeSpec.getValue() != 2 && sizeSpec.getValue() != 4 && sizeSpec.getValue() != 8) {
                DiagnosticsManager::get().unexpectedToken(ts, sizeSpec, "Invalid sizespec, must be 1, 2, 4 or 8, was " + std::to_string(sizeSpec.getValue()));
                exit(EXIT_FAILURE);
            }

            matchToken(TokenType::BracketR, "closing bracket of indexing operation");
            value = new IndexExpr(std::unique_ptr<Expression>(value), std::move(idxExpr), sizeSpec.getValue());
        
        } else {
            matchToken(TokenType::BracketR, "closing bracket of indexing operation");
            value = new IndexExpr(std::unique_ptr<Expression>(value), std::move(idxExpr));
        }
    }

    return std::unique_ptr<Expression>(value);
}

std::unique_ptr<Statement> Parser::parseDeclStatement() {
    TokenType type = ts.peek().type;
    if (type == TokenType::KeyAuto || type == TokenType::KeyRegister) {
        ts.next();
        Identifier ident = matchIdentifier();
        
        matchToken(TokenType::OpAssign, "assignment operator in declaration");
        std::unique_ptr<Expression> expr = parseExpression();
        matchToken(TokenType::EndOfStatement, "\";\"");

        if (!scopes.insert(ident.name, ident.name, type == TokenType::KeyRegister, false)) {
            DiagnosticsManager::get().error(ts, "redeclared identifier \"" + ident.name + "\"");
        }

        return std::make_unique<Declaration>( 
            type == TokenType::KeyAuto,
            type == TokenType::KeyRegister, ident, std::move(expr));

    } else {
        return parseStatement();
    }
}

std::unique_ptr<Statement> Parser::parseStatement() {
    TokenType next = ts.peek().type;
    if (next == TokenType::KeyReturn) {
        ts.next();
        if (ts.peek().type == TokenType::EndOfStatement) {
            ts.next();
            return std::make_unique<ReturnStatement>();
        }

        std::unique_ptr<Expression> expr = parseExpression();
        matchToken(TokenType::EndOfStatement, "\";\"");
        return std::make_unique<ReturnStatement>(std::move(expr));
    }

    if (next == TokenType::KeyIf) {
        ts.next();
        matchToken(TokenType::ParenthesisL, "opening parenthesis around if condition");
        std::unique_ptr<Expression> expr = parseExpression();
        matchToken(TokenType::ParenthesisR, "closing parenthesis around if condition");
        std::unique_ptr<Statement> statement = parseStatement();

        if (ts.peek().type == TokenType::KeyElse) {
            ts.next();
            std::unique_ptr<Statement> elseStatement = parseStatement();

            return std::make_unique<IfStatement>(std::move(expr), std::move(statement), std::move(elseStatement));
        }

        return  std::make_unique<IfStatement>(std::move(expr), std::move(statement));
    }

    if (next == TokenType::KeyWhile) {
        ts.next();
        matchToken(TokenType::ParenthesisL, "opening parenthesis around while condition");
        std::unique_ptr<Expression> expr = parseExpression();
        matchToken(TokenType::ParenthesisR, "closing parenthesis around while condition");
        std::unique_ptr<Statement> stmt = parseStatement();
        return std::make_unique<WhileStatement>(std::move(expr), std::move(stmt));
    }

    if (next == TokenType::BraceL) {
        return parseBlock();
    }

    std::unique_ptr<Expression> expr = parseExpression();
    matchToken(TokenType::EndOfStatement, "\";\"");
    return std::make_unique<ExprStatement>(std::move(expr));
}
