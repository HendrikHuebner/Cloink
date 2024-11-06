#pragma once

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include "lexer.hpp"
#include "debug.hpp"
#include "diagnostics.hpp"
#include "ast.hpp"

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
    void parsingError() {

    }

    void matchToken(TokenType type) {
        Token token = ts.next();

        if (token.type != type) {
            log("Did not match token types: %d %d \n", type, token.type);
            dm.parsingError(token);
        }
    }

    Identifier matchIdentifier() {
        Token token = ts.next();

        if (token.type != TokenType::IdentifierType) {
            log("Did not match Identifier: %d %d \n", token.type);
            dm.parsingError(token);
            exit(EXIT_FAILURE);
        }

        return Identifier(token.identifier);
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

    std::unique_ptr<Expression> parseExpression() {
        TokenType next = ts.next().type;

        switch (next) {
            case TokenType::ParenthesisL: {
                ts.next();
                std::unique_ptr<Expression> expr = parseExpression();
                matchToken(TokenType::ParenthesisR);
                return expr;
            }

            case TokenType::OpNot: 
            case TokenType::OpMinus: 
            case TokenType::OpBitNot: 
            case TokenType::OpBitAnd: {
                TokenType op = ts.next().type;
                std::unique_ptr<Expression> expr = parseExpression();
                return std::make_unique<UnOp>(std::move(expr), op);
            }

            case TokenType::NumberLiteral: {
                return std::make_unique<IntLiteral>(ts.next().value);
            }

            case TokenType::IdentifierType: {
                Identifier ident = matchIdentifier();

                if (ts.peek().type == TokenType::ParenthesisL) {
                    // function call
                    ts.next();

                    std::vector<std::unique_ptr<Expression>> params;
                    if (ts.peek().type != TokenType::ParenthesisR) {
                        params.push_back(parseExpression());
                    }

                    while (ts.peek().type == TokenType::Comma) {
                        ts.next();
                        params.push_back(parseExpression());
                    }

                    matchToken(TokenType::ParenthesisR);
                    return std::make_unique<FunctionCall>(ident, std::move(params));
                }
            }

            default: {
                std::unique_ptr<Expression> left = parseExpression();
                Token op = ts.next();

                if (op.type == TokenType::BracketL) {
                    std::unique_ptr<Expression> idxExpr = parseExpression();

                    if (ts.peek().type == TokenType::SizeSpec) {
                        ts.next();
                        Token sizeSpec = ts.next();

                        if (sizeSpec.type != TokenType::NumberLiteral) {
                            log("Invalid sizespec: \n", sizeSpec.type);
                            dm.parsingError(sizeSpec);
                            exit(EXIT_FAILURE);

                        } else if (sizeSpec.value != 1 && sizeSpec.value != 2 && sizeSpec.value != 4 && sizeSpec.value != 8) {
                       
                            log("Invalid sizespec, must be 1, 2, 4 or 8: \n", sizeSpec.value);
                            dm.parsingError(sizeSpec);
                            exit(EXIT_FAILURE);
                        }

                        return std::make_unique<IndexExpr>(std::move(left), std::move(idxExpr), sizeSpec.value);
                    
                    } else {
                        return std::make_unique<IndexExpr>(std::move(left), std::move(idxExpr));
                    }
                }

                // binop
                int precedence = getBinOpPrecedence(op.type);

                if (precedence < 0) {
                    log("Expected BinOp token: %d %d \n", op.type);
                    dm.parsingError(op);
                    exit(EXIT_FAILURE);
                }

                std::unique_ptr<Expression> right = parseExpression();

                BinOp* rightBinop;
                if ((rightBinop = dynamic_cast<BinOp*>(right.get()))) {
                    int rightPrecedence = getBinOpPrecedence(rightBinop->op);

                    if (rightPrecedence < precedence) {
                        return std::make_unique<BinOp>(std::move(left), 
                            std::make_unique<BinOp>(std::move(rightBinop->leftExpr), std::move(rightBinop->rightExpr), op.type), rightBinop->op);
                    }
                }

                return std::make_unique<BinOp>(std::move(left), std::move(right), op.type);
            }
        }
    }

    std::unique_ptr<Block> parseBlock() {
        matchToken(TokenType::BraceL);
        
        std::vector<std::unique_ptr<Statement>> statements;

        while(ts.peek().type != TokenType::BraceR) {
            statements.push_back(parseStatement());
        }

        matchToken(TokenType::BraceR);
        return std::make_unique<Block>(std::move(statements));
    }

    std::vector<Identifier> parseParamlist() {
        std::vector<Identifier> params;

        if (ts.peek().type == TokenType::IdentifierType) {
            params.push_back(matchIdentifier());
        }

        while (ts.peek().type == TokenType::Comma) {
            matchToken(TokenType::Comma);
            params.push_back(matchIdentifier());
        }

        return params;
    }

    std::unique_ptr<Function> parseFunction() {
        Identifier ident = matchIdentifier();
        matchToken(TokenType::ParenthesisL);
        std::vector<Identifier> params = parseParamlist();
        matchToken(TokenType::ParenthesisR);
        std::unique_ptr<Block> block = parseBlock();
        return std::make_unique<Function>(ident, std::move(params), std::move(block));
    }

    std::unique_ptr<Statement> parseDeclStatement() {
        TokenType next = ts.peek().type;
        if (next == TokenType::KeyAuto | next == TokenType::KeyRegister) {
            TokenType type = ts.next().type;
            Identifier ident = matchIdentifier();
            matchToken(TokenType::OpAssign);
            std::unique_ptr<Expression> expr = parseExpression();
            matchToken(TokenType::EndOfStatement);

            return std::make_unique<Declaration>( 
                type == TokenType::KeyAuto,
                type == TokenType::KeyRegister, ident, std::move(expr));

        } else {
            return parseStatement();
        }
    }

    std::unique_ptr<Statement> parseStatement() {
        TokenType next = ts.peek().type;
        if (next == TokenType::KeyReturn) {
            ts.next();
            if (ts.peek().type == TokenType::EndOfStatement) {
                ts.next();
                return std::make_unique<ReturnStatement>();
            }

            std::unique_ptr<Expression> expr = parseExpression();
            matchToken(TokenType::EndOfStatement);
            return std::make_unique<ReturnStatement>(std::move(expr));
        }

        if (next == TokenType::KeyIf) {
            ts.next();
            matchToken(TokenType::ParenthesisL);
            std::unique_ptr<Expression> expr = parseExpression();
            matchToken(TokenType::ParenthesisR);
            std::unique_ptr<Statement> statement = parseStatement();

            if (ts.peek().type == TokenType::KeyElse) {
                ts.next();
                std::unique_ptr<Statement> elseStatement = parseStatement();
                matchToken(TokenType::EndOfStatement);

                return std::make_unique<IfStatement>(std::move(expr), std::move(statement), std::move(elseStatement));
            }

            matchToken(TokenType::EndOfStatement);
            return  std::make_unique<IfStatement>(std::move(expr), std::move(statement));
        }

        if (next == TokenType::KeyWhile) {
            ts.next();
            matchToken(TokenType::ParenthesisL);
            std::unique_ptr<Expression> expr = parseExpression();
            matchToken(TokenType::ParenthesisR);
            std::unique_ptr<Block> block = parseBlock();
        }

        if (next == TokenType::BraceR) {
            return parseBlock();
        }

        std::unique_ptr<Expression> expr = parseExpression();
        matchToken(TokenType::EndOfStatement);
        return std::make_unique<ExprStatement>(std::move(expr));
    }
};
