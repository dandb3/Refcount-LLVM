#ifndef DYNAMIC_H
#define DYNAMIC_H

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

class DynamicCounter : public llvm::PassInfoMixin<DynamicCounter> {
public:
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);

    static bool isRequired() { return true; }
};

#endif
