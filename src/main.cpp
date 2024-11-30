#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include "ast.hpp"
#include "debug.hpp"
#include "diagnostics.hpp"
#include "lexer.hpp"
#include "codegen.hpp"
#include "parser.hpp"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>

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
    const std::filesystem::path path = argv[2];

    std::ifstream f(path);

    if (!f) {
        logger::warn("File not found: " + path.string());
        exit(EXIT_FAILURE);
    }

    std::stringstream ss;
    ss << f.rdbuf();
    std::string program = ss.str();

    clonk::TokenStream ts(program);
    clonk::Parser parser(ts);
    clonk::AbstractSyntaxTree ast = parser.parseProgram();

    if (option == "-a") {
        std::cout << ast.to_string();
        
    } else if (option == "-l") {
        llvm::LLVMContext ctx;
        std::unique_ptr<llvm::Module> mod = clonk::createModule(ctx, path.filename(), ast);

        if(llvm::verifyModule(*mod, &llvm::errs())) {
            mod->print(llvm::errs(), nullptr, false, true);
        }

        mod->print(llvm::outs(), nullptr, false, true); 

    } else if (option != "-c") {
        printUsage();
        return EXIT_FAILURE;
    }

    if (clonk::DiagnosticsManager::get().isError()) {
        clonk::DiagnosticsManager::get().printErrors(std::cerr);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
