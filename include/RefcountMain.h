#ifndef DYNAMIC_H
#define DYNAMIC_H

#include "Statistics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"

using namespace llvm;

class Refcount : public PassInfoMixin<Refcount> {
private:
    static StructType *typeAtomic;
    static StructType *typeAtomic64;
    static StructType *typeKref;
    static StructType *typeRefcountStruct;

    static bool refcountAllExist;

    std::string filename;
    bool isRefcountType(StructType *ST);

public:
    PreservedAnalyses run(llvm::Module &M, ModuleAnalysisManager &MAM);

    bool containStructType(StructType *ST, StructType *targetST);
    int refcountNum(StructType *ST);
    StructType *getNamedAncestor(std::vector<StructType *> &structTypes, StructType *ST);
    static bool isRequired() { return true; }
};

#endif
