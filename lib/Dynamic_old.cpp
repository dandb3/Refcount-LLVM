#include "Dynamic.h"

#include "llvm/ADT/MapVector.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

PreservedAnalyses DynamicCounter::run(Module &M, ModuleAnalysisManager &MAM) {
    MapVector<Constant *, GlobalVariable *> result;
    auto &CTX = M.getContext();

    Constant *formatStr = ConstantDataArray::getString(CTX, "Function Name: %s, Called: %d\n");
    GlobalVariable *formatStrVar = new GlobalVariable(M, formatStr->getType(), true, GlobalValue::PrivateLinkage, formatStr, "formatStr");
    formatStrVar->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
        
    PointerType *printfArgType = PointerType::getUnqual(Type::getInt8Ty(CTX));
    FunctionType *printfType = FunctionType::get(IntegerType::getInt32Ty(CTX), {printfArgType}, true);
    FunctionCallee printfFunc = M.getOrInsertFunction("printf", printfType);

    for (auto &F : M) {
        if (F.isDeclaration())
            continue;
        
        IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());

        GlobalVariable *counter = new GlobalVariable(M, Builder.getInt32Ty(), false, GlobalValue::PrivateLinkage, Builder.getInt32(0), "counter_" + F.getName());
        
        Constant *funcNameVar = Builder.CreateGlobalStringPtr(F.getName());

        result[funcNameVar] = counter;

        LoadInst *load = Builder.CreateLoad(Builder.getInt32Ty(), counter);
        Value *val = Builder.CreateAdd(load, Builder.getInt32(1));
        Builder.CreateStore(val, counter);
    }

    FunctionType *printfWrapperTy = FunctionType::get(Type::getVoidTy(CTX), {}, false);
    Function *printfWrapper = dyn_cast<Function>(M.getOrInsertFunction("printf_wrapper", printfWrapperTy).getCallee());
    printfWrapper->setDSOLocal(true);

    BasicBlock *retBlock = BasicBlock::Create(CTX, "", printfWrapper);

    IRBuilder<> Builder(retBlock);

    for (auto &it : result) {
        LoadInst *load = Builder.CreateLoad(Builder.getInt32Ty(), it.second);
        Builder.CreateCall(printfFunc, {formatStrVar, it.first, load});
    }
    Builder.CreateRetVoid();

    appendToGlobalDtors(M, printfWrapper, 0);
    return PreservedAnalyses::none();
}

PassPluginLibraryInfo getDynamicPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "wow", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
          PB.registerPipelineParsingCallback(
              [](StringRef Name, ModulePassManager &MPM,
                 ArrayRef<PassBuilder::PipelineElement>) {
                if (Name == "wow") {
                  MPM.addPass(DynamicCounter());
                  return true;
                }
                return false;
              });
        }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getDynamicPluginInfo();
}
