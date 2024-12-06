#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PatternMatch.h>
#include <llvm/Support/Casting.h>
#include <unordered_map>



namespace clonk {

struct DAGNode {
    llvm::Instruction* inst;
    std::vector<DAGNode*> predecessors;
};

enum PatternShape {
    SHAPE_1,
    SHAPE_2_L,
    SHAPE_2_R,
    SHAPE_3_L_L,
    SHAPE_3_L_R,
    SHAPE_3_R_L,
    SHAPE_3_R_R,
    SHAPE_3_RL,
};

enum PatternKey {
    CONST_ZERO,
    CONST_ONE,
    CONST_N,
    ADD,
    MUL,
    SUB,
    DIV
};

template<unsigned Size>
struct Pattern {
    PatternShape shape;
    std::array<PatternKey, Size> pattern;

    bool match(DAGNode* node) {

    }

    private:
    bool match(DAGNode* node, PatternKey key) {
        switch (key) {
            case CONST_ZERO:
            case CONST_ONE:
            case CONST_N: {
                if (auto constant = llvm::dyn_cast<llvm::Constant>(node->inst)) {
                    if (key == CONST_ZERO) {
                        return constant->isZeroValue();
                    } else if (key == CONST_ZERO) {
                        return constant->isOneValue();
                    } else {
                        return true;
                    }
                }

                return false;
            }
            default:  {
                if (auto inst = llvm::dyn_cast<llvm::Instruction>(node->inst)) {
                    switch (inst->getOpcode()) {
                    }
                }

                return false;
            }

        }
    }
};

static void printDAG(const std::unordered_map<llvm::Instruction*, DAGNode*>& dag) {
    for (const auto& [Inst, Node] : dag) {
        llvm::errs() << "Instruction: " << *Node->inst << "\n";
        llvm::errs() << "Depends on:\n";
        for (DAGNode* Dep : Node->predecessors) {
            llvm::errs() << "  " << *Dep->inst << "\n";
        }
        llvm::errs() << "------------\n";
    }
}

class InstructionSelector {
    llvm::LLVMContext& context;
    llvm::IRBuilder<> builder;
    llvm::Module* module;
    
    public:
    InstructionSelector(llvm::LLVMContext& ctx) : context(ctx), builder(llvm::IRBuilder<>(context)) {}

    llvm::Module* performPass(llvm::Module* m) {
        llvm::Module* result = new llvm::Module(m->getName(), m->getContext());
        this->module = result;

        for (auto& func : *m) {
            if (func.isDeclaration()) {
                continue;
            }

            for (auto& BB : func) {
                visitBasicBlock(&BB);
            }
        }

        this->module = nullptr;
        return result;
    }

    private:

    std::vector<DAGNode*> buildDAG(llvm::BasicBlock* BB) {
        std::vector<DAGNode*> roots;
        std::unordered_map<llvm::Instruction*, DAGNode*> dag;

        for (auto& inst : *BB) {
            dag[&inst] = new DAGNode{&inst, {}};
        }

        for (auto& Inst : *BB) {
            DAGNode* currentNode = dag[&Inst];
            
            for (unsigned i = 0; i < Inst.getNumOperands(); ++i) {
                llvm::Value* operand = Inst.getOperand(i);

                if (auto* dependencyInst = llvm::dyn_cast<llvm::Instruction>(operand)) {
                    if (dag.find(dependencyInst) != dag.end()) {
                        currentNode->predecessors.push_back(dag[dependencyInst]);
                    }
                }
            }
            
            if (currentNode->predecessors.empty()) {
                roots.push_back(currentNode);
            }
        }

        printDAG(dag);
        return roots;
    }

    unsigned selectPattern(DAGNode* root, Pattern& pattern) {
        llvm::match()
    }

    llvm::Value* visitBasicBlock(llvm::BasicBlock* BB) {
        std::vector<DAGNode*> roots = buildDAG(BB);
        while (!roots.empty()) {
            unsigned minWeight;
            Pattern minPattern;
            DAGNode* minRoot;

            for (DAGNode* root : roots) {
                Pattern pattern;
                unsigned weight = selectPattern(root, pattern);
                if (weight > minWeight) {
                    minWeight = weight;
                    minPattern = pattern;
                    minRoot = root;
                }
            }

            applyPattern(minRoot, minPattern);
            roots.erase(std::find(roots.begin(), roots.end(), minRoot), roots.end());
        }

        return nullptr;
    }
};


} // end namespace clonk