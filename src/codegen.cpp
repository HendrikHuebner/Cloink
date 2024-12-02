#include "codegen.hpp"
#include <llvm-19/llvm/IR/BasicBlock.h>
#include <llvm-19/llvm/IR/Instructions.h>
#include <llvm/IR/DerivedTypes.h>
#include "ast.hpp"
#include "debug.hpp"

using namespace clonk;

llvm::Value* ASTVisitor::addPHIOperands(std::string_view name, llvm::PHINode* PN, llvm::BasicBlock* BB) {
    for (auto pred = llvm::pred_begin(BB), end = llvm::pred_end(BB); pred != end; ++pred) {
        PN->addIncoming(readSSAValue(*pred, name), *pred);
    }

    return tryRemovePHI(PN);
}

llvm::Value* ASTVisitor::readSSAValue(llvm::BasicBlock* BB, std::string_view name) {
    SSABlock& blockMapping = blockMappings[BB];
    std::string n = BB->getName().str();

    llvm::Value* value;
    if ((value = blockMapping.mappings[name])) {
        return value;
    }

    if (!blockMapping.sealed) {
        llvm::PHINode* PN = builder.CreatePHI(builder.getInt64Ty(), 2);
        PN->moveBefore(&*BB->getFirstInsertionPt());
        blockMapping.incompletePhis.emplace_back(name, PN);
        value = PN;
    
    } else if (BB->hasNPredecessors(1)) {
        value = readSSAValue(BB->getSinglePredecessor(), name);

    } else {
        llvm::PHINode* PN = builder.CreatePHI(builder.getInt64Ty(), 2);
        PN->moveBefore(&*BB->getFirstInsertionPt());
        blockMapping.mappings[name] = PN;
        value = addPHIOperands(name, PN, BB);
    }

    blockMapping.mappings[name] = value;
    return value;

}

llvm::Value* ASTVisitor::visit(const clonk::ASTNode* node) {
    if (auto* expr = dynamic_cast<const clonk::Expression*>(node)) {
        return visitExpression(expr);
    } else if (auto* stmt = dynamic_cast<const clonk::Statement*>(node)) {
        return visitStatement(stmt);
    }
    return nullptr;
}

llvm::Value* ASTVisitor::visitExpression(const clonk::Expression* expr, bool getAddr) {
    if (auto* id = dynamic_cast<const clonk::Identifier*>(expr)) {
        return visitIdentifier(id);
    } else if (auto* lit = dynamic_cast<const clonk::IntLiteral*>(expr)) {
        return visitIntLiteral(lit);
    } else if (auto* binOp = dynamic_cast<const clonk::BinOp*>(expr)) {
        return visitBinOp(binOp);
    } else if (auto* unOp = dynamic_cast<const clonk::UnOp*>(expr)) {
        return visitUnOp(unOp);
    } else if (auto* call = dynamic_cast<const clonk::FunctionCall*>(expr)) {
        return visitFunctionCall(call);
    } else if (auto* indexExpr = dynamic_cast<const clonk::IndexExpr*>(expr)) {
        return visitIndexingOp(indexExpr, getAddr);
    }

    assert(false && "Unknown expression");

    return nullptr;
}

llvm::Value* ASTVisitor::visitStatement(const clonk::Statement* stmt) {
    if (auto* decl = dynamic_cast<const clonk::Declaration*>(stmt)) {
        return visitDeclaration(decl);
    } else if (auto* returnStmt = dynamic_cast<const clonk::ReturnStatement*>(stmt)) {
        return visitReturnStatement(returnStmt);
    } else if (auto* ifStmt = dynamic_cast<const clonk::IfStatement*>(stmt)) {
        return visitIfStatement(ifStmt);
    } else if (auto* whileStmt = dynamic_cast<const clonk::WhileStatement*>(stmt)) {
        return visitWhileStatement(whileStmt);
    } else if (auto* block = dynamic_cast<const clonk::Block*>(stmt)) {
        return visitBlock(block);
    } else if (auto* exprStmt = dynamic_cast<const clonk::ExprStatement*>(stmt)) {
        return visitExpression(exprStmt->expr.get());
    }

    return nullptr;
}

