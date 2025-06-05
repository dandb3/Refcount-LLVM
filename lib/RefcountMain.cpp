#include "RefcountMain.h"

#include "llvm/ADT/MapVector.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Support/Debug.h"

#include <fstream>

using namespace llvm;

#define REF_DEBUG

#ifdef REF_DEBUG
#define REF_LOG(args) args
#else
#define REF_LOG(args)
#endif

StructType *Refcount::typeAtomic;
StructType *Refcount::typeAtomic64;
StructType *Refcount::typeKref;
StructType *Refcount::typeRefcountStruct;
bool Refcount::refcountAllExist = false;

PreservedAnalyses Refcount::run(Module &M, ModuleAnalysisManager &MAM) {
    Refcount::typeAtomic
        = StructType::getTypeByName(M.getContext(), "struct.atomic_t");
    Refcount::typeAtomic64
        = StructType::getTypeByName(M.getContext(), "struct.atomic64_t");
    Refcount::typeKref
        = StructType::getTypeByName(M.getContext(), "struct.kref");
    Refcount::typeRefcountStruct
        = StructType::getTypeByName(M.getContext(), "struct.refcount_struct");
    // if (Refcount::typeAtomic == nullptr || Refcount::typeAtomic64 == nullptr
    //     || Refcount::typeKref == nullptr || Refcount::typeRefcountStruct == nullptr) {
    //     llvm::errs() << "Not Enough Refcount Structs!\n";
    //     return PreservedAnalyses::none();
    // }

    for (StructType *ST : M.getIdentifiedStructTypes()) {
        if (ST->isOpaque()) {
            continue;
        }

        if (true) {
        // if (Refcount::containRefcountType(ST)) {
            ST->print(llvm::outs());
            llvm::outs() << "\n";
        }
    }
    
    return PreservedAnalyses::none();
}

bool Refcount::isRefcountType(StructType *ST) {
    if (ST == Refcount::typeAtomic || ST == typeAtomic64
        || ST == Refcount::typeKref || ST == typeRefcountStruct) {
        return true;
    }
    else {
        return false;
    }
}

bool Refcount::containRefcountType(StructType *ST) {
    StructType *fieldST;

    if (ST == Refcount::typeKref || ST == Refcount::typeRefcountStruct) {
        return false;
    }

    // REF_LOG(llvm::outs() << ST->getName() << "\n");

    unsigned int numElem = ST->getNumElements();
    for (unsigned int i = 0; i < numElem; ++i) {
        Type *fieldTy = ST->getElementType(i);

        if ((fieldST = dyn_cast<StructType>(fieldTy)) == nullptr)
            continue;

        if (Refcount::isRefcountType(fieldST))
            return true;
        // REF_LOG(llvm::outs() << "    " << fieldST->getName() << "\n");
    }

    return false;
}
