
#include <cstdlib>
#include "lexer.hpp"
#include "parser.hpp"
#include "diagnostics.hpp"
#include "debug.hpp"

void Parser::matchToken(TokenType type) {
    Token token = ts.next();

    if (token.type != type) {
        //debug("Did not match token types: %d %d \n", type, token.type);
        diagnostics.parsingError(ts, token);
    }
}

Identifier Parser::matchIdentifier() {
    Token token = ts.next();

    if (token.type != TokenType::IdentifierType) {
        //debug("Did not match Identifier: %d %d \n", token.type);
        diagnostics.parsingError(ts, token);
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

std::unique_ptr<Expression> Parser::parseExpression() {
    TokenType next = ts.peek().type;
    std::unique_ptr<Expression> expr;

    switch (next) {
        case TokenType::ParenthesisL: {
            ts.next();
            expr = parseExpression();
            matchToken(TokenType::ParenthesisR);
            break;
        }

        case TokenType::OpNot: 
        case TokenType::OpMinus: 
        case TokenType::OpBitNot: 
        case TokenType::OpBitAnd: {
            TokenType op = ts.next().type;
            expr = parseExpression();
            expr = std::make_unique<UnOp>(std::move(expr), op);
            break;
        }

        case TokenType::NumberLiteral: {
            Token num = ts.next();
            expr = std::make_unique<IntLiteral>(num.getValue());
            break;
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
                expr = std::make_unique<FunctionCall>(ident, std::move(params));
                break;

            } else {
                expr = std::make_unique<Identifier>(ident);
                break;
            }
        }

        default: {
            ts.next();
            debug("Unexpected Token: \n", next);
            diagnostics.parsingError(ts, ts.peek());
            exit(EXIT_FAILURE);
        }
    }

    Token op = ts.peek();

    // check indexing expr
    if (op.type == TokenType::BracketL) {
        ts.next();

        std::unique_ptr<Expression> idxExpr = parseExpression();

        if (ts.peek().type == TokenType::SizeSpec) {
            ts.next();
            Token sizeSpec = ts.next();

            if (sizeSpec.type != TokenType::NumberLiteral) {
                debug("Invalid sizespec: \n", sizeSpec.type);
                diagnostics.parsingError(ts, sizeSpec);
                exit(EXIT_FAILURE);

            } else if (sizeSpec.getValue() != 1 && sizeSpec.getValue() != 2 && sizeSpec.getValue() != 4 && sizeSpec.getValue() != 8) {
            
                debug("Invalid sizespec, must be 1, 2, 4 or 8: \n", sizeSpec.getValue());
                diagnostics.parsingError(ts, sizeSpec);
                exit(EXIT_FAILURE);
            }

            return std::make_unique<IndexExpr>(std::move(expr), std::move(idxExpr), sizeSpec.getValue());
        
        } else {

            return std::make_unique<IndexExpr>(std::move(expr), std::move(idxExpr));
        }
    }

    // binop
    int precedence = getBinOpPrecedence(op.type);

    if (precedence == -1)
        return expr;

    if (precedence < 0) {
        debug("Expected BinOp token: %d %d \n", op.type);
        diagnostics.parsingError(ts, op);
        exit(EXIT_FAILURE);
    }
    
    ts.next();
    std::unique_ptr<Expression> right = parseExpression();

    BinOp* rightBinop;
    if ((rightBinop = dynamic_cast<BinOp*>(right.get()))) {
        int rightPrecedence = getBinOpPrecedence(rightBinop->op);

        if (rightPrecedence < precedence) {
            return std::make_unique<BinOp>(std::move(expr), 
                std::make_unique<BinOp>(std::move(rightBinop->leftExpr), std::move(rightBinop->rightExpr), op.type), rightBinop->op);
        }
    }

    return std::make_unique<BinOp>(std::move(expr), std::move(right), op.type);

}

std::unique_ptr<Block> Parser::parseBlock() {
    matchToken(TokenType::BraceL);
    
    std::vector<std::unique_ptr<Statement>> statements;

    while(ts.peek().type != TokenType::BraceR) {
        statements.push_back(parseDeclStatement());
    }

    matchToken(TokenType::BraceR);
    return std::make_unique<Block>(std::move(statements));
}

std::vector<Identifier> Parser::parseParamlist() {
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

std::unique_ptr<Function> Parser::parseFunction() {
    Identifier ident = matchIdentifier();
    matchToken(TokenType::ParenthesisL);
    std::vector<Identifier> params = parseParamlist();
    matchToken(TokenType::ParenthesisR);
    std::unique_ptr<Block> block = parseBlock();
    return std::make_unique<Function>(ident, std::move(params), std::move(block));
}

std::unique_ptr<Statement> Parser::parseDeclStatement() {
    TokenType type = ts.peek().type;
    if (type == TokenType::KeyAuto || type == TokenType::KeyRegister) {
        ts.next();
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

std::unique_ptr<Statement> Parser::parseStatement() {
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

    if (next == TokenType::BraceL) {
        return parseBlock();
    }

    std::unique_ptr<Expression> expr = parseExpression();
    matchToken(TokenType::EndOfStatement);
    return std::make_unique<ExprStatement>(std::move(expr));
}
