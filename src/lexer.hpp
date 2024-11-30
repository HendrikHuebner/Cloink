#pragma once

#include <cassert>
#include <cctype>
#include <cstdint>
#include <string>
#include <optional>
#include <variant>

namespace clonk {

enum TokenType {
    IdentifierType,
    NumberLiteral,
    EndOfFile,

    // Operators
    OpPlus,
    OpMinus,
    OpMultiply,
    OpDivide,
    OpModulo,
    OpBitNot,
    OpAmp,
    OpOr,
    OpXor,
    OpShiftLeft,
    OpShiftRight,
    OpNot,
    OpLogicalOr,
    OpLogicalAnd,
    OpGreaterThan,
    OpLessThan,
    OpGreaterEq,
    OpLessEq,
    OpEquals,
    OpNotEquals,
    OpAssign,

    // keywords
    KeyAuto,
    KeyRegister,
    KeyIf,
    KeyElse,
    KeyWhile,
    KeyReturn,

    // Punctuation characters
    ParenthesisR,
    ParenthesisL,
    BraceR,
    BraceL,
    BracketR,
    BracketL,
    SizeSpec,
    Comma,
    EndOfStatement,
};

std::string opToString(TokenType op);

struct Token {
    TokenType type;
    std::variant<std::monostate, std::string_view, uint64_t> data;

    Token(TokenType type, const std::string_view& identifier)
        : type(type), data(identifier) {
        assert(type == TokenType::IdentifierType);
    }

    Token(TokenType type, uint64_t value)
        : type(type), data(value) {
        assert(type == TokenType::NumberLiteral);
    }

    Token(TokenType type)
        : type(type), data(std::monostate()) {}

    Token(const Token& other) : type(other.type), data(other.data) {}

    Token& operator=(const Token& other) {
        if (this != &other) {
            type = other.type;
            data = other.data;
        }
        return *this;
    }

    ~Token() = default;

    const std::string_view& getIdentifier() const {
        assert(type == TokenType::IdentifierType);
        return std::get<std::string_view>(data);
    }

    uint64_t getValue() const {
        assert(type == TokenType::NumberLiteral);
        return std::get<uint64_t>(data);
    }

    std::string to_string() const;
};

class TokenStream {
    std::string_view input;
    size_t position = 0;
    size_t line = 1;
    size_t lineStart = 0;
    std::optional<Token> top = std::nullopt;
    
   public:
    explicit TokenStream(std::string_view input) : input(input), position(0) {}

    [[maybe_unused]] Token next();

    Token peek();

    bool empty();

    std::string_view getCurrentLine() const { 
        return input.substr(lineStart, std::max(input.size() - 1, position + 1)); 
    }

    size_t getCurrentLineNumber() const { return line; }

    size_t getLinePosition() const { return position - lineStart - 1; }

   private:
    char moveToNextToken();

    Token lexOperator();
    Token lexWord();
    Token lexNumber();
    Token lexPunctuationChar();
};

} // end namespace clonk
