#include <getopt.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_os_ostream.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include "ast.hpp"
#include "codegen.hpp"
#include "debug.hpp"
#include "diagnostics.hpp"
#include "isel.hpp"
#include "lexer.hpp"
#include "parser.hpp"

enum class Mode { AST, CHECK, IR, MIR, NONE };

void printUsage() {
    std::cerr << "usage: ./clonk (-a|-c|-l|-b) source_file\n"
              << "    Exits with non-zero status code on invalid input.\n"
              << "    -a: print AST as S-Expressions.\n"
              << "    -c: syntax/semantic check only (build AST nonetheless). No output other than "
                 "the exit code.\n"
              << "    -l: generate LLVM IR and print it.\n"
              << "    -o: output file path.\n"
              << "    -b: benchmark\n";
}

Mode parseOption(int argc, char* argv[], bool& benchmark, std::filesystem::path& path, std::filesystem::path& outputPath) {
    int opt;
    benchmark = false;
    Mode mode = Mode::NONE;

    while ((opt = getopt(argc, argv, "aclbso:")) != -1) {
        switch (opt) {
            case 'a': mode = Mode::AST; break;
            case 'c': mode = Mode::CHECK; break;
            case 'l': mode = Mode::IR; break;
            case 's': mode = Mode::MIR; break;
            case 'b': benchmark = true; break;
            case 'o': outputPath = std::filesystem::path(optarg); break;
            case '?':
                if (optopt == 'o')
                    std::cerr << "Option -o requires an argument!" << std::endl;
            
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
    std::filesystem::path outputPath;

    Mode mode = parseOption(argc, argv, benchmark, path, outputPath);

    std::ostream* outputStream = &std::cout;
    std::ofstream file;

    if (!outputPath.empty()) {
        file = std::ofstream(outputPath);
        if (!file) {
            logger::warn("Output file not found: " + path.string());
            exit(EXIT_FAILURE);
        }

        outputStream = &file;
    }

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

    std::unique_ptr<llvm::Module> mod;
    
    switch (mode) {
        case Mode::AST: *outputStream << ast.to_string() << std::endl; break;
        case Mode::CHECK: break;
        case Mode::MIR:
        case Mode::IR: {
            llvm::LLVMContext ctx;
            mod = clonk::createModule(ctx, path.filename(), ast);

            if (benchmark) {
                end = std::chrono::steady_clock::now();
                std::chrono::duration<double> codegen_duration = end - start;
                std::cout << "Codegen time: " << codegen_duration.count() << " seconds\n";
            }

            if (llvm::verifyModule(*mod, &llvm::errs())) {
                mod->print(llvm::errs(), nullptr, false, true);
                assert(false && "Invalid Module!");
            }

            llvm::raw_os_ostream os(*outputStream);
            mod->print(os, nullptr, false, true);

            if (mode == Mode::MIR) {
                clonk::InstructionSelector isel(ctx);
                mod = std::unique_ptr<llvm::Module>(isel.performPass(mod.get()));
                //mod->print(os, nullptr, false, true);
            }

            break;
        }
        default: printUsage(); return EXIT_FAILURE;
    }

    if (clonk::DiagnosticsManager::get().isError()) {
        clonk::DiagnosticsManager::get().printErrors(std::cerr);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