llvm::Value* ASTVisitor::visitIdentifier(const clonk::Identifier* ident) {
    auto opt = symbolTable.get(ident->name);
    if (opt) {
        if(opt->isRegister || opt->isFunctionParam) {
            return readSSAValue(builder.GetInsertBlock(), ident->name);
        }

        return opt->value;
    }
    
    return nullptr;
}

llvm::Value* ASTVisitor::visitIntLiteral(const clonk::IntLiteral* lit) {
    return builder.getInt64(lit->value);
}

llvm::Value* ASTVisitor::visitBinOp(const clonk::BinOp* binOp, bool allowBoolResult) {
    llvm::Value* left = visit(binOp->leftExpr.get());

    llvm::Value* right = nullptr;
    if (binOp->op != clonk::OpLogicalAnd && binOp->op != clonk::OpLogicalOr) {
        right = visit(binOp->rightExpr.get());
    }

    llvm::IntegerType* ty = llvm::Type::getInt64Ty(context);

    // Load values if operands are pointers
    if (right && right->getType()->isPointerTy()) {
        right = builder.CreateLoad(ty, right, right->getName() + ".val");
    }

    if (binOp->op == clonk::OpAssign) {
        if (!left->getType()->isPointerTy()) {
            if (const clonk::Identifier* ident = dynamic_cast<const clonk::Identifier*>(binOp->leftExpr.get())) {
                blockMappings[builder.GetInsertBlock()].mappings[ident->name] = right;
                return right;
            } else {
                assert(false && "trying to assign non pointer that isnt a variable");
            }
        }

        builder.CreateStore(right, left);
        return right;
    }

    if (left->getType()->isPointerTy()) {
        left = builder.CreateLoad(ty, left, left->getName() + ".val");
    }

    switch (binOp->op) {
        case clonk::OpPlus: return builder.CreateAdd(left, right);
        case clonk::OpMinus: return builder.CreateSub(left, right);
        case clonk::OpMultiply: return builder.CreateMul(left, right);
        case clonk::OpDivide: return builder.CreateSDiv(left, right);
        case clonk::OpModulo: return builder.CreateSRem(left, right);
        case clonk::OpOr: return builder.CreateOr(left, right);
        case clonk::OpXor: return builder.CreateXor(left, right);
        case clonk::OpAmp: return builder.CreateAnd(left, right);

        // Comparison Operators
        case clonk::OpEquals: {
            llvm::Value* value = builder.CreateICmpEQ(left, right);
            return allowBoolResult ? value : builder.CreateSExt(value, builder.getInt64Ty());
        }
        case clonk::OpNotEquals: {
            llvm::Value* value = builder.CreateICmpNE(left, right);
            return allowBoolResult ? value : builder.CreateSExt(value, builder.getInt64Ty());
        }
        case clonk::OpGreaterThan: {
            llvm::Value* value = builder.CreateICmpSGT(left, right);
            return allowBoolResult ? value : builder.CreateSExt(value, builder.getInt64Ty());
        }
        case clonk::OpGreaterEq: {
            llvm::Value* value = builder.CreateICmpSGE(left, right);
            return allowBoolResult ? value : builder.CreateSExt(value, builder.getInt64Ty());
        }
        case clonk::OpLessThan: {
            llvm::Value* value = builder.CreateICmpSLT(left, right);
            return allowBoolResult ? value : builder.CreateSExt(value, builder.getInt64Ty());
        }
        case clonk::OpLessEq: {
            llvm::Value* value = builder.CreateICmpSLE(left, right);
            return allowBoolResult ? value : builder.CreateSExt(value, builder.getInt64Ty());
        }
        case clonk::OpLogicalOr:
        case clonk::OpLogicalAnd: {
            bool isOr = binOp->op == clonk::OpLogicalOr;

            llvm::BasicBlock* entryBB = builder.GetInsertBlock();
            llvm::BasicBlock* rhsBB =
                llvm::BasicBlock::Create(context, "rhs", builder.GetInsertBlock()->getParent());
            llvm::BasicBlock* endBB =
                llvm::BasicBlock::Create(context, "end", builder.GetInsertBlock()->getParent());
            
            blockMappings[rhsBB].sealed = true;
            blockMappings[endBB].sealed = false;

            llvm::Value* leftFalse = builder.CreateIsNull(left);
            if (isOr)
                builder.CreateCondBr(leftFalse, rhsBB, endBB);
            else 
                builder.CreateCondBr(leftFalse, endBB, rhsBB);

            builder.SetInsertPoint(rhsBB);
            right = visitExpression(binOp->rightExpr.get());
            if (right && right->getType()->isPointerTy()) {
                right = builder.CreateLoad(ty, right, right->getName() + ".val");
            }

            llvm::Value* rightVal = builder.CreateIsNull(right);
            llvm::Value* result =
                builder.CreateSelect(rightVal, builder.getInt64(0), builder.getInt64(1));
            
            builder.CreateBr(endBB);
            builder.SetInsertPoint(endBB);

            blockMappings[endBB].sealed = true;
            for (auto& entry : blockMappings[endBB].incompletePhis) {
                addPHIOperands(entry.first, entry.second, endBB);
            }

            llvm::PHINode* phiNode = builder.CreatePHI(llvm::Type::getInt64Ty(context), 2);
            phiNode->addIncoming(result, rhsBB);
            phiNode->addIncoming(builder.getInt64(isOr ? 1 : 0), entryBB);
            return phiNode;
        }
        
        default: return nullptr;
    }
}

