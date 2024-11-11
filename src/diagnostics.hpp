#pragma once

#include <cstdlib>
#include <iostream>
#include <ostream>
#include <string>
#include "lexer.hpp"

class DiagnosticError {

    const std::string& line;
    const int lineNum;
    const int linePosition;
    const std::string message;

  public:
    DiagnosticError(const std::string& line, int lineNum, int linePosition, const std::string message = "") :
        line(line), lineNum(lineNum), linePosition(linePosition), message(message) {}
    
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
    public:
    std::string unknownToken(const TokenStream& ts) {
        std::string line = ts.getCurrentLine();
        int lineNum = ts.getCurrentLineNumber();
        int linePosition = ts.getLinePosition();

        DiagnosticError error(line, lineNum, linePosition, "Unknown Token");
        std::cerr << error << std::endl;

        exit(EXIT_FAILURE);
    }

    std::string parsingError(const TokenStream& ts, const Token& token) {
        std::string line = ts.getCurrentLine();
        int lineNum = ts.getCurrentLineNumber();
        int linePosition = ts.getLinePosition();
        
        DiagnosticError error(line, lineNum, linePosition, "Unexpected Token \"" + token.to_string() + "\"");
        std::cerr << error << std::endl;

        exit(EXIT_FAILURE);
    }
};
