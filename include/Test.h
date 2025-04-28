#ifndef TEST_H
#define TEST_H

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"

using namespace llvm;

class Test : public PassInfoMixin<Test> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);

    static bool isRequired() { return true; }
};

#endif
