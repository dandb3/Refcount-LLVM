#include "ClangRefcount.h"
#include "RefcountMain.h"

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
    const clang::FieldDecl *node = Result.Nodes.getNodeAs<clang::FieldDecl>("fieldType");

    if (node == nullptr) {
        return;
    }

    const auto &SM = *Result.SourceManager;
    const auto &loc = node->getBeginLoc();
    const auto &filename = SM.getFilename(SM.getSpellingLoc(loc)).str();
    
    const RecordDecl *tmp = dyn_cast<RecordDecl>(node->getLexicalDeclContext());
    const RecordDecl *parent;

    // if (isFilesStruct) {
    //     *GS.filesStructLog << "File: " << filename << ":" << SM.getExpansionLineNumber(loc) << "\n";
    // }
    // llvm::outs() << "File: " << SM.getFilename(SM.getLocForStartOfFile(SM.getMainFileID())) << "\n";
    
    do {
        parent = tmp;
        // llvm::outs() << "cur: " << parent->getName() << "\n";
        if (parent->getName() != "")
            break;
        // llvm::outs() << "parent: " << parent->getParent()->getDeclKindName() << "\n";
    } while((tmp = dyn_cast<RecordDecl>(parent->getLexicalDeclContext())));

    // llvm::outs() << "Struct: " << parent->getName() << "\n";
    // llvm::outs() << "    " << node->getType().getAsString() << " " << node->getName() << "\n";

    std::string name = parent->getNameAsString();

    if (name == "") {
        const DeclContext *top = parent->getLexicalDeclContext();
        bool typedefFound = false;

        for (auto it = top->decls_begin(); it != top->decls_end(); ++it) {
            const RecordDecl *cur = dyn_cast<RecordDecl>(*it);
            if (cur == nullptr)
                continue;

            if (cur == parent) {
                // llvm::outs() << "cur == parent\n";
                // llvm::outs() << "\n";
                ++it;
                if (it == top->decls_end())
                    break;
                const TypedefDecl *TD = dyn_cast<TypedefDecl>(*it);
                if (TD == nullptr)
                    break;
                const ElaboratedType *ET = dyn_cast<ElaboratedType>(TD->getUnderlyingType().getTypePtr());
                if (ET == nullptr)
                    break;
                const RecordType *RT = dyn_cast<RecordType>(ET->getNamedType());
                if (RT == nullptr)
                    break;
                const RecordDecl *RD = RT->getDecl();
                if (RD == parent) {
                    name = TD->getNameAsString();
                    typedefFound = true;
                    // llvm::outs() << "struct: " << name << "\n";
                    break;
                }
                break;
            }
        }

        if (!typedefFound) {
            // auto result = LS.anonymousSet.insert({filename, SM.getExpansionLineNumber(loc)});
            // if (!result.second) {
            //     llvm::errs() << "ERROR occured in run() - " << filename << ":" << SM.getExpansionLineNumber(loc) << "\n";
            //     exit(1);
            // }
            *GS.clangAnonymousStructLog << "pos: " << filename << ":" << SM.getExpansionLineNumber(loc) << "\n";
            *GS.clangAnonymousStructLog << "name: " << name << "\n";
            parent->print(*GS.clangAnonymousStructLog);
            *GS.clangAnonymousStructLog << "\n";

            return;
        }
    }

    // if (isFilesStruct) {
    //     *GS.filesStructLog << "name: " << name << "\n";
    // }
    LS.clangStructNames[name] += 1;
    // if (!LS.clangStructNames.insert(name).second) {
        // if (LS.dupStructNames.insert(name).second) {
        //     // There might be multiple structs with duplicated name...
        //     *GS.dupStructNameLog << "DUPLICATED NAME FOUND!\n";
        //     *GS.dupStructNameLog << "pos: " << filename << ":" << SM.getExpansionLineNumber(loc) << "\n";
        //     *GS.dupStructNameLog << "name: " << name << "\n";
        // }
    // }
}

FieldTypeASTConsumer::FieldTypeASTConsumer(clang::Preprocessor& PP) {
    
    // Here we add all of the checks that should be run
    // when the AST is traversed by using Matcher.addMatcher

    // At the moment, there are no checks registered

    // PP.addPPCallbacks(std::make_unique<clang::PPCallbacks>());
    // PP.addPPCallbacks(std::make_unique<FieldTypePPCallbacks>());

    auto *callback = new FieldTypeCallback;

    Matcher.addMatcher(
        fieldDecl(
            hasType(namedDecl(hasAnyName(
                "atomic_t",
                "atomic_long_t",
                "atomic64_t",
                "refcount_t",
                "kref",
                "refcount_struct"
            ))),
            unless(hasParent(recordDecl(hasAnyName(
                "kref",
                "refcount_struct"
            ))))
        ).bind("fieldType"),
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
    // llvm::outs() << "LEARN\n";
    return false;
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
    // LS.print();
    // llvm::outs() << "\n";

    // LS.print();
    LS.compare();
    // LS.print();
    *GS.compareLog << "cpath: " << cPath << ", bcpath: " << bcPath << "\n";
    *GS.compareLog << "<Clang>\n";
    for (auto &elem : LS.clangStructNames) {
        *GS.compareLog << "    " << elem.first << "\n";
    }
    *GS.compareLog << "<LLVM>\n";
    for (auto &elem : LS.llvmStructNames) {
        *GS.compareLog << "    " << elem.first << "\n";
    }
    *GS.compareLog << "\n";
    
    LS.clear();
    // isFilesStruct = false;
    // llvm::outs() << "LLVM pass finished\n";
}
