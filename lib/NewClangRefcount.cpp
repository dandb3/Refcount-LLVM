#include "NewClangRefcount.h"
#include "NewRefcountMain.h"

#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/SourceMgr.h"

// bool isFilesStruct;

void FieldTypePPCallbacks::InclusionDirective(SourceLocation HashLoc,
    const Token &IncludeTok,
    StringRef FileName,
    bool IsAngled,
    CharSourceRange FilenameRange,
    const FileEntry *File,
    StringRef SearchPath,
    StringRef RelativePath,
    const clang::Module *Imported,
    SrcMgr::CharacteristicKind FileType) {
    // if (isFilesStruct) {
    //     *GS.filesStructIncludeLog << "Include: " << FileName << "\n";
    // }
}

void FieldTypeCallback::onStartOfTranslationUnit() {}

void FieldTypeCallback::onEndOfTranslationUnit() {}

void FieldTypeCallback::run(const MatchFinder::MatchResult& Result) {
    const clang::RecordDecl *node = Result.Nodes.getNodeAs<clang::RecordDecl>("field");

    if (node == nullptr) {
        return;
    }

    const auto &SM = *Result.SourceManager;
    const auto &loc = node->getBeginLoc();
    // const auto &filename = SM.getFilename(SM.getSpellingLoc(loc)).str();

    const FileEntry *FE = SM.getFileEntryForID(SM.getFileID(SM.getSpellingLoc(loc)));
    const auto &filename = FE->tryGetRealPathName().str();
    // llvm::outs() << "Struct: " << parent->getName() << "\n";
    // llvm::outs() << "    " << node->getType().getAsString() << " " << node->getName() << "\n";

    /**
     * 1.   find name of struct
     * 2.   if found => check if "name of struct" is unique
     * 2-1. if not unique => log and pass
     * 2-2. if unique => construct tree of the struct
     * 3.   insert tree to map
     */

    std::string name = RefClangLib::getTypeName(node);

    if (name == "") {
        // LOG if it is not top level struct
        return;
    }

    if (LS.clangRefcountTrees.find(name) != LS.clangRefcountTrees.end()) {
        // LOG
        return;
    }

    RefcountNode *root = RefcountNode::makeTree(node);
    if (root == nullptr) {
        // LOG
        return;
    }

    if (!root->hasRefcountField()) {
        // LOG
        delete root;
        return;
    }

    LS.clangRefcountTrees[name] = root;
}

FieldTypeASTConsumer::FieldTypeASTConsumer(clang::Preprocessor& PP) {
    
    // Here we add all of the checks that should be run
    // when the AST is traversed by using Matcher.addMatcher

    // At the moment, there are no checks registered

    // PP.addPPCallbacks(std::make_unique<clang::PPCallbacks>());
    // PP.addPPCallbacks(std::make_unique<FieldTypePPCallbacks>());

    auto *callback = new FieldTypeCallback;

    Matcher.addMatcher(
        recordDecl(
            hasDescendant(
                fieldDecl(
                    hasType(namedDecl(hasAnyName(
                        "atomic_t",
                        "atomic_long_t",
                        "atomic64_t",
                        "refcount_t",
                        "kref",
                        "refcount_struct",
                        "snd_use_lock_t"
                    )))
                )
            ),
            unless(hasAnyName(
                "kref",
                "refcount_struct"
            ))
        ).bind("field"),
        callback
    );
}

void FieldTypeASTConsumer::HandleTranslationUnit(ASTContext& Context) {
    Matcher.matchAST(Context);
}

bool FieldTypeFrontEndAction::BeginSourceFileAction(CompilerInstance &CI) {
    const auto &SM = CI.getSourceManager();
    
    cPath = SM.getFileEntryForID(SM.getMainFileID())->tryGetRealPathName().str();
    llvm::outs() << "[" << ++GS.fileNo << "/" << GS.fileNum << "]\n";
    llvm::outs() << "File: " << cPath << "\n";

    if (cPath.size() >= 2 && cPath.compare(cPath.size() - 2, 2, ".c") == 0) {
        bcPath = cPath.substr(0, cPath.size() - 2) + ".bc";

        std::ifstream cFile(cPath);
        std::ifstream bcFile(bcPath);

        if (!cFile.is_open() || !bcFile.is_open()) {
            llvm::outs() << "cpath: " << cPath << ", bcpath: " << bcPath << "\n";
            *GS.fileAccessLog << "cpath: " << cPath << ", bcpath: " << bcPath << "\n";
            return false;
        }

        // if (cPath == "/home/junwoong/linux/linux-current-v6.6/drivers/hid/bpf/hid_bpf_jmp_table.c"
        //     || cPath == "/home/junwoong/linux/linux-current-v6.6/drivers/hid/bpf/hid_bpf_dispatch.c") {
        //     isFilesStruct = true;
        //     *GS.filesStructLog << "Path: " << cPath << "\n";
        //     *GS.filesStructIncludeLog<< "Path: " << cPath << "\n";
        // }
        return true;
    }
    *GS.fileAccessLog << "cpath: " << cPath << "\n";
    llvm::outs() << "LEARN\n";
    return false;
    // return true;
}

std::unique_ptr<ASTConsumer> FieldTypeFrontEndAction::CreateASTConsumer(
    CompilerInstance& CI, StringRef file) {

    return std::unique_ptr<ASTConsumer>(
            new FieldTypeASTConsumer(CI.getPreprocessor()));
}

void FieldTypeFrontEndAction::EndSourceFileAction() {
    SMDiagnostic Err;
    LLVMContext Ctx;

    // llvm::outs() << "Clang libtooling finished\n";
    std::unique_ptr<llvm::Module> M = parseIRFile(bcPath, Err, Ctx);
    if (!M) {
        llvm::errs() << "ERROR: reading bitcode file: " << bcPath << "\n";
        exit(1);
    }

    ModulePassManager MPM;
    MPM.addPass(Refcount());

    ModuleAnalysisManager MAM;
    PassBuilder PB;
    PB.registerModuleAnalyses(MAM);
    MPM.run(*M, MAM);

    // compare clang & llvm

    // llvm::outs() << "cpath: " << cPath << ", bcpath: " << bcPath << "\n";

    LS.print();
    // LS.compare();
    // LS.print();
    // *GS.compareLog << "cpath: " << cPath << ", bcpath: " << bcPath << "\n";
    // *GS.compareLog << "<Clang>\n";
    // for (auto &elem : LS.clangStructNames) {
    //     *GS.compareLog << "    " << elem.first << "\n";
    // }
    // *GS.compareLog << "<LLVM>\n";
    // for (auto &elem : LS.llvmStructNames) {
    //     *GS.compareLog << "    " << elem.first << "\n";
    // }
    // *GS.compareLog << "\n";
    
    LS.clear();
    // isFilesStruct = false;
    // llvm::outs() << "LLVM pass finished\n";
}
