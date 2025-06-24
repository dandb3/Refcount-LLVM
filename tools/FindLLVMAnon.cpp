#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/JSONCompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "llvm/Support/CommandLine.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stddef.h>

// #define TEST_DIR "new"
#define LOG_DIR "/home/junwoong/work/refcount/build_test/log/"
#define COMPILE_DATABASE LOG_DIR "compile_commands.json"

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

static cl::OptionCategory refcntCategory("refcnt options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

static cl::opt<bool> verbose("verbose",
    cl::desc(R"(Generate verbose output)"),     // the description
    cl::init(false),                            // the initial value of the option
    cl::cat(refcntCategory)                   // what category this belongs to
);

class WarningDiagConsumer : public DiagnosticConsumer {

    public:
    virtual void HandleDiagnostic(
            DiagnosticsEngine::Level Level, const Diagnostic& Info) override {
        // simply do nothing
    }
};

static std::set<std::pair<std::string, unsigned int>> llvmAnonSet;
static std::set<std::string> globalWorkingFiles;

size_t fileNum;
size_t fileNo;

llvm::raw_fd_ostream *llvmAnonLog;

// static bool checkType(const clang::RecordDecl *node) {
//     size_t size;
//     auto it = node->field_begin();
//     auto tmpIt = it;


//     for (size = 0; tmpIt != node->field_end(); ++tmpIt)
//         ++size;

//     if (size != 7) {
//         return false;
//     }
    
//     tmpIt = it;
//     for (int i = 0; i < 3; ++i) {
//         ++tmpIt;
//     }

//     for (int i = 0; i < 3; ++i, ++tmpIt) {
//         if ((*tmpIt)->getType().getAsString() != "atomic_t") {
//             return false;
//         }
//     }

//     return true;
// }

// static bool checkType(const clang::RecordDecl *node) {
//     size_t size;
//     auto it = node->field_begin();
//     auto tmpIt = it;


//     for (size = 0; tmpIt != node->field_end(); ++tmpIt)
//         ++size;

//     if (size != 21) {
//         return false;
//     }
    
//     tmpIt = it;

//     for (int i = 0; i < 21; ++i, ++tmpIt) {
//         if ((*tmpIt)->getType().getAsString() != "atomic_t") {
//             return false;
//         }
//     }

//     return true;
// }

// static bool checkType(const clang::RecordDecl *node) {
//     int size;
//     auto it = node->field_begin();
//     auto tmpIt = it;


//     for (size = 0; tmpIt != node->field_end(); ++tmpIt)
//         ++size;

//     if (size < 8) {
//         return false;
//     }
    
//     tmpIt = it;
//     for (int i = 0; i < size - 1; ++i) {
//         ++tmpIt;
//     }

//     if ((*tmpIt)->getType().getAsString() != "refcount_t") {
//         return false;
//     }

//     return true;
// }

// static bool checkType(const clang::RecordDecl *node) {
//     int size;
//     auto it = node->field_begin();
//     auto tmpIt = it;


//     for (size = 0; tmpIt != node->field_end(); ++tmpIt)
//         ++size;

//     if (size != 3) {
//         return false;
//     }
    
//     tmpIt = it;
//     for (int i = 0; i < size - 1; ++i) {
//         ++tmpIt;
//     }

//     if ((*tmpIt)->getType().getAsString() != "atomic_t") {
//         return false;
//     }

//     return true;
// }

// static bool checkType(const clang::RecordDecl *node) {
//     int size;
//     auto it = node->field_begin();
//     auto tmpIt = it;


//     for (size = 0; tmpIt != node->field_end(); ++tmpIt)
//         ++size;

//     if (size != 6) {
//         return false;
//     }
    
//     tmpIt = it;

//     if ((*tmpIt)->getType().getAsString() != "cpumask_var_t") {
//         return false;
//     }

//     ++tmpIt;
//     if ((*tmpIt)->getType().getAsString() != "atomic_t") {
//         return false;
//     }

//     return true;
// }

// static bool checkType(const clang::RecordDecl *node) {
//     int size;
//     auto it = node->field_begin();
//     auto tmpIt = it;


//     for (size = 0; tmpIt != node->field_end(); ++tmpIt)
//         ++size;

//     if (size != 7) {
//         return false;
//     }
    
//     tmpIt = it;
//     for (int i = 0; i < 2; ++i) {
//         ++tmpIt;
//     }

//     if ((*tmpIt)->getType().getAsString() != "atomic_t") {
//         return false;
//     }
//     ++tmpIt;
//     if ((*tmpIt)->getType().getAsString() != "atomic_t") {
//         return false;
//     }

//     return true;
// }

static bool checkType(const clang::RecordDecl *node) {
    int size;
    auto it = node->field_begin();
    auto tmpIt = it;


    for (size = 0; tmpIt != node->field_end(); ++tmpIt)
        ++size;

    if (size != 1) {
        return false;
    }
    
    tmpIt = it;

    if ((*tmpIt)->getType().getAsString() != "atomic_t") {
        return false;
    }

    return true;
}

class FieldTypeCallback : public MatchFinder::MatchCallback {
    private:
    std::set<std::string> workingFiles;

    public:
    virtual void onStartOfTranslationUnit() override {}

    virtual void onEndOfTranslationUnit() override {
        globalWorkingFiles.insert(workingFiles.begin(), workingFiles.end());
        workingFiles.clear();
    }

    virtual void run(const MatchFinder::MatchResult& Result) override {
        const clang::RecordDecl *node = Result.Nodes.getNodeAs<clang::RecordDecl>("fieldType");

        if (node == nullptr) {
            return;
        }

        const auto &SM = *Result.SourceManager;
        const auto &loc = node->getBeginLoc();
        const auto &filename = SM.getFilename(SM.getSpellingLoc(loc)).str();

        if (globalWorkingFiles.find(filename) != globalWorkingFiles.end()) {
            return;
        }

        workingFiles.insert(filename);
        
        // llvm::outs() << "File: " << filename << ":" << SM.getExpansionLineNumber(loc) << "\n";

        if (checkType(node)) {
            *llvmAnonLog << "File: " << filename << ":" << SM.getExpansionLineNumber(loc) << "\n";
        }
    }
};

class FieldTypeASTConsumer : public ASTConsumer {

    public:
    FieldTypeASTConsumer(clang::Preprocessor& PP) {
        auto *callback = new FieldTypeCallback;

        Matcher.addMatcher(
            recordDecl(has(
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
                )
            )).bind("fieldType"),
            callback
        );
    }

    virtual void HandleTranslationUnit(ASTContext& Context) override {
        Matcher.matchAST(Context);
    }

    private:
    MatchFinder Matcher;
};

class FieldTypeFrontEndAction : public ASTFrontendAction {

    public:
    virtual bool BeginSourceFileAction(CompilerInstance &CI) override {
        llvm::outs() << "[" << ++fileNo << "/" << fileNum << "]\n";
        return true;
    }

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
            CompilerInstance& CI, StringRef file) override {
        return std::unique_ptr<ASTConsumer>(
                new FieldTypeASTConsumer(CI.getPreprocessor()));
    }

    virtual void EndSourceFileAction() override {}
};

void initialize(std::vector<std::string> &totalFiles) {
    std::error_code error_code;
    llvmAnonLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "llvm_anon.log"), error_code);
    fileNum = totalFiles.size();
}

void finish() {
    delete llvmAnonLog;
}

int main(int argc, const char** argv)
{
    llvm_shutdown_obj SDO;
    
    if (argc > 1) {
        auto OptionsParser = CommonOptionsParser::create(argc, argv, refcntCategory, cl::ZeroOrMore);
        if (auto err = OptionsParser.takeError()) {
            llvm::errs() << std::move(err);
            return EXIT_FAILURE;
        }
    
        auto totalFiles = OptionsParser->getSourcePathList();

        initialize(totalFiles);

        ClangTool Tool(OptionsParser->getCompilations(), totalFiles);
        Tool.setDiagnosticConsumer(new WarningDiagConsumer);
        Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());

        finish();
    }
    else {
        std::string err_msg;
        auto database = clang::tooling::JSONCompilationDatabase::loadFromFile(COMPILE_DATABASE, err_msg, JSONCommandLineSyntax::AutoDetect);
        if (database == nullptr) {
            llvm::errs() << "JSON file parse failed\n";
            return 1;
        }
        
        std::vector<std::string> totalFiles = database->getAllFiles();

        initialize(totalFiles);

        for (size_t i = 0; i < totalFiles.size(); ++i) {
            std::vector<std::string> file(1, totalFiles[i]);

            ClangTool Tool(*database, file);
            Tool.setDiagnosticConsumer(new WarningDiagConsumer);
            Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());
        }

        finish();
    }
    return 0;
}