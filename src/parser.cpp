
#include "parser.hpp"
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include "ast.hpp"
#include "debug.hpp"
#include "diagnostics.hpp"
#include "lexer.hpp"

using namespace clonk;

void Parser::matchToken(TokenType type, const std::string& expected) {
    Token token = ts.next();

    if (token.type != type) {
        DiagnosticsManager::get().unexpectedToken(ts, token, expected);
    }
}

std::unique_ptr<Identifier> Parser::parseIdentifier() {
    Token token = ts.next();

    if (token.type != TokenType::IdentifierType) {
        DiagnosticsManager::get().unexpectedToken(ts, token);
        exit(EXIT_FAILURE);
    }

    return std::make_unique<Identifier>(std::string(token.getIdentifier()));
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
        case TokenType::OpAmp: return 7;
        case TokenType::OpXor: return 6;
        case TokenType::OpOr: return 5;
        case TokenType::OpLogicalAnd: return 4;
        case TokenType::OpLogicalOr: return 3;
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
        case TokenType::OpAmp: {
            ts.next();
            Token token = ts.peek();
            expr = parseValue(true);

            if (!dynamic_cast<IndexExpr*>(expr.get())) {
                auto scopedIdent = scopes.get(token.getIdentifier());

                if (scopedIdent->isRegister) {
                    DiagnosticsManager::get().error(ts, "cannot reference register type \"" +
                                                            std::string(token.getIdentifier()) +
                                                            "\"");
                } else if (scopedIdent->isFunctionParam) {
                    DiagnosticsManager::get().error(ts, "cannot reference function parameter \"" +
                                                            std::string(token.getIdentifier()) +
                                                            "\"");
                }
            }

            return std::make_unique<UnOp>(std::move(expr), TokenType::OpAmp);
        }
        case TokenType::OpNot:
        case TokenType::OpMinus:
        case TokenType::OpBitNot: {
            TokenType op = ts.next().type;
            expr = parseTerm();
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
            DiagnosticsManager::get().unexpectedToken(ts, ts.peek());
            exit(EXIT_FAILURE);
        }
    }
}

