#pragma once

#include <cassert>
#include <cctype>
#include <cstdint>
#include <string>
#include <optional>

enum TokenType {
    IdentifierType,
    NumberLiteral,
    EndOfFile,
    Unknown,

    // Operators
    OpPlus,
    OpMinus,
    OpMultiply,
    OpDivide,
    OpModulo,
    OpBitNot,
    OpBitAnd,
    OpBitOr,
    OpBitXor,
    OpShiftLeft,
    OpShiftRight,
    OpNot,
    OpOr,
    OpAnd,
    OpGreaterThan,
    OpLessThan,
    OpGreaterEq,
    OpLessEq,
    OpEquals,
    OpNotEquals,
    OpAssign,

    // keywords
    KeyNumber,
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

struct Token {
    TokenType type;

    union {
        std::string identifier;
        uint16_t value;
    };

    Token(const Token& token) : type(token.type) {
        if (type == TokenType::IdentifierType) {
            this->identifier = token.identifier;
        } else if (type == TokenType::NumberLiteral) {
            this->value = token.value;
        }
    };

    void operator=(const Token& other) {
        if (type == TokenType::IdentifierType) {
            this->identifier = other.identifier;
        } else if (type == TokenType::NumberLiteral) {
            this->value = other.value;
        }
    }

    ~Token() {}

    Token(TokenType type) : type(type) {}

    Token(TokenType type, std::string& identifier) : type(type), identifier(identifier) {
        assert(type == TokenType::IdentifierType);
    }

    Token(TokenType type, uint64_t value) : type(type), value(value) {
        assert(type == TokenType::NumberLiteral);
    }
};

class TokenStream {
    const std::string input;
    size_t position;
    size_t line;
    size_t lineStart;
    std::optional<Token> top;
    
   public:
    explicit TokenStream(const std::string& input) : input(input), position(0) {}

    [[maybe_unused]] Token next();

    Token peek();

    bool empty();

    std::string getCurrentLine() const { return ""; }

    size_t getCurrentLineNumber() const { return line; }

    size_t getLinePosition() const { return position - lineStart; }

   private:
    char moveToNextToken();

    Token lexOperator();
    Token lexWord();
    Token lexNumber();
    Token lexPunctuationChar();
};
