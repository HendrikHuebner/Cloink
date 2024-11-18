#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include "lexer.hpp"
#include <string>
#include <vector>
#include <optional>
#include <sstream>

inline std::string opToString(TokenType op) {
    switch (op) {
        case OpPlus: return "+";
        case OpMinus: return "-";
        case OpDivide: return "/";
        case OpModulo: return "%";
        case OpMultiply: return "*";
        case OpBitNot: return "~";
        case OpBitOr: return "|";
        case OpBitXor: return "^";
        case OpBitAnd: return "&";
        case OpAnd: return "&&";
        case OpEquals: return "==";
        case OpGreaterEq: return ">=";
        case OpLessEq: return "<=";
        case OpNot: return "!";
        case OpGreaterThan: return ">";
        case OpNotEquals: return "!=";
        case OpOr: return "||";
        case OpLessThan: return "<";
        case OpShiftLeft: return "<<";
        case OpAssign: return "=";
        case OpShiftRight: return ">>";
        default: return "";
    }
}

struct ASTNode {
    virtual std::string to_string() const = 0;
    virtual ~ASTNode() = default;
};

struct Expression : public ASTNode {};

struct Identifier : Expression {
    std::string name;

    Identifier(const std::string& name) : name(name) {}

    std::string to_string() const override {
        return "(Identifier " + name + ")";
    }
};

struct IntLiteral : Expression {
    uint64_t value;

    IntLiteral(uint64_t value) : value(value) {}

    std::string to_string() const override {
        return "(Number " + std::to_string(value) + ")";
    }
};

struct BinOp : Expression {
    TokenType op;
    std::unique_ptr<Expression> leftExpr;
    std::unique_ptr<Expression> rightExpr;

    BinOp(std::unique_ptr<Expression> left, std::unique_ptr<Expression> right, TokenType op) 
        : op(op), leftExpr(std::move(left)), rightExpr(std::move(right)) {}

    std::string to_string() const override {
        return "(BinOp " + opToString(op) + " " + leftExpr->to_string() + " " + rightExpr->to_string() + ")";
    }
};

struct UnOp : Expression {
    TokenType op;
    std::unique_ptr<Expression> expr;

    UnOp(std::unique_ptr<Expression> expr, TokenType op) 
        : op(op), expr(std::move(expr)) {}

    std::string to_string() const override {
        return "(UnOp " + opToString(op) + " " + expr->to_string() + ")";
    }
};

struct FunctionCall : Expression {
    Identifier ident;
    std::vector<std::unique_ptr<Expression>> paramList;

    FunctionCall(const Identifier& ident, std::vector<std::unique_ptr<Expression>> params)
        : ident(ident), paramList(std::move(params)) {}

    std::string to_string() const override {
        std::ostringstream ss;
        ss << "(FunctionCall " << ident.to_string();
        for (const auto& param : paramList) {
            ss << " " << param->to_string();
        }
        ss << ")";
        return ss.str();
    }
};

struct IndexExpr : Expression {
    std::unique_ptr<Expression> array;
    std::unique_ptr<Expression> idx;
    int sizeSpec;

    IndexExpr(std::unique_ptr<Expression> array, std::unique_ptr<Expression> idx, int sizeSpec = 8) 
        : array(std::move(array)), idx(std::move(idx)), sizeSpec(sizeSpec) {}

    std::string to_string() const override {
        return "(IndexExpr " + array->to_string() + " " + idx->to_string() + " " + std::to_string(sizeSpec) + ")";
    }
};

struct Statement : ASTNode {};

struct Declaration : Statement {
    bool isAuto;
    bool isRegister;
    Identifier ident;
    std::unique_ptr<Expression> expr;

    Declaration(bool isAuto, bool isRegister, const Identifier& ident, std::unique_ptr<Expression> expr)
        : isAuto(isAuto), isRegister(isRegister), ident(ident), expr(std::move(expr)) {}

    std::string to_string() const override {
        return "(Declaration " + ident.to_string() + " " + (expr ? expr->to_string() + ")": "()") + "\n";
    }
};

struct WhileStatement : Statement {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> statement;

    WhileStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> statement)
        : condition(std::move(condition)), statement(std::move(statement)) {}

    std::string to_string() const override {
        return "(While " + condition->to_string() + " " + statement->to_string() + ")\n";
    }
};

struct IfStatement : Statement {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> statement;
    std::optional<std::unique_ptr<Statement>> elseStatement;

    IfStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> statement)
        : condition(std::move(condition)), statement(std::move(statement)), elseStatement(std::nullopt) {}

    IfStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> statement, std::unique_ptr<Statement> elseStatement)
        : condition(std::move(condition)), statement(std::move(statement)), elseStatement(std::move(elseStatement)) {}

    std::string to_string() const override {
        std::string result = "(If " + condition->to_string() + " " + statement->to_string();
        if (elseStatement) {
            result += " (Else " + (*elseStatement)->to_string() + ")\n";
        }
        result += ")";
        return result;
    }
};

struct ExprStatement : Statement {
    std::unique_ptr<Expression> expr;

    ExprStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    std::string to_string() const override {
        return "(ExprStatement " + expr->to_string() + ")\n";
    }
};

struct ReturnStatement : Statement {
    std::optional<std::unique_ptr<Expression>> expr;

    ReturnStatement() : expr(std::nullopt) {}
    ReturnStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    std::string to_string() const override {
        return "(Return " + (expr ? expr.value()->to_string() : "()") + ")\n";
    }
};

struct Block : Statement {
    std::vector<std::unique_ptr<Statement>> statements;

    Block(std::vector<std::unique_ptr<Statement>> statements) : statements(std::move(statements)) {}

    std::string to_string() const override {
        std::string result = "(Block \n";
        for (const auto& statement : statements) {
            result += " " + statement->to_string();
        }
        result += ")";
        return result;
    }
};

struct Function {
    Identifier ident;
    std::vector<Identifier> params;
    std::unique_ptr<Block> block;

    Function(const Identifier& ident, const std::vector<Identifier>& params, std::unique_ptr<Block> block)
        : ident(ident), params(params), block(std::move(block)) {}

    std::string to_string() const {
        std::ostringstream ss;
        ss << "(Function " << ident.to_string() << " (Params";
        for (const auto& param : params) {
            ss << " " << param.to_string();
        }
        ss << ") " << block->to_string() << ")";
        return ss.str();
    }
};

class AbstractSyntaxTree {
    std::vector<std::unique_ptr<Function>> functions;

  public:
    AbstractSyntaxTree(std::vector<std::unique_ptr<Function>>& functions) : functions(std::move(functions)) {}

    std::string to_string() const {
        std::ostringstream ss;
        for (auto& func : functions) {
            ss << func->to_string() << "\n";
        }
        return ss.str();
    }
};
