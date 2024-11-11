#include <cstdlib>
#include <iostream>
#include <string>
#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "diagnostics.hpp"

void printUsage() {
    std::cerr << "usage: ./clonk (-a|-c) source_file\n"
              << "    Exits with non-zero status code on invalid input.\n"
              << "    -a: print AST as S-Expressions.\n"
              << "    -c: syntax/semantic check only (build AST nonetheless). No output other than the exit code.\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printUsage();
        return EXIT_FAILURE;
    }

    const std::string option = argv[1];
    const std::string program(argv[2]);

    TokenStream ts(program);
    DiagnosticsManager dm;
    Parser parser(ts, dm);

    if (option == "-a") {
        AbstractSyntaxTree ast = parser.parseProgram();
        std::cout << ast.to_string();

    } else if (option == "-c") {
        AbstractSyntaxTree ast = parser.parseProgram();

    } else {
        printUsage();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