llvm::Value* ASTVisitor::visitUnOp(const clonk::UnOp* unOp) {
    llvm::Value* expr = visitExpression(unOp->expr.get(), unOp->op == clonk::OpAmp);
    llvm::Type* ty = llvm::Type::getInt64Ty(context);

    switch (unOp->op) {
        case clonk::OpAmp: return builder.CreatePtrToInt(expr, ty);

        case clonk::OpMinus: return builder.CreateNeg(expr);

        case clonk::OpNot: {
            llvm::Value* isZero = builder.CreateIsNull(expr);
            return builder.CreateZExt(isZero, ty);
        }
        case clonk::OpBitNot: return builder.CreateNot(expr);
        default: return nullptr;
    }
}

llvm::Value* ASTVisitor::visitIndexingOp(const clonk::IndexExpr* indexExpr, bool getAddr) {
    llvm::Value* expr = visit(indexExpr->array.get());
    llvm::Value* index = visit(indexExpr->idx.get());

    if (!expr->getType()->isPointerTy()) {
        expr = builder.CreateIntToPtr(expr, llvm::PointerType::get(expr->getType(), 0));
    } else if (dynamic_cast<clonk::Identifier*>(indexExpr->array.get())) {
        expr = builder.CreateLoad(builder.getInt64Ty(), expr);
        expr = builder.CreateIntToPtr(expr, llvm::PointerType::get(expr->getType(), 0));
    }

    llvm::Type* elementType = nullptr;
    llvm::IntegerType* ty = llvm::Type::getInt64Ty(context);

    switch (indexExpr->sizeSpec) {
        case 1: elementType = llvm::Type::getInt8Ty(context); break;
        case 2: elementType = llvm::Type::getInt16Ty(context); break;
        case 4: elementType = llvm::Type::getInt32Ty(context); break;

        default:
            // this might be assigned -> return pointer
            elementType = llvm::Type::getInt64Ty(context);
            return builder.CreateGEP(elementType, expr, index);
    }

    llvm::Value* elementPtr = builder.CreateGEP(elementType, expr, index);
    if (getAddr)
        return elementPtr;

    llvm::Value* loadedValue = builder.CreateLoad(elementType, elementPtr);

    if (elementType->getIntegerBitWidth() < 64) {
        loadedValue = builder.CreateSExt(loadedValue, ty);
    }

    return loadedValue;
}

llvm::Value* ASTVisitor::visitFunctionCall(const clonk::FunctionCall* funcCall) {
    std::vector<llvm::Value*> args;
    for (const auto& param : funcCall->paramList) {
        args.push_back(visit(param.get()));
    }

    llvm::Function* func = module.getFunction(funcCall->ident->name);
    if (!func) {
        logger::warn("Unknown Function during code gen: " + funcCall->ident->name);
        exit(EXIT_FAILURE);
    }

    return builder.CreateCall(func, args);
}

