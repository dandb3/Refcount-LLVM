#include "Refcount.h"

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

PreservedAnalyses Refcount::run(Module &M, ModuleAnalysisManager &MAM) {
    for (StructType *ST : M.getIdentifiedStructTypes()) {
        if (ST->isOpaque()) {
            continue;
        }
        StringRef parentType = ST->getName();
        if (isRefcountType(parentType)) {
            REF_LOG(llvm::outs() << "   "; ST->print(llvm::outs()); llvm::outs() << "\n";);
        }
        // LLVM_DEBUG(dbgs() << "Struct: " << parentType << "\n");
        // REF_LOG(llvm::outs() << parentType << "\n");
        // for (Type *ElementTy : ST->elements()) {
        //     // if (StructType *Child = dyn_cast<StructType>(ElementTy)) {
        //     //     StringRef childType = Child->getName();
        //     //     REF_LOG(llvm::outs() << "   " << childType << "\n");
        //     //     // if (isRefcountType(parentType, childType)) {
        //     //     //     log << "Parent: " << parentType.str() << "Child: " << childType.str();
        //     //     // }
        //     // }
        // }
    }
    
    return PreservedAnalyses::none();
}

bool Refcount::containRefcountType(StringRef &parentType, StringRef &childType) {
    if (parentType == "struct.kref" || parentType == "struct.refcount_struct") {
        return false;
    }
    if (childType == "struct.atomic_t") {
        return true;
    }
    else if (childType == "struct.atomic64_t") {
        return true;
    }
    else if (childType == "struct.refcount_struct") {
        return true;
    }
    else if (childType == "struct.kref") {
        return true;
    }
    else {
        return false;
    }
}

bool Refcount::isRefcountType(StringRef &type) {
    if (type == "struct.atomic_t" || type == "struct.atomic64_t"
        || type == "struct.refcount_struct" || type == "struct.kref") {
        return true;
    }
    else {
        return false;
    }
}

PassPluginLibraryInfo getRefcountPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "refcount", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                    MPM.addPass(Refcount());
                    return true;
                });
        }};
};

// PassPluginLibraryInfo getRefcountPluginInfo() {
//     return {LLVM_PLUGIN_API_VERSION, "refcount", LLVM_VERSION_STRING,
//         [](PassBuilder &PB) {
//             PB.registerPipelineStartEPCallback(
//                 [](ModulePassManager &MPM, OptimizationLevel Level) {
//                     MPM.addPass(Refcount());
//                 });
//         }};
// };

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getRefcountPluginInfo();
}
