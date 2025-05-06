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
#include "RefcountMain.h"

#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <vector>

using namespace llvm;

//===----------------------------------------------------------------------===//
// Command line options
//===----------------------------------------------------------------------===//
// static cl::OptionCategory CallCounterCategory{"call counter options"};

// static cl::opt<std::string> InputModule{cl::Positional,
//                                         cl::desc{"<Module to analyze>"},
//                                         cl::value_desc{"bitcode filename"},
//                                         cl::init(""),
//                                         cl::Required,
//                                         cl::cat{CallCounterCategory}};

static void refcountIdentify(Module &M) {
  // Create a module pass manager and add StaticCallCounterPrinter to it.
  ModulePassManager MPM;
  MPM.addPass(Refcount());

  // Create an analysis manager and register StaticCallCounter with it.
  ModuleAnalysisManager MAM;
//   MAM.registerPass([&] { return StaticCallCounter(); });

  // Register all available module analysis passes defined in PassRegistry.def.
  // We only really need PassInstrumentationAnalysis (which is pulled by
  // default by PassBuilder), but to keep this concise, let PassBuilder do all
  // the _heavy-lifting_.
  PassBuilder PB;
  PB.registerModuleAnalyses(MAM);

  // Finally, run the passes registered with MPM
  MPM.run(M, MAM);
}

// static void refcountIdentify(std::vector<std::string> &objects) {
//     SMDiagnostic Err;
//     LLVMContext Ctx;

//     for (std::string &object : objects) {
//         std::unique_ptr<Module> M = parseIRFile(object, Err, Ctx);
//         if (!M) {
//             llvm::errs() << "Error reading bitcode file: " << object << "\n";
//             exit(1);
//         }
//     }
// }

int main(int argc, char *argv[]) {
    // Hide all options apart from the ones specific to this tool
    if (argc != 2) {
        llvm::errs() << "Usage: " << argv[0] << " <bc file>\n";
        return 1;
    }

    SMDiagnostic Err;
    LLVMContext Ctx;

    llvm::outs() << "Parsing Start\n";
    std::unique_ptr<Module> M = parseIRFile(argv[1], Err, Ctx);
    if (!M) {
        llvm::errs() << "Error reading bitcode file: " << argv[1] << "\n";
        return 1;
    }

    // Makes sure llvm_shutdown() is called (which cleans up LLVM objects)
    //  http://llvm.org/docs/ProgrammersManual.html#ending-execution-with-llvm-shutdown
    llvm_shutdown_obj SDO;

    llvm::outs() << "Analysis Start\n";
    refcountIdentify(*M);

    return 0;
}
