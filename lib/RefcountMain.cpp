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

static bool isAnonymous(StructType *ST) {
    if (ST == nullptr)
        return false;

    std::string name = ST->getName().str();
    size_t start = name.find_first_of('.');
    size_t end = name.find('.', start + 1);

    if (start == std::string::npos) {
        llvm::errs() << "ERROR: struct name\n";
        exit(1);
    }

    return (name.compare(start + 1, end - (start + 1), "anon") == 0);
}

StructType *Refcount::getNamedAncestor(std::vector<StructType *> &structTypes, StructType *ST) {
    bool found;
    
    // Assume that each anonymous struct is uniquely contained within a single parent struct.
    while (isAnonymous(ST)) {
        found = false;
        for (StructType *cur : structTypes) {
            if (containStructType(cur, ST)) {
                if (found) {
                    // check if there exists multiple structs containing same anonymous struct
                    *GS.llvmMultipleStructWithAnonLog << "file: " << filename << ", " << ST->getName() << "\n";
                    break;
                }
                found = true;
                ST = cur;
            }
        }
        if (!found) {
            *GS.llvmAnonymousStructLog << "file: " << filename << ", " << ST->getName() << "\n";
            ST = nullptr;
        }
    }
    return ST;
}

PreservedAnalyses Refcount::run(llvm::Module &M, ModuleAnalysisManager &MAM) {
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

    filename = M.getSourceFileName();
    auto structTypes = M.getIdentifiedStructTypes();

    for (StructType *ST : structTypes) {
        if (ST == nullptr) {
            llvm::errs() << "ERROR: ST == NULL!\n";
        }
        if (ST->isOpaque()) {
            continue;
        }

        if (Refcount::containRefcountType(ST)) {
            StructType *namedST;
            // ST->print(llvm::outs());
            // llvm::outs() << "\n";
            namedST = getNamedAncestor(structTypes, ST);
            if (namedST != nullptr) {
                StringRef name = namedST->getName();
                LS.llvmStructNames.insert(name.substr(name.find_first_of(".") + 1));
            }
        }
    }
    
    return PreservedAnalyses::none();
}

bool Refcount::isRefcountType(StructType *ST) {
    if (ST == Refcount::typeAtomic || ST == Refcount::typeAtomic64
        || ST == Refcount::typeKref || ST == Refcount::typeRefcountStruct) {
        return true;
    }
    else {
        return false;
    }
}

bool Refcount::containStructType(StructType *ST, StructType *targetST) {
    unsigned int numElem;
    StructType *fieldST;
    Type *fieldTy;

    if (ST == Refcount::typeKref || ST == Refcount::typeRefcountStruct) {
        return false;
    }

    // REF_LOG(llvm::outs() << ST->getName() << "\n");

    numElem = ST->getNumElements();
    for (unsigned int i = 0; i < numElem; ++i) {
        fieldTy = ST->getElementType(i);

        if ((fieldST = dyn_cast<StructType>(fieldTy)) == nullptr)
            continue;

        if (fieldST == targetST)
            return true;
        // REF_LOG(llvm::outs() << "    " << fieldST->getName() << "\n");
    }

    return false;
}

bool Refcount::containRefcountType(StructType *ST) {
    unsigned int numElem;
    StructType *fieldST;
    Type *fieldTy;

    if (ST == Refcount::typeKref || ST == Refcount::typeRefcountStruct) {
        return false;
    }

    // REF_LOG(llvm::outs() << ST->getName() << "\n");

    numElem = ST->getNumElements();
    for (unsigned int i = 0; i < numElem; ++i) {
        fieldTy = ST->getElementType(i);

        if ((fieldST = dyn_cast<StructType>(fieldTy)) == nullptr)
            continue;

        if (isRefcountType(fieldST))
            return true;
        // REF_LOG(llvm::outs() << "    " << fieldST->getName() << "\n");
    }

    return false;
}