std::unique_ptr<Expression> Parser::parseExpression() {
    std::unique_ptr<Expression> expr = parseTerm();

    while (true) {
        Token op = ts.peek();

        int precedence = getBinOpPrecedence(op.type);

        if (precedence == -1) {
            if (op.type == TokenType::OpAssign && !dynamic_cast<LValue*>(expr.get())) {
                DiagnosticsManager::get().unexpectedToken(ts, op,
                                                          "cannot assign to rvalue expression");
            }

            return expr;
        }

        // expr is a BinOp
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
            int leftPrecedence = getBinOpPrecedence(leftBinop->op);

            if (precedence > leftPrecedence ||
                (precedence == leftPrecedence && op.type == TokenType::OpAssign)) {
                if (op.type == TokenType::OpAssign &&
                    !dynamic_cast<LValue*>(leftBinop->leftExpr.get())) {
                    DiagnosticsManager::get().unexpectedToken(ts, op,
                                                              "cannot assign to rvalue expression");
                }

                expr =
                    std::make_unique<BinOp>(std::move(leftBinop->leftExpr),
                                            std::make_unique<BinOp>(std::move(leftBinop->rightExpr),
                                                                    std::move(right), op.type),
                                            leftBinop->op);
                continue;

            } else if (op.type == TokenType::OpAssign) {
                DiagnosticsManager::get().unexpectedToken(ts, op,
                                                          "cannot assign to rvalue expression");
            }

        } else {
            if (op.type == TokenType::OpAssign && !dynamic_cast<LValue*>(expr.get())) {
                DiagnosticsManager::get().unexpectedToken(ts, op,
                                                          "cannot assign to rvalue expression");
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

    while (ts.peek().type != TokenType::BraceR) {
        statements.push_back(parseDeclStatement());
    }

    matchToken(TokenType::BraceR, "closing brace in block");

    scopes.leaveScope();
    return std::make_unique<Block>(std::move(statements));
}

std::vector<std::unique_ptr<Identifier>> Parser::parseParamlist() {
    std::vector<std::unique_ptr<Identifier>> params;

    if (ts.peek().type == TokenType::IdentifierType) {
        std::unique_ptr<Identifier> ident = parseIdentifier();
        scopes.insert(ident->name, ident->name, false, true);
        params.push_back(std::move(ident));
    }

    while (ts.peek().type == TokenType::Comma) {
        matchToken(TokenType::Comma, "parameter seperator comma");
        std::unique_ptr<Identifier> ident = parseIdentifier();
        std::string newName = scopes.insert(ident->name, ident->name, false, true);

        if (newName.empty()) {
            DiagnosticsManager::get().error(
                ts, "duplicate function parameter: \"" + ident->name + "\"");
        }

        ident->name = newName;
        params.push_back(std::move(ident));
    }

    return params;
}

std::unique_ptr<Function> Parser::parseFunction() {
    std::unique_ptr<Identifier> ident = parseIdentifier();
    declaredFunctions.insert(ident->name);

    matchToken(TokenType::ParenthesisL, "parameter list opening parenthesis");
    std::vector<std::unique_ptr<Identifier>> params = parseParamlist();
    matchToken(TokenType::ParenthesisR, "parameter list closing parenthesis");
    std::unique_ptr<Block> block = parseBlock();

    checkFunctionParamCounts(ident->name, params.size());
    auto retval = std::make_unique<Function>(std::move(ident), std::move(params), std::move(block));
    retval->autoDecls = scopes.collectAutoDecls();
    return retval;
}

void Parser::checkFunctionParamCounts(const std::string& name, size_t paramCount) {
    if (paramCounts.find(name) == paramCounts.end()) {
        paramCounts[name] = paramCount;

    } else if (paramCounts[name] != paramCount) {
        DiagnosticsManager::get().error(
            ts, "function \"" + name + "\" called with missmatching number of parameters: " +
                    std::to_string(paramCount) + " previously called with " +
                    std::to_string(paramCounts[name]) + " parameters");
    }
}

std::unique_ptr<Expression> Parser::parseValue(bool lvalue) {
    std::unique_ptr<Identifier> ident = parseIdentifier();
    std::unique_ptr<Expression> value;

    TokenType type = ts.peek().type;

    if (type == TokenType::ParenthesisL) {
        auto params = parseFunctionCallParamList();
        checkFunctionParamCounts(ident->name, params.size());

        value = std::make_unique<FunctionCall>(std::move(ident), std::move(params));

        if (lvalue && ts.peek().type != TokenType::BracketL) {
            DiagnosticsManager::get().error(ts, "expected lvalue");
            exit(EXIT_FAILURE);
        }

    } else {
        if (!scopes.get(ident->name)) {
            DiagnosticsManager::get().error(ts, "unknown identifier: \"" + ident->name + "\"");
        }

        value = std::move(ident);
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

            } else if (sizeSpec.getValue() != 1 && sizeSpec.getValue() != 2 &&
                       sizeSpec.getValue() != 4 && sizeSpec.getValue() != 8) {
                DiagnosticsManager::get().unexpectedToken(
                    ts, sizeSpec,
                    "Invalid sizespec, must be 1, 2, 4 or 8, was " +
                        std::to_string(sizeSpec.getValue()));
                exit(EXIT_FAILURE);
            }

            matchToken(TokenType::BracketR, "closing bracket of indexing operation");
            value = std::make_unique<IndexExpr>(std::unique_ptr<Expression>(std::move(value)),
                                                std::move(idxExpr), sizeSpec.getValue());

        } else {
            matchToken(TokenType::BracketR, "closing bracket of indexing operation");
            value = std::make_unique<IndexExpr>(std::unique_ptr<Expression>(std::move(value)),
                                                std::move(idxExpr));
        }
    }

    return std::unique_ptr<Expression>(std::move(value));
}

std::unique_ptr<Statement> Parser::parseDeclStatement() {
    TokenType type = ts.peek().type;
    if (type == TokenType::KeyAuto || type == TokenType::KeyRegister) {
        ts.next();
        std::unique_ptr<Identifier> ident = parseIdentifier();

        matchToken(TokenType::OpAssign, "assignment operator in declaration");
        std::unique_ptr<Expression> expr = parseExpression();
        matchToken(TokenType::EndOfStatement, "\";\"");

        std::string newName =
            scopes.insert(ident->name, ident->name, type == TokenType::KeyRegister, false);
        if (newName.empty()) {
            DiagnosticsManager::get().error(ts, "redeclared identifier \"" + ident->name + "\"");
        }

        ident->name = newName;
        return std::make_unique<Declaration>(type == TokenType::KeyAuto,
                                             type == TokenType::KeyRegister, std::move(ident),
                                             std::move(expr));

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

            return std::make_unique<IfStatement>(std::move(expr), std::move(statement),
                                                 std::move(elseStatement));
        }

        return std::make_unique<IfStatement>(std::move(expr), std::move(statement));
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
