#pragma once

#include <cstdlib>
#include <iostream>
#include <ostream>
#include <string>
#include "ast.hpp"
#include "lexer.hpp"

namespace clonk {

class DiagnosticError {

    const std::string_view line;
    const int lineNum;
    const int linePosition;
    const std::string message;

   public:
    DiagnosticError(std::string_view line, int lineNum, int linePosition,
                    const std::string& message = "")
        : line(line), lineNum(lineNum), linePosition(linePosition), message(message) {}

    friend std::ostream& operator<<(std::ostream& os, const DiagnosticError& err);
};

inline std::ostream& operator<<(std::ostream& os, const DiagnosticError& err) {
    os << "error in line " << err.lineNum << ": ";
    os << err.message << std::endl;
    os << err.line << std::endl;
    std::string indent(err.linePosition, '-');
    os << indent << '^' << std::endl;

    return os;
}

class DiagnosticsManager {
    std::vector<DiagnosticError> errors;
    bool _isError = false;

   public:
    static DiagnosticsManager& get() {
        static DiagnosticsManager instance;
        return instance;
    }

    void unknownToken(const TokenStream& ts) {
        std::string_view line = ts.getCurrentLine();
        int lineNum = ts.getCurrentLineNumber();
        int linePosition = ts.getLinePosition();

        errors.emplace_back(line, lineNum, linePosition, "Unknown token");
        printErrors(std::cerr);
        exit(EXIT_FAILURE);
    }

    void unexpectedToken(const TokenStream& ts, const Token& token,
                         const std::string& expected = "") {
        std::string_view line = ts.getCurrentLine();
        int lineNum = ts.getCurrentLineNumber();
        int linePosition = ts.getLinePosition();

        std::string message;

        if (token.type == TokenType::EndOfFile) {
            message = "Unexpected end of file";
        } else {
            message = "Unexpected Token \"" + token.to_string() + "\"";
        }

        if (!expected.empty())
            message += ", expected \"" + expected + "\"";

        errors.emplace_back(line, lineNum, linePosition, message);
        _isError = true;
    }

    void error(const TokenStream& ts, const std::string& message = "") {
        std::string_view line = ts.getCurrentLine();
        int lineNum = ts.getCurrentLineNumber();
        int linePosition = ts.getLinePosition();

        errors.emplace_back(line, lineNum, linePosition, message);
        _isError = true;
    }

    void printErrors(std::ostream& os) {
        for (auto& err : errors) {
            os << err;
        }
    }

    bool isError() { return _isError; }
};

}  // end namespace clonk
