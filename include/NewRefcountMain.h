#ifndef DYNAMIC_H
#define DYNAMIC_H

#include "NewStatistics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"

using namespace llvm;

class Refcount : public PassInfoMixin<Refcount> {
private:
    std::string filename;

public:
    PreservedAnalyses run(llvm::Module &M, ModuleAnalysisManager &MAM);

    static bool isRequired() { return true; }
};

#endif
