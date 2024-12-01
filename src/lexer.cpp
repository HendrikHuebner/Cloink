#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "diagnostics.hpp"
#include "lexer.hpp"

using namespace clonk;

std::string clonk::opToString(TokenType op) {
    switch (op) {
        case OpPlus: return "+";
        case OpMinus: return "-";
        case OpDivide: return "/";
        case OpModulo: return "%";
        case OpMultiply: return "*";
        case OpBitNot: return "~";
        case OpOr: return "|";
        case OpXor: return "^";
        case OpAmp: return "&";
        case OpLogicalAnd: return "&&";
        case OpEquals: return "==";
        case OpGreaterEq: return ">=";
        case OpLessEq: return "<=";
        case OpNot: return "!";
        case OpGreaterThan: return ">";
        case OpNotEquals: return "!=";
        case OpLogicalOr: return "||";
        case OpLessThan: return "<";
        case OpShiftLeft: return "<<";
        case OpAssign: return "=";
        case OpShiftRight: return ">>";
        default: return "";
    }
}

std::string Token::to_string() const {
    switch (this->type) {
        case IdentifierType: return std::string(this->getIdentifier());
        case NumberLiteral: return std::to_string(this->getValue());
        case EndOfFile: return "EOF";
        case KeyAuto: return "auto";
        case KeyRegister: return "register";
        case KeyIf: return "if";
        case KeyElse: return "else";
        case KeyWhile: return "while";
        case KeyReturn: return "return";
        case ParenthesisR: return ")";
        case ParenthesisL: return "(";
        case BraceR: return "}";
        case BraceL: return "{";
        case BracketR: return "]";
        case BracketL: return "[";
        case SizeSpec: return "@";
        case Comma: return ",";
        case EndOfStatement: return ";";
        default: opToString(this->type);
    }

    return "";
}

const std::unordered_map<std::string_view, TokenType> keywords = {
    {"auto", TokenType::KeyAuto},         {"return", TokenType::KeyReturn},
    {"register", TokenType::KeyRegister}, {"if", TokenType::KeyIf},
    {"else", TokenType::KeyElse},         {"while", TokenType::KeyWhile},
};

enum CharType {
    X,  // None
    A,  // Alphabetical char or underscore
    O,  // Operator
    P,  // Punctuation char
    N   // Numeric char
};

const CharType lut[128] = {
    X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
    X, O, X, X, X, O, O, X, P, P, O, O, P, O, P, O, N, N, N, N, N, N, N, N, N, N, X, P, O, O, O, X,
    P, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, P, X, P, O, A,
    X, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, P, O, P, O, X,
};

inline CharType lookupChar(unsigned char c) {
    if (c >= 128)
        return X;
    return lut[c];
}

inline bool isBaseChar(char c) {
    return std::isalnum(c) || c == '_';
}

Token TokenStream::next() {
    if (top) {
        Token token = top.value();
        top = std::nullopt;
        return token;
    }

    char c = moveToNextToken();
    if (position >= input.size() || c == 0) {
        return Token(TokenType::EndOfFile);
    }

    CharType charType = lookupChar(c);
    switch (charType) {
        case CharType::A: return lexWord();
        case CharType::N: return lexNumber();
        case CharType::O: return lexOperator();
        case CharType::P: return lexPunctuationChar();
        default: {
            position++;
            DiagnosticsManager::get().unknownToken(*this);
            exit(EXIT_FAILURE);
        }
    }
}

Token TokenStream::peek() {
    if (top) {
        return top.value();
    }

    Token next = this->next();
    this->top = std::make_optional<Token>(next);
    return next;
}

bool TokenStream::empty() {
    return this->peek().type == TokenType::EndOfFile;
}

char TokenStream::moveToNextToken() {
    while (position < input.size()) {
        char c = input[position];

        if (c == '/' && position + 1 < input.size()) {
            if (input[++position] == '/') {

                // skip comment
                while (position < input.size() && input[position] != '\n') {
                    position++;
                }

                if (position >= input.size()) {
                    return 0;
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
        case '+': op = TokenType::OpPlus; break;
        case '-': op = TokenType::OpMinus; break;
        case '*': op = TokenType::OpMultiply; break;
        case '/': op = TokenType::OpDivide; break;
        case '%': op = TokenType::OpModulo; break;
        case '^': op = TokenType::OpXor; break;
        case '~': op = TokenType::OpBitNot; break;
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
                op = TokenType::OpLogicalAnd;
                position++;
            } else {
                op = TokenType::OpAmp;
            }
            break;
        }
        case '|': {
            if (c2 == '|') {
                op = TokenType::OpLogicalOr;
                position++;
            } else {
                op = TokenType::OpOr;
            }
            break;
        }
    }

    position++;

    if (!op.has_value()) {
        DiagnosticsManager::get().unknownToken(*this);
        exit(EXIT_FAILURE);

    } else {
        return {op.value()};
    }
}

Token TokenStream::lexWord() {
    size_t start = position;

    while (position < input.size() && isBaseChar(input[position])) {
        position++;
    }

    std::string_view lexeme = input.substr(start, position - start);

    if (keywords.find(lexeme) != keywords.end()) {
        return {keywords.at(lexeme)};
    }

    return {TokenType::IdentifierType, lexeme};
}

Token TokenStream::lexNumber() {
    size_t start = position;

    while (position < input.size() && std::isdigit(input[position])) {
        position++;
    }

    return Token(TokenType::NumberLiteral,
                 static_cast<uint64_t>(atoll(input.substr(start, position).cbegin())));
}

Token TokenStream::lexPunctuationChar() {
    switch (input[position++]) {
        case ';': return TokenType::EndOfStatement;
        case '(': return TokenType::ParenthesisL;
        case ')': return TokenType::ParenthesisR;
        case '[': return TokenType::BracketL;
        case ']': return TokenType::BracketR;
        case '{': return TokenType::BraceL;
        case '}': return TokenType::BraceR;
        case '@': return TokenType::SizeSpec;
        case ',': return TokenType::Comma;
        default: throw std::runtime_error("Invalid punctuation character!");
    }
}
