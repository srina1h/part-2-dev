#include <vector>
#include <set>
#include <map>
#include "llvm/IR/Function.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
using namespace llvm;

namespace
{

    class SeminalInputFeaturesAnalysis
    {
    private:
        struct Variable
        {
            std::string name;
            int line;
            Variable(std::string n, int l) : name(n), line(l) {}
        };

    public:
        std::unordered_map<std::string, Variable> variables;
        std::set<std::string> exploredPoints;

        void run(Function &F, LoopInfo &LI)
        {
            // TODO: loops to identify influential variables
            analyzeLoops(LI, F);
        };

        void analyzeLoops(LoopInfo &LI, Function &F)
        {
            for (Loop *L : LI)
            {
                BasicBlock *BB = L->getHeader();
                for (Instruction &I : *BB)
                {
                    if (auto *BI = dyn_cast<BranchInst>(&I))
                    {
                        if (BI->isConditional())
                        {
                            for (auto &op : BI->operands())
                            {
                                defUseAnalysis(*op.get(), F);
                            };
                        }
                    }
                }
            }
        }

        void defUseAnalysis(Value &v, Function &F)
        {
            /* try to check if the val is already visited through exploredPoints and if not then add it to the exploredPoints.
            If it is already visited then return */
            if (exploredPoints.find(v.getName().str()) != exploredPoints.end())
            {
                return;
            }
            exploredPoints.insert(v.getName().str());

            if (Instruction *I = dyn_cast<Instruction>(&v))
            {
                // check if the instruction is a store instruction
                if (StoreInst *SI = dyn_cast<StoreInst>(I))
                {
                    Value *val = SI->getValueOperand();
                    if (ConstantInt *CI = dyn_cast<ConstantInt>(val))
                    {
                        int line = I->getDebugLoc().getLine();
                        std::string name = SI->getPointerOperand()->getName().str();
                        Variable var(name, line);
                        variables[name] = var;
                    }
                }
                // check if the instruction is a load instruction
                else if (LoadInst *LI = dyn_cast<LoadInst>(I))
                {
                    std::string name = LI->getPointerOperand()->getName().str();
                    if (variables.find(name) != variables.end())
                    {
                        int line = I->getDebugLoc().getLine();
                        variables[name].line = line;
                    }
                }
                // check if the instruction is a binary operation
                else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(I))
                {
                    for (auto &op : BO->operands())
                    {
                        // recursively call the defUseAnalysis function on the operands
                        defUseAnalysis(*op, F);
                    }
                }
                // check if the instruction is a call instruction
                else if (CallInst *CI = dyn_cast<CallInst>(I))
                {
                    Function *calledFunction = CI->getCalledFunction();
                    if (calledFunction->isIntrinsic())
                    {
                        std::string name = calledFunction->getName().str();
                        if (name.find("llvm.") != std::string::npos)
                        {
                            int line = I->getDebugLoc().getLine();
                            std::string varName = CI->getArgOperand(0)->getName().str();
                            Variable var(varName, line);
                            variables[varName] = var;
                        }
                    }
                }
            }
        }
    };

    struct Part2Pass : public PassInfoMixin<Part2Pass>
    {
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM)
        {
            LoopInfo &loopDetails = FAM.getResult<LoopAnalysis>(F);
            SeminalInputFeaturesAnalysis analysis;
            analysis.run(F, loopDetails);
            return PreservedAnalyses::all();
        }
    };

}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo()
{
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "Part 2 pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB)
        {
            PB.registerPipelineStartEPCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>)
                {
                    if (Name == "part2pass")
                    {
                        FPM.addPass(Part2Pass());
                        return true;
                    }
                    return false;
                });
        }};
}