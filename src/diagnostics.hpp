#pragma once

#include <cstdlib>
#include <iostream>
#include <ostream>
#include <string>
#include "ast.hpp"
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
    bool _isError = false;

    public:

    static DiagnosticsManager get() {
        static DiagnosticsManager instance;
        return instance;
    }
    
    void unknownToken(const TokenStream& ts) const {
        std::string line = ts.getCurrentLine();
        int lineNum = ts.getCurrentLineNumber();
        int linePosition = ts.getLinePosition();

        DiagnosticError error(line, lineNum, linePosition, "Unknown Token");
        std::cerr << error << std::endl;

        exit(EXIT_FAILURE);
    }

    void unexpectedToken(const TokenStream& ts, const Token& token, const std::string& expected = "") {
        std::string line = ts.getCurrentLine();
        int lineNum = ts.getCurrentLineNumber();
        int linePosition = ts.getLinePosition();
        
        DiagnosticError error(line, lineNum, linePosition, "Unexpected Token \"" + token.to_string() + "\"" + 
            (expected.empty() ? "" : ", expected " + expected + "."));
            
        std::cerr << error << std::endl;
        _isError = true;
    }

    void error(const TokenStream& ts, const std::string& msg = "") {
        std::string line = ts.getCurrentLine();
        int lineNum = ts.getCurrentLineNumber();
        int linePosition = ts.getLinePosition();
        
        DiagnosticError error(line, lineNum, linePosition, msg + ".");
            
        std::cerr << error << std::endl;
        _isError = true;
    } 
    
    bool isError() {
        return _isError;
    }
};
