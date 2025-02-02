#pragma once

#include <llvm/ADT/FunctionExtras.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "lexer.hpp"

namespace clonk {

template <typename T>
struct ScopedSymbol {
    unsigned scopeDepth;
    T value;
    std::string_view name;
    bool isRegister;
    bool isFunctionParam;

    ScopedSymbol(unsigned scopeDepth, T value, std::string_view ident, bool isRegister = false,
                 bool isFunctionParam = false)
        : scopeDepth(isFunctionParam
                         ? scopeDepth + 1
                         : scopeDepth),  // no shadowing of function parameters in top block
          value(value),
          name(ident),
          isRegister(isRegister),
          isFunctionParam(isFunctionParam) {}
};

template <typename T>
class SymbolTable {
    std::unordered_map<std::string_view, std::stack<ScopedSymbol<T>>> symbols;
    unsigned currentDepth = 0;

   public:
    std::optional<ScopedSymbol<T>> get(std::string_view name) {
        auto stack = symbols[name];
        if (stack.empty()) {
            return std::nullopt;
        }

        return stack.top();
    }

    std::string insert(std::string_view name, T value, bool isRegister, bool isFunctionParam) {
        auto& stack = symbols[name];

        if (!stack.empty()) {
            auto scope = stack.top();

            if (!scope.isRegister && scope.scopeDepth >= currentDepth) {
                return "";
            }
        }

        symbols[name].push(ScopedSymbol<T>(currentDepth, value, name, isRegister, isFunctionParam));
        return std::string(name);
    }

    void enterScope() { currentDepth++; }

    void leaveScope() {
        for (auto& e : symbols) {
            if (e.second.empty())
                continue;

            if (e.second.top().scopeDepth >= currentDepth) {
                e.second.pop();
            }
        }

        currentDepth--;
    }
};

struct ASTNode {
    virtual std::string to_string() const = 0;
    virtual ~ASTNode() = default;
};

struct Expression : public ASTNode {};

struct LValue : public Expression {};

static unsigned idIndex = 1;

struct Identifier : LValue {
    std::string name;
    const unsigned id;

    Identifier(const std::string& name) : name(name), id(idIndex++) {}

    std::string to_string() const override { return name; }

    bool operator==(const Identifier& other) { return other.id == this->id; }
};

struct IntLiteral : Expression {
    uint64_t value;

    IntLiteral(uint64_t value) : value(value) {}

    std::string to_string() const override { return std::to_string(value); }
};

struct BinOp : Expression {
    TokenType op;
    std::unique_ptr<Expression> leftExpr;
    std::unique_ptr<Expression> rightExpr;

    BinOp(std::unique_ptr<Expression> left, std::unique_ptr<Expression> right, TokenType op)
        : op(op), leftExpr(std::move(left)), rightExpr(std::move(right)) {}

    std::string to_string() const override {
        return "(" + opToString(op) + " " + leftExpr->to_string() + " " + rightExpr->to_string() +
               ")";
    }
};

struct UnOp : Expression {
    TokenType op;
    std::unique_ptr<Expression> expr;

    UnOp(std::unique_ptr<Expression> expr, TokenType op) : op(op), expr(std::move(expr)) {}

    std::string to_string() const override {
        return "(" + opToString(op) + " " + expr->to_string() + ")";
    }
};

struct FunctionCall : Expression {
    std::unique_ptr<Identifier> ident;
    std::vector<std::unique_ptr<Expression>> paramList;

    FunctionCall(std::unique_ptr<Identifier> ident, std::vector<std::unique_ptr<Expression>> params)
        : ident(std::move(ident)), paramList(std::move(params)) {}

    std::string to_string() const override {
        std::ostringstream ss;
        ss << "(function call " << ident->to_string();
        for (const auto& param : paramList) {
            ss << " " << param->to_string();
        }
        ss << ")";
        return ss.str();
    }
};

struct IndexExpr : LValue {
    std::unique_ptr<Expression> array;
    std::unique_ptr<Expression> idx;
    int sizeSpec;

    IndexExpr(std::unique_ptr<Expression> array, std::unique_ptr<Expression> idx, int sizeSpec = 8)
        : array(std::move(array)), idx(std::move(idx)), sizeSpec(sizeSpec) {}

