#ifndef DYNAMIC_H
#define DYNAMIC_H

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"

using namespace llvm;

class Refcount : public PassInfoMixin<Refcount> {
private:

public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);

    // enum Type {
    //     NONE = -1,
    //     ATOMIC_T = 0,
    //     ATOMIC64_T,
    //     REFCOUNT_T,
    //     KREF
    // };

    bool containRefcountType(StringRef &parentType, StringRef &childType);
    bool isRefcountType(StringRef &type);
    static bool isRequired() { return true; }
};

#endif
