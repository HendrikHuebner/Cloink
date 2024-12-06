#pragma once

#include <llvm/ADT/STLFunctionalExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "ast.hpp"

namespace clonk {

struct SSABlock {
    bool sealed;
    std::unordered_map<std::string_view, llvm::Value*> mappings;
    std::vector<std::pair<std::string_view, llvm::PHINode*>> incompletePhis;
};

class ASTVisitor {
   private:
    llvm::LLVMContext& context;
    llvm::Module& module;
    llvm::IRBuilder<>& builder;

    SymbolTable<llvm::Value*> symbolTable;
    std::unordered_map<std::string_view, llvm::AllocaInst*> autoAllocas;

    std::unordered_map<llvm::BasicBlock*, SSABlock> blockMappings;
    bool currentBBterminated = false;

    llvm::Function* currentFunction = nullptr;
    int variableIndex = 0;

    void terminateBB(llvm::BasicBlock* BB) {
        if (!currentBBterminated)
            builder.CreateBr(BB);
        currentBBterminated = false;
    }

   public:
    ASTVisitor(llvm::LLVMContext& ctx, llvm::Module& mod, llvm::IRBuilder<>& irBuilder)
        : context(ctx), module(mod), builder(irBuilder) {}

    // SSA construction
    llvm::Value* readSSAValue(llvm::BasicBlock* BB, std::string_view name);
    llvm::Value* tryRemovePHI(llvm::PHINode* PN) { return PN; }; // TODO
    llvm::Value* addPHIOperands(std::string_view name, llvm::PHINode* PN, llvm::BasicBlock* BB);

    llvm::Value* visit(const clonk::ASTNode* node);
    llvm::Value* visitExpression(const clonk::Expression* expr, bool getAddr = false);
    llvm::Value* visitStatement(const clonk::Statement* stmt);
    llvm::Value* visitIdentifier(const clonk::Identifier* id);
    llvm::Value* visitIntLiteral(const clonk::IntLiteral* lit);
    llvm::Value* visitBinOp(const clonk::BinOp* binOp, bool allowBoolResult = false);
    llvm::Value* visitUnOp(const clonk::UnOp* unOp);
    llvm::Value* visitIndexingOp(const clonk::IndexExpr* indexExpr, bool getAddr = false);
    llvm::Value* visitFunctionCall(const clonk::FunctionCall* funcCall);
    llvm::Value* visitDeclaration(const clonk::Declaration* decl);
    llvm::Value* visitReturnStatement(const clonk::ReturnStatement* returnStmt);
    llvm::Value* visitBlock(const clonk::Block* block);
    llvm::Value* visitWhileStatement(const clonk::WhileStatement* whileStmt);
    llvm::Value* visitIfStatement(const clonk::IfStatement* ifStmt);
    llvm::Function* visitFunction(const clonk::Function* func);
};

inline std::unique_ptr<llvm::Module> createModule(llvm::LLVMContext& ctx, const std::string& name,
                                                  const AbstractSyntaxTree& ast) {
    auto module = std::make_unique<llvm::Module>(name, ctx);
    auto builder = llvm::IRBuilder<>(ctx);
    auto astVisitor = ASTVisitor(ctx, *module, builder);

    llvm::Type* ty = builder.getInt64Ty();

    // declare extern functions
    for (const std::pair<std::string, int>& externFunc : ast.getExternFunctions()) {
        llvm::FunctionType* externFuncType =
            llvm::FunctionType::get(ty, std::vector<llvm::Type*>(externFunc.second, ty), false);

        llvm::Function::Create(externFuncType, llvm::Function::ExternalLinkage, externFunc.first,
                               *module);
    }

    for (const std::unique_ptr<clonk::Function>& func : ast.getFunctions()) {
        astVisitor.visitFunction(func.get());
    }

    return module;
}

}  // end namespace clonk
