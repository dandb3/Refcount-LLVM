//========================================================================
// FILE:
//    StaticMain.cpp
//
// DESCRIPTION:
//    A command-line tool that counts all static calls (i.e. calls as seen
//    in the source code) in the input LLVM file. Internally it uses the
//    StaticCallCounter pass.
//
// USAGE:
//    # First, generate an LLVM file:
//      clang -emit-llvm <input-file> -c -o <output-llvm-file>
//    # Now you can run this tool as follows:
//      <BUILD/DIR>/bin/static <output-llvm-file>
//
// License: MIT
//========================================================================

#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Regex.h"
#include "llvm/IR/Attributes.h"
#include "llvm/Transforms/Utils/dandb.h"

#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <climits>

using namespace llvm;

// #define TARGET_BASE_DIR "/home/jdoh/Desktop/work/linux-v5.6/"
// #define LOG_DIR "/home/jdoh/Desktop/work/Refcount-LLVM/log/linux/"

#define TARGET_BASE_DIR "/home/jdoh/Desktop/work/linux-v6.17/"
// #define LOG_DIR "/home/jdoh/Desktop/work/Refcount-LLVM/log/linux/"

//===----------------------------------------------------------------------===//
// Command line options
//===----------------------------------------------------------------------===//
static cl::OptionCategory CallCounterCategory{"call counter options"};

static cl::opt<std::string> InputModule{cl::Positional,
                                        cl::desc{"<Module to analyze>"},
                                        cl::value_desc{"bitcode filename"},
                                        cl::init(""),
                                        cl::Required,
                                        cl::cat{CallCounterCategory}};


class Refcount : public PassInfoMixin<Refcount> {
private:
    LLVMContext *CTX;

    bool regexCompare(const std::string &regex, const std::string &target) {
        llvm::Regex R(regex);

        return R.match(target);
    }

    void init(Module &M) {
        CTX = &M.getContext();
    }

    void fini() {

    }

public:
    std::map<Function *, std::set<Function *>> CallerMap;

    std::set<Function *> &getOrFindCallers(Function *Callee) {
        auto it = CallerMap.find(Callee);
        if (it == CallerMap.end()) {
            it = CallerMap.insert({ Callee, std::set<Function *>() }).first;
            for (auto *U : Callee->users()) {
                if (auto *CB = dyn_cast<CallBase>(U)) {
                    if (auto *CallerBB = CB->getParent()) {
                        if (auto *Caller = CallerBB->getParent()) {
                            it->second.insert(Caller);
                        }
                    }
                }
            }
        }

        return it->second;
    }

    void analysisInstruction(Function *Callee, std::set<Function *> &RootCallerSet, std::set<Function *> &Visited) {
        auto res = Visited.insert(Callee);
        if (!res.second)
            return;
        
        // llvm::errs() << "    " << Callee->getName() << "\n";

        auto &Callers = getOrFindCallers(Callee);
        if (Callers.size() == 0 ||
            (Callers.size() == 1 && *Callers.begin() == Callee)) {
            RootCallerSet.insert(Callee);
            return;
        }
        for (auto *Caller : Callers) {
            analysisInstruction(Caller, RootCallerSet, Visited);
        }
    }

    PreservedAnalyses run(llvm::Module &M, ModuleAnalysisManager &MAM) {
        // llvm::outs() << "run() called\n";
        init(M);

        std::vector<Function *> CalleeVec;
        
        for (auto &F : M) {
            if (F.isDeclaration())
                continue;
            if (F.hasFnAttribute(Attribute::FieldArg0) || F.hasFnAttribute(Attribute::FieldArg1)) {
               CalleeVec.push_back(&F);
            }
        }

        std::map<Function *, std::set<dandb::ClangID>> SyscallMap;

        for (auto *Callee : CalleeVec) {
            // llvm::errs() << "Function: " << Callee->getName() << "\n";
            for (auto *U : Callee->users()) {
                // llvm::errs() << "users exist!\n";
                if (auto *CB = dyn_cast<CallBase>(U))
                    if (MDNode *MN = CB->getMetadata(LLVMContext::MD_refop_field)) {
                        // for (int i = 0; i < MN->getNumOperands(); ++i) {
                        //     llvm::errs() << "    " << dyn_cast<MDString>(MN->getOperand(i))->getString() << "\n";
                        // }
                        if (MN->getNumOperands() == 3) {
                            // llvm::errs() << "Identified refcount operation!\n";
                            std::set<Function *> RootCallerSet;
                            std::set<Function *> Visited;

                            auto *CallerBB = CB->getParent();
                            if (!CallerBB) {
                                llvm::errs() << "No callerBB!\n";
                                continue;
                            }

                            auto *Caller = CallerBB->getParent();
                            if (!Caller) {
                                llvm::errs() << "No Caller!\n";
                                continue;
                            }
                            analysisInstruction(Caller, RootCallerSet, Visited);

                            if (auto *MDS = dyn_cast<MDString>(MN->getOperand(2))) {
                                dandb::ClangID CID(MDS->getString().str());

                                for (auto *RootCaller : RootCallerSet)
                                    SyscallMap[RootCaller].insert(CID);
                            }
                        }
                    }
            }
        }
            
        for (auto it = SyscallMap.begin(); it != SyscallMap.end(); ++it) {
            llvm::outs() << it->first->getName() << "\n";
            for (auto &CID : it->second) {
                llvm::outs() << "    ";
                CID.print(llvm::outs());
            }
        }

        fini();

        return PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }
};

void parsebcFiles(std::vector<std::string> &bcFiles) {
    std::ifstream ifs(TARGET_BASE_DIR "bitcodes.txt");
    if (!ifs.is_open()) {
        llvm::errs() << "PARSE FAILED!\n";
    }

    std::string line;

    while (std::getline(ifs, line)) {
        bcFiles.push_back(TARGET_BASE_DIR + line);
    }
}

int main(int Argc, char **Argv) {
    cl::HideUnrelatedOptions(CallCounterCategory);

    cl::ParseCommandLineOptions(Argc, Argv,
                              "Counts the number of static function "
                              "calls in the input IR file\n");


    SMDiagnostic Err;
    LLVMContext Ctx;

    std::vector<std::string> bcFiles;
    parsebcFiles(bcFiles);

    size_t count = 0;

    for (auto &file : bcFiles) {
        llvm_shutdown_obj SDO;
        llvm::outs() << "[" << ++count << "/" << bcFiles.size() << "]\n";
        llvm::outs() << file << "\n";
        std::unique_ptr<Module> M = parseIRFile(file, Err, Ctx);

        if (!M) {
            errs() << "Error reading bitcode file: " << InputModule << "\n";
            Err.print(Argv[0], errs());
            return -1;
        }

        ModulePassManager MPM;
        MPM.addPass(Refcount());

        ModuleAnalysisManager MAM;
        PassBuilder PB;
        PB.registerModuleAnalyses(MAM);

        MPM.run(*M, MAM);
    }

    // std::ofstream totalResult(LOG_DIR "../result/total.stat");
    // if (!totalResult.is_open()) {
    //     llvm::errs() << "result file open failed\n";
    // }

    // for (auto &elem : RefcountOpMap) {
    //     totalResult << elem.first.getOutput();
    //     for (auto &vecElem : elem.second) {
    //         totalResult << vecElem.getModeStr() << "," << vecElem.getVal() << "\n";
    //     }
    //     totalResult << "\n";
    // }

    return 0;
}