llvm::Value* ASTVisitor::visitDeclaration(const clonk::Declaration* decl) {
    llvm::Value* exprValue = visit(decl->expr.get());

    if (decl->isRegister) {
        blockMappings[builder.GetInsertBlock()].mappings[decl->ident->name] = exprValue;
        symbolTable.insert(decl->ident->name, exprValue, true, false);
        return exprValue;

    } else {
        llvm::AllocaInst* alloc = autoAllocas[decl->ident->name];
        assert(alloc && "missing alloca");
        builder.CreateStore(exprValue, alloc);
        symbolTable.insert(decl->ident->name, alloc, decl->isRegister, false);
        return alloc;
    }
}

llvm::Value* ASTVisitor::visitReturnStatement(const clonk::ReturnStatement* returnStmt) {
    llvm::Value* returnValue =
        returnStmt->expr ? visit(returnStmt->expr.value().get()) : builder.getInt64(0);

    if (returnValue->getType()->isPointerTy()) {
        returnValue = builder.CreateLoad(llvm::Type::getInt64Ty(context), returnValue,
                                         returnValue->getName() + ".val");
    }

    builder.CreateRet(returnValue);
    currentBBterminated = true;

    return returnValue;
}

llvm::Value* ASTVisitor::visitBlock(const clonk::Block* block) {
    symbolTable.enterScope();

    for (const auto& stmt : block->statements) {
        auto* v = visit(stmt.get());
        if (v && dynamic_cast<const clonk::ReturnStatement*>(stmt.get())) {
            break;
        }
    }

    symbolTable.leaveScope();

    return nullptr;
}

llvm::Value* ASTVisitor::visitWhileStatement(const clonk::WhileStatement* whileStmt) {
    std::string loopName = "loop" + std::to_string(variableIndex++);
    llvm::Type* ty = llvm::Type::getInt64Ty(context);

    llvm::BasicBlock* loopCondBB =
        llvm::BasicBlock::Create(context, loopName + ".cond", currentFunction);

    builder.CreateBr(loopCondBB);
    builder.SetInsertPoint(loopCondBB);
    blockMappings[loopCondBB].sealed = false;

    llvm::Value* condition = visit(whileStmt->condition.get());
    llvm::BasicBlock* loopBodyBB = nullptr;
    llvm::BasicBlock* loopEndBB =
        llvm::BasicBlock::Create(context, loopName + ".end", currentFunction);

    if (llvm::Constant* constCond = llvm::dyn_cast<llvm::Constant>(condition)) {
        blockMappings[loopCondBB].sealed = true;

        if (constCond->isZeroValue()) {
            builder.CreateBr(loopEndBB);
            builder.SetInsertPoint(loopEndBB);
            blockMappings[loopEndBB].sealed = true;

            return nullptr;

        } else {
            // while (true)
            loopBodyBB = llvm::BasicBlock::Create(context, loopName + ".body", currentFunction);
            builder.CreateBr(loopBodyBB);
            blockMappings[loopBodyBB].sealed = true;
        }
    } else {
        // Conditional branch
        loopBodyBB = llvm::BasicBlock::Create(context, loopName + ".body", currentFunction);
        blockMappings[loopBodyBB].sealed = true;

        if (condition->getType()->isPointerTy()) {
            condition = builder.CreateLoad(ty, condition, condition->getName() + ".val");
        }

        llvm::Value* conditionValue = builder.CreateIsNull(condition);
        builder.CreateCondBr(conditionValue, loopEndBB, loopBodyBB);
    }

    builder.SetInsertPoint(loopBodyBB);
    visitStatement(whileStmt->statement.get());
    terminateBB(loopCondBB);

    blockMappings[loopCondBB].sealed = true;
    for (auto& entry : blockMappings[loopCondBB].incompletePhis) {
        addPHIOperands(entry.first, entry.second, loopCondBB);
    }

    builder.SetInsertPoint(loopEndBB);
    blockMappings[loopEndBB].sealed = true;

    return nullptr;
}

