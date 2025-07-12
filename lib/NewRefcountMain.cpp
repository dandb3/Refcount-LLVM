#include "NewRefcountMain.h"

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

PreservedAnalyses Refcount::run(llvm::Module &M, ModuleAnalysisManager &MAM) {
    const auto &filename = M.getSourceFileName();
    const auto &structVec = M.getIdentifiedStructTypes();

    std::set<StructType *> structSet;
    std::map<std::string, std::pair<RefcountNode *, bool>> cache;
    RefcountNode *root;

    for (StructType *ST : structVec) {
        if (ST == nullptr) {
            llvm::errs() << "ERROR: ST == NULL!\n";
        }
        if (!ST->isOpaque()) {
            structSet.insert(ST);
        }
    }

    while (!structSet.empty()) {
        StructType *ST = *structSet.begin();
        structSet.erase(structSet.begin());

        root = RefcountNode::makeTree(ST, structSet, cache);
        if (root == nullptr) {
            // LOG
            // cleanup routine
        }
        cache[RefLLVMLib::getRawTypeName(ST)] = { root, false };
        // 집어넣을 때 이미 존재하면 에러처리?
    }
    llvm::outs() << "structSet is empty!\n";
    for (auto &p : cache) {
        llvm::outs() << "Name: " << p.first << "\n";
        p.second.first->print();
        if (p.first == "struct.kref" || p.first == "struct.refcount_struct"
            || p.second.second || !p.second.first->hasRefcountField()) {
            // llvm::outs() << "target: " << p.first << "\n";
            delete p.second.first;
            // llvm::outs() << "delete finished\n";
            continue;
        }
        // llvm::outs() << "step 1\n";
        const std::string &name = p.second.first->getTypeName();

        if (name == "") {
            // LOG
        }
        else {
            LS.llvmRefcountTrees.insert({ name, p.second.first });
            // insert 실패하면 LOG => 같은 파일 내부에 동일한 이름이 존재하기 때문.
        }
    }
    
    return PreservedAnalyses::none();
}
