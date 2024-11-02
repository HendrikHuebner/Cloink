#include <cctype>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include "lexer.hpp"

static const std::unordered_map<std::string, TokenType> keywords = {
    {"number", TokenType::KeyNumber},     {"auto", TokenType::KeyAuto},
    {"register", TokenType::KeyRegister}, {"if", TokenType::KeyIf},
    {"else", TokenType::KeyElse},         {"while", TokenType::KeyWhile},
    {"return", TokenType::KeyReturn},
};

static inline bool isBaseChar(char c) {
    return std::isalnum(c) || c == '_';
}

static inline bool isOperator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '=' || c == '%' || c == '!' ||
           c == '^' || c == '~' || c == '&' || c == '|' || c == '<' || c == '>';
}

static inline bool isPunctuationChar(char c) {
    return c == ';' || c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' ||
           c == '@';
}

Token TokenStream::next() {
    if (top.has_value()) {
        top = std::nullopt;
        return top.value();
    }

    char c = moveToNextToken();

    if (position >= input.size()) {
        return {TokenType::EndOfFile};
    }

    if (std::isalpha(c) || c == '_') {
        return lexWord();

    } else if (std::isdigit(c)) {
        return lexNumber();

    } else if (isOperator(c)) {
        return lexOperator();

    } else if (isPunctuationChar(c)) {
        return lexPunctuationChar();
    }

    return {TokenType::Unknown};
}

Token TokenStream::peek() {
    if (top.has_value()) {
        top = std::nullopt;
        return top.value();
    }

    Token next = this->next();
    this->top = next;
    return next;
}

char TokenStream::moveToNextToken() {
    char c;

    while (position < input.size()) {
        char c = input[position];

        if (c == '/' && position + 1 < input.size()) {
            if (input[++position] == '/') {

                // skip comment
                while (position < input.size() && input[position] != '\n') {
                    position++;
                }

                position++;
                lineStart = position;
                line++;

                continue;
            }
        }

        // check whitespace
        if (std::isspace(c)) {
            if (c == '\n') {
                line++;
                lineStart = position;
            }

            position++;

        } else {
            return c;
        }
    }

    return 0;
}

Token TokenStream::lexOperator() {
    char c = input[position];
    char c2 = (position + 1 >= input.size()) ? 0 : input[position + 1];

    std::optional<TokenType> op = std::nullopt;

    switch (c) {
        case '+':
            op = TokenType::OpPlus;
            break;
        case '-':
            op = TokenType::OpMinus;
            break;
        case '*':
            op = TokenType::OpMultiply;
            break;
        case '/':
            op = TokenType::OpDivide;
            break;
        case '%':
            op = TokenType::OpModulo;
            break;
        case '^':
            op = TokenType::OpBitXor;
            break;
        case '~':
            op = TokenType::OpBitNot;
            break;
        case '=': {
            if (c2 == '=') {
                op = TokenType::OpEquals;
                position++;
            } else {
                op = TokenType::OpAssign;
            }
            break;
        }
        case '!': {
            if (c2 == '=') {
                op = TokenType::OpNotEquals;
                position++;
            } else {
                op = TokenType::OpNot;
            }
            break;
        }
        case '<': {
            if (c2 == '=') {
                op = TokenType::OpLessEq;
                position++;
            } else if (c2 == '<') {
                op = TokenType::OpShiftLeft;
                position++;
            } else {
                op = TokenType::OpLessThan;
            }
            break;
        }
        case '>': {
            if (c2 == '=') {
                op = TokenType::OpGreaterEq;
                position++;
            } else if (c2 == '>') {
                op = TokenType::OpShiftRight;
                position++;
            } else {
                op = TokenType::OpGreaterThan;
            }
            break;
        }
        case '&': {
            if (c2 == '&') {
                op = TokenType::OpAnd;
                position++;
            } else {
                op = TokenType::OpBitAnd;
            }
            break;
        }
        case '|': {
            if (c2 == '|') {
                op = TokenType::OpOr;
                position++;
            } else {
                op = TokenType::OpBitOr;
            }
            break;
        }
    }

    if (!op.has_value()) {
        return {TokenType::Unknown};

    } else {
        return {op.value()};
    }
}

Token TokenStream::lexWord() {
    size_t start = position;

    while (position < input.size() && isBaseChar(input[position])) {
        position++;
    }

    std::string lexeme = input.substr(start, position - start);

    if (keywords.find(lexeme) != keywords.end()) {
        return {keywords.at(lexeme)};
    }

    return {TokenType::Identifier, lexeme};
}

Token TokenStream::lexNumber() {
    size_t start = position;
    while (position < input.size() && std::isdigit(input[position])) {
        position++;
    }

    return {TokenType::NumberLiteral, (uint64_t)std::atoll(input.substr(start, position).c_str())};
}

Token TokenStream::lexPunctuationChar() {
    switch (input[position]) {
        case ';':
            return TokenType::EndOfStatement;
        case '(':
            return TokenType::ParenthesisL;
        case ')':
            return TokenType::ParenthesisR;
        case '[':
            return TokenType::BracketL;
        case ']':
            return TokenType::BracketR;
        case '{':
            return TokenType::BraceL;
        case '}':
            return TokenType::BraceR;
        case '@':
            return TokenType::SizeSpec;
        default:
            throw std::runtime_error("Invalid punctuation character!");
    }

    position++;
}
