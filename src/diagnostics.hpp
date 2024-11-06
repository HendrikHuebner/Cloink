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

std::ostream& operator<<(std::ostream& os, const DiagnosticError& err) {
    os << "ERROR! line " << err.lineNum << ": " << std::endl;
    os << err.line << std::endl;
    std::string indent(err.linePosition - 1, '-');
    os << indent << '^' << std::endl;

    return os;
}

class DiagnosticsManager {
    public:

    std::string unknownToken(const TokenStream& ts, const Token& token) {
        std::string line = ts.getCurrentLine();
        int lineNum = ts.getCurrentLineNumber();
        int linePosition = ts.getLinePosition();

        DiagnosticError error(line, lineNum, linePosition);
        std::cerr << error << std::endl;

        exit(EXIT_FAILURE);
    }

    std::string parsingError(Token token) {
        std::cerr << "Parsing Error" << std::endl;
        exit(EXIT_FAILURE);
    }
};