llvm::Value* ASTVisitor::visitIfStatement(const clonk::IfStatement* ifStmt) {
    std::string ifname = "if" + std::to_string(variableIndex++);
    llvm::Type* ty = llvm::Type::getInt64Ty(context);

    llvm::BasicBlock* ifCondBB =
        llvm::BasicBlock::Create(context, ifname + ".cond", currentFunction);
    builder.CreateBr(ifCondBB);
    builder.SetInsertPoint(ifCondBB);
    blockMappings[ifCondBB].sealed = true;

    llvm::Value* condition;
    if (auto binOp = dynamic_cast<const BinOp*>(ifStmt->condition.get())) {
        condition = visitBinOp(binOp, true);
    } else {
        condition = visitExpression(ifStmt->condition.get());
    }

    llvm::BasicBlock* ifEndBB = llvm::BasicBlock::Create(context, ifname + ".end", currentFunction);
    blockMappings[ifEndBB].sealed = false;

    // check constant in if condition
    if (llvm::Constant* constCond = llvm::dyn_cast<llvm::Constant>(condition)) {

        // Only one path to if.end
        blockMappings[ifEndBB].sealed = true;

        if (constCond->isZeroValue()) {
            llvm::Value* value = nullptr;
            if (ifStmt->elseStatement) {
                value = visit(ifStmt->elseStatement->get());
            }

            terminateBB(ifEndBB);
            builder.SetInsertPoint(ifEndBB);
            return value;

        } else {
            visitStatement(ifStmt->statement.get());
            terminateBB(ifEndBB);
            builder.SetInsertPoint(ifEndBB);
            return nullptr;
        }
    }

    llvm::BasicBlock* ifBodyBB =
        llvm::BasicBlock::Create(context, ifname + ".body", currentFunction);
    llvm::BasicBlock* elseBodyBB =
        (ifStmt->elseStatement)
            ? llvm::BasicBlock::Create(context, ifname + ".else", currentFunction)
            : nullptr;

    if (condition->getType()->isPointerTy()) {
        condition = builder.CreateLoad(ty, condition, condition->getName() + ".val");
    }

    llvm::Value* conditionValue = builder.CreateIsNull(condition);
    blockMappings[ifBodyBB].sealed = true;

    // check if else statement exists
    if (!ifStmt->elseStatement) {
        builder.CreateCondBr(conditionValue, ifEndBB, ifBodyBB);
        builder.SetInsertPoint(ifBodyBB);

        visitStatement(ifStmt->statement.get());
        terminateBB(ifEndBB);

    } else {
        builder.CreateCondBr(conditionValue, elseBodyBB, ifBodyBB);
        builder.SetInsertPoint(ifBodyBB);

        visitStatement(ifStmt->statement.get());
        terminateBB(ifEndBB);

        builder.SetInsertPoint(elseBodyBB);
        blockMappings[elseBodyBB].sealed = true;
        visitStatement(ifStmt->elseStatement->get());
        terminateBB(ifEndBB);
    }

    blockMappings[ifEndBB].sealed = true;
    for (auto& entry : blockMappings[ifEndBB].incompletePhis) {
        addPHIOperands(entry.first, entry.second, ifEndBB);
    }

    builder.SetInsertPoint(ifEndBB);
    return nullptr;
}

llvm::Function* ASTVisitor::visitFunction(const clonk::Function* func) {
    variableIndex = 0;  // reset

    llvm::IntegerType* ty = builder.getInt64Ty();

    llvm::FunctionType* funcType =
        llvm::FunctionType::get(ty, std::vector<llvm::Type*>(func->params.size(), ty), false);

    llvm::Function* llvmFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                                      func->ident->name, module);

    llvm::BasicBlock* BB = llvm::BasicBlock::Create(context, "entry", llvmFunc);
    
    blockMappings[BB].sealed = true;
    builder.SetInsertPoint(BB);
    this->currentFunction = llvmFunc;

    auto paramIt = func->params.begin();
    for (llvm::Argument& llvmParam : llvmFunc->args()) {
        llvmParam.setName((*paramIt)->name);
        blockMappings[BB].mappings[(*paramIt)->name] = &llvmParam;
        symbolTable.insert((*paramIt)->name, nullptr, false, true);
        ++paramIt;
    }

    for (std::string_view varName : func->autoDecls) {
        autoAllocas[varName] = builder.CreateAlloca(ty, nullptr, varName);
    }

    visitBlock(func->block.get());
    if (!currentBBterminated) {
        builder.CreateRet(builder.getInt64(0));
    }

    return llvmFunc;
}
