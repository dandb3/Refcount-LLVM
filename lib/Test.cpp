#include "Test.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

PreservedAnalyses Test::run(Module &M, ModuleAnalysisManager &MAM) {
    for (auto &F : M) {
        if (F.isDeclaration()) {
            return PreservedAnalyses::all();
        }

        Instruction *FHead = &*F.getEntryBlock().getFirstInsertionPt();
        IRBuilder<> Builder(FHead);

        auto Val0 = ConstantInt::get(Builder.getInt32Ty(), 0);

        // Value *Var = Builder.CreateAlloca(Builder.getInt32Ty());
        // Builder.CreateStore(, Var);

        Value *Cond = Builder.CreateIsNull(Val0);

        Instruction *ThenTerm = nullptr;
        Instruction *ElseTerm = nullptr;
        SplitBlockAndInsertIfThenElse(Cond, FHead, &ThenTerm, &ElseTerm);
    }

    return PreservedAnalyses::none();
}

PassPluginLibraryInfo getTestPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "TesT", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(Test());
                });
        }};
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getTestPluginInfo();
}
