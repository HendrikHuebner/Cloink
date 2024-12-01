#include <getopt.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include "ast.hpp"
#include "codegen.hpp"
#include "debug.hpp"
#include "diagnostics.hpp"
#include "lexer.hpp"
#include "parser.hpp"

enum class Mode { AST, CHECK, CODEGEN, NONE };

void printUsage() {
    std::cerr << "usage: ./clonk (-a|-c|-l|-b) source_file\n"
              << "    Exits with non-zero status code on invalid input.\n"
              << "    -a: print AST as S-Expressions.\n"
              << "    -c: syntax/semantic check only (build AST nonetheless). No output other than "
                 "the exit code.\n"
              << "    -l: generate LLVM IR and print it.\n"
              << "    -b: benchmark parsing and codegen time.\n";
}

Mode parseOption(int argc, char* argv[], bool& benchmark, std::filesystem::path& path) {
    int opt;
    benchmark = false;
    Mode mode = Mode::NONE;

    while ((opt = getopt(argc, argv, "aclb")) != -1) {
        switch (opt) {
            case 'a': mode = Mode::AST; break;
            case 'c': mode = Mode::CHECK; break;
            case 'l': mode = Mode::CODEGEN; break;
            case 'b': benchmark = true; break;
            default: return Mode::NONE;
        }
    }

    if (optind != argc - 1) {
        return Mode::NONE;
    }

    path = argv[optind];
    return mode;
}

std::string readProgram(std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        logger::warn("File not found: " + path.string());
        exit(EXIT_FAILURE);
    }

    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    bool benchmark = false;
    std::filesystem::path path;

    Mode mode = parseOption(argc, argv, benchmark, path);
    clonk::AbstractSyntaxTree ast;
    std::chrono::steady_clock::time_point start, end;

    if (mode == Mode::NONE) {
        printUsage();
        return EXIT_FAILURE;

    } else {
        if (benchmark) {
            start = std::chrono::steady_clock::now();
        }

        std::string program = readProgram(path);
        clonk::TokenStream ts(program);
        clonk::Parser parser(ts);
        ast = parser.parseProgram();
    }

    if (benchmark) {
        end = std::chrono::steady_clock::now();
        std::chrono::duration<double> parse_duration = end - start;
        std::cout << "Parsing time: " << parse_duration.count() << " seconds\n";
        start = std::chrono::steady_clock::now();
    }

    switch (mode) {
        case Mode::AST: std::cout << ast.to_string(); break;
        case Mode::CHECK: break;
        case Mode::CODEGEN: {
            llvm::LLVMContext ctx;
            std::unique_ptr<llvm::Module> mod = clonk::createModule(ctx, path.filename(), ast);

            if (benchmark) {
                end = std::chrono::steady_clock::now();
                std::chrono::duration<double> codegen_duration = end - start;
                std::cout << "Codegen time: " << codegen_duration.count() << " seconds\n";
            }

            if (llvm::verifyModule(*mod, &llvm::errs())) {
                mod->print(llvm::errs(), nullptr, false, true);
            }

            mod->print(llvm::outs(), nullptr, false, true);
        } break;
        default: printUsage(); return EXIT_FAILURE;
    }

    if (clonk::DiagnosticsManager::get().isError()) {
        clonk::DiagnosticsManager::get().printErrors(std::cerr);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