    std::string to_string() const override {
        return "([] " + array->to_string() + " " + idx->to_string() + "@" +
               std::to_string(sizeSpec) + ")";
    }
};

struct Statement : ASTNode {};

struct Declaration : Statement {
    bool isAuto;
    bool isRegister;
    std::unique_ptr<Identifier> ident;
    std::unique_ptr<Expression> expr;

    Declaration(bool isAuto, bool isRegister, std::unique_ptr<Identifier> ident,
                std::unique_ptr<Expression> expr)
        : isAuto(isAuto), isRegister(isRegister), ident(std::move(ident)), expr(std::move(expr)) {}

    std::string to_string() const override {
        return "(decl " + ident->to_string() + " " + (expr ? expr->to_string() + ")" : "()") + "\n";
    }
};

struct WhileStatement : Statement {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> statement;

    WhileStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> statement)
        : condition(std::move(condition)), statement(std::move(statement)) {}

    std::string to_string() const override {
        return "(while " + condition->to_string() + " " + statement->to_string() + ")\n";
    }
};

struct IfStatement : Statement {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> statement;
    std::optional<std::unique_ptr<Statement>> elseStatement;

    IfStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> statement)
        : condition(std::move(condition)),
          statement(std::move(statement)),
          elseStatement(std::nullopt) {}

    IfStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> statement,
                std::unique_ptr<Statement> elseStatement)
        : condition(std::move(condition)),
          statement(std::move(statement)),
          elseStatement(std::move(elseStatement)) {}

    std::string to_string() const override {
        std::string result = "(if " + condition->to_string() + " " + statement->to_string();
        if (elseStatement) {
            result += " (else " + (*elseStatement)->to_string() + ")\n";
        }
        result += ")";
        return result;
    }
};

struct ExprStatement : Statement {
    std::unique_ptr<Expression> expr;

    ExprStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    std::string to_string() const override {
        return "(expr statement " + expr->to_string() + ")\n";
    }
};

struct ReturnStatement : Statement {
    std::optional<std::unique_ptr<Expression>> expr;

    ReturnStatement() : expr(std::nullopt) {}

    ReturnStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    std::string to_string() const override {
        return "(return " + (expr ? expr.value()->to_string() : "()") + ")\n";
    }
};

struct Block : Statement {
    const std::vector<std::unique_ptr<Statement>> statements;

    Block(std::vector<std::unique_ptr<Statement>> statements) : statements(std::move(statements)) {}

    std::string to_string() const override {
        std::string result = "(block \n";
        for (const auto& statement : statements) {
            result += " " + statement->to_string();
        }
        result += ")";
        return result;
    }
};

struct Function {
    const std::unique_ptr<Identifier> ident;
    const std::vector<std::unique_ptr<Identifier>> params;
    const std::unique_ptr<Block> block;
    std::vector<std::string_view> autoDecls;

    Function(std::unique_ptr<Identifier> ident, std::vector<std::unique_ptr<Identifier>> params,
             std::unique_ptr<Block> block, std::vector<std::string_view> autoDecls)
        : ident(std::move(ident)), params(std::move(params)), block(std::move(block)), autoDecls(autoDecls) {}

    std::string to_string() const {
        std::ostringstream ss;
        ss << "(function " << ident->to_string() << " (params";
        for (const auto& param : params) {
            ss << " " << param->to_string();
        }
        ss << ") " << block->to_string() << ")";
        return ss.str();
    }
};

class Parser;

class AbstractSyntaxTree {
    std::vector<std::unique_ptr<Function>> functions;
    std::vector<std::pair<std::string, int>> externFunctions;  // name, paramcount

    friend Parser;

    void addFunction(std::unique_ptr<Function> function) {
        functions.push_back(std::move(function));
    }

    void addExternFunction(std::string name, int paramCount) {
        externFunctions.push_back({name, paramCount});
    }

   public:
    std::string to_string() const {
        std::ostringstream ss;
        for (auto& func : functions) {
            ss << func->to_string() << "\n";
        }
        return ss.str();
    }

    const std::vector<std::unique_ptr<Function>>& getFunctions() const { return functions; }

    const std::vector<std::pair<std::string, int>>& getExternFunctions() const {
        return externFunctions;
    }
};

}  // end namespace clonk

template <>
struct std::hash<clonk::Identifier> {
    std::size_t operator()(const clonk::Identifier& ident) const noexcept { return ident.id; }
};
