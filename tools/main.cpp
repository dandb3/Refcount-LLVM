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
#include "ClangRefcount.h"

#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#define LOG_DIR "/home/junwoong/work/refcount/build1/log/"
#define COMPILE_DATABASE LOG_DIR "compile_commands.json"

GlobalStatistic GS;
LocalStatistic LS;

void initialize(std::vector<std::string> &totalFiles) {
    std::error_code errCode;
    GS.clangAnonymousStructLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "clang_anonymous_struct.log"), errCode);
    GS.llvmAnonymousStructLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "llvm_anonymous_struct.log"), errCode);
    GS.llvmMultipleStructWithAnonLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "llvm_mult_struct_with_anon.log"), errCode);
    GS.dupStructNameLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "dup_struct_name.log"), errCode);
    GS.fileAccessLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "file_access.log"), errCode);
    GS.compareLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "compare.log"), errCode);

    if (GS.clangAnonymousStructLog == nullptr || GS.llvmAnonymousStructLog == nullptr
        || GS.llvmMultipleStructWithAnonLog == nullptr || GS.dupStructNameLog == nullptr
        || GS.fileAccessLog == nullptr) {
        llvm::errs() << "ERROR: Log init failed\n";
        exit(1);
    }

    GS.fileNum = totalFiles.size();
}

void finish() {
    delete GS.clangAnonymousStructLog;
    delete GS.llvmAnonymousStructLog;
    delete GS.llvmMultipleStructWithAnonLog;
    delete GS.dupStructNameLog;
    delete GS.fileAccessLog;
    delete GS.compareLog;
}

int main(int argc, char *argv[]) {
    // Hide all options apart from the ones specific to this tool
    if (argc != 1) {
        llvm::errs() << "Usage: " << argv[0] << "\n";
        return 1;
    }

    std::string errMsg;

    auto database = clang::tooling::JSONCompilationDatabase::loadFromFile(COMPILE_DATABASE, errMsg, JSONCommandLineSyntax::AutoDetect);
    if (database == nullptr) {
        llvm::errs() << "JSON file parse failed\n";
        return 1;
    }


    llvm_shutdown_obj SDO;

    std::vector<std::string> totalFiles = database->getAllFiles();
        
    ClangTool Tool(*database, totalFiles);
    Tool.setDiagnosticConsumer(new WarningDiagConsumer);

    initialize(totalFiles);

    Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());

    finish();

    return 0;
}
