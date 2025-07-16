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

#define TARGET_DIR "/home/junwoong/linux/linux-current/"
#define LOG_DIR "/home/junwoong/work/refcount/build_refcount_id_thread/log/"
#define COMPILE_DATABASE LOG_DIR "compile_commands.json"

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

#include "../include/MultiThread.h"

#include <set>

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

enum RefcountType {
    REF_ATOMIC_T,
    REF_ATOMIC_LONG_T,
    REF_ATOMIC64_T,
    REF_REFCOUNT_T,
    REF_KREF,
    REF_REFCOUNT_STRUCT,
    REF_ERROR
};

class ID {
    public:
    std::string filename;
    unsigned int line;
    unsigned int col;

    ID(const SourceManager &SM, const std::string &filename, const SourceLocation &loc)
    : filename(filename), line(SM.getExpansionLineNumber(loc)), col(SM.getExpansionColumnNumber(loc)) {}

    // void print(mult::ostream &os) const {
    //     std::stringstream ss;
    //     ss << filename << ":" << line << ":" << col << "\n";
    //     os << ss.str();
    // }

    // void print(mult::ostream &os, const std::string funcName) const {
    //     std::stringstream ss;
    //     ss << filename << ":" << line << ":" << col << ":" << funcName << "\n";
    //     os << ss.str();
    // }

    void print(std::stringstream &ss) const {
        ss << filename << ":" << line << ":" << col << "\n";
    }

    void print(std::stringstream &ss, const std::string funcName) const {
        ss << filename << ":" << line << ":" << col << ":" << funcName << "\n";
    }

    bool operator<(const ID &id) const {
        return std::tie(filename, line, col) < std::tie(id.filename, id.line, id.col);
    }
};

typedef ID RefcntKey;
typedef std::string RefcntVal;
typedef std::map<RefcntKey, RefcntVal> RefcntMap;

size_t fileNum;

// logs
mult::ostream resultLog;
mult::ostream sameKeyDiffTypeErr;

RefcntMap result;
std::mutex resultMtx;

class FieldTypeCallback : public MatchFinder::MatchCallback {
    public:
    static size_t fileNo;
    static std::mutex fileNoMtx;

    virtual void onStartOfTranslationUnit() override {
        llvm::outs() << "Collecting refcount candidates...\n";
        llvm::outs() << "[";
        fileNoMtx.lock();
        llvm::outs() << ++fileNo;
        fileNoMtx.unlock();
        llvm::outs() << "/" << fileNum << "]\n";
    }

    virtual void onEndOfTranslationUnit() override {}

    virtual void run(const MatchFinder::MatchResult& Result) override {
        const clang::FieldDecl *FD = Result.Nodes.getNodeAs<clang::FieldDecl>("field");
        const clang::QualType *QT = Result.Nodes.getNodeAs<clang::QualType>("type");

        if (FD == nullptr || QT == nullptr) {
            return;
        }

        const auto &SM = *Result.SourceManager;
        const auto &fieldLoc = SM.getExpansionLoc(FD->getLocation());
        const auto &fieldLocBegin = SM.getExpansionLoc(FD->getBeginLoc());
        // const auto &filename = SM.getFilename(SM.getSpellingLoc(fieldLoc)).str();

        const FileEntry *FE = SM.getFileEntryForID(SM.getFileID(SM.getSpellingLoc(fieldLocBegin)));
        const auto &filename = FE->tryGetRealPathName().str().substr(sizeof(TARGET_DIR) - 1);

        ID key(SM, filename, fieldLoc);
        const std::string &typeName = QT->getAsString();

        resultMtx.lock();
        auto res = result.insert({ key, typeName });
        resultMtx.unlock();
        if (!res.second && res.first->second != typeName) {
            // LOG
            std::stringstream ss;
            ss << res.first->second << "\n";
            key.print(ss);
            sameKeyDiffTypeErr << ss;
        }
    }
};

size_t FieldTypeCallback::fileNo = 0;
std::mutex FieldTypeCallback::fileNoMtx;

class FieldTypeASTConsumer : public ASTConsumer {

    public:
    FieldTypeASTConsumer(clang::Preprocessor& PP) {

        auto *callback = new FieldTypeCallback;
        Matcher.addMatcher(
            fieldDecl(
                hasType(
                    qualType(
                        hasCanonicalType(
                            recordType(
                                hasDeclaration(
                                    recordDecl(
                                        isDefinition(),
                                        hasAnyName(
                                            "atomic_t",
                                            "atomic_long_t",
                                            "atomic64_t",
                                            // "refcount_t",
                                            "kref",
                                            "refcount_struct"
                                        )
                                    )
                                )
                            )
                        )
                    ).bind("type")
                ),
                unless(hasParent(recordDecl(hasAnyName(
                    "kref",
                    "refcount_struct"
                ))))
            ).bind("field"),
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
        const auto &SM = CI.getSourceManager();
        const clang::FileEntry *FE = SM.getFileEntryForID(SM.getMainFileID());
        llvm::outs() << "file: " << FE->getName() << "\n";
        return true;
    }

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
            CompilerInstance& CI, StringRef file) override {
        return std::unique_ptr<ASTConsumer>(
                new FieldTypeASTConsumer(CI.getPreprocessor()));
    }

    virtual void EndSourceFileAction() override {}
};

bool filepathAccessible(std::string &path)
{   
    std::ifstream file(path);
    return file.is_open();
}

void initialize(std::error_code &ecode) {
    if (!resultLog.open(LOG_DIR "result.stat.thread")) {
        llvm::errs() << "log file creation failed!\n";
        exit(1);
    }
    
    if (!sameKeyDiffTypeErr.open(LOG_DIR "same_key_diff_type.err.thread")) {
        llvm::errs() << "log file creation failed!\n";
        exit(1);
    }
}

void finish() {

}

void execute(const clang::tooling::JSONCompilationDatabase *database, const std::string &filename) {
    std::vector<std::string> file(1, filename);
    ClangTool Tool(*database, file);
    Tool.setDiagnosticConsumer(new WarningDiagConsumer);
    Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());
}

int main(int argc, const char** argv)
{
    std::error_code ecode;
    initialize(ecode);

    if (argc > 1) {
        auto OptionsParser = CommonOptionsParser::create(argc, argv, refcntCategory, cl::ZeroOrMore);
        if (auto err = OptionsParser.takeError()) {
            llvm::errs() << std::move(err);
            return EXIT_FAILURE;
        }
    
        auto files = OptionsParser->getSourcePathList();
        if (files.empty()) {
            llvm::errs() << "Error: No input files specified\n";
            return EXIT_FAILURE;
        }
        
        for (auto path : files) {
            if (!filepathAccessible(path)) {
                llvm::errs() << "Unable to access file '" << path << "'\n";
                return EXIT_FAILURE;
            }
        }

        ClangTool Tool(OptionsParser->getCompilations(), files);
        Tool.setDiagnosticConsumer(new WarningDiagConsumer);
        fileNum = argc - 1;

        Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());
    }
    // filenames are in compile_commands.json
    else {
        std::string err_msg;
        const auto database = clang::tooling::JSONCompilationDatabase::loadFromFile(COMPILE_DATABASE, err_msg, JSONCommandLineSyntax::AutoDetect);
        if (database == nullptr) {
            llvm::errs() << "JSON file parse failed\n";
            return 1;
        }

        std::vector<std::string> totalFiles = database->getAllFiles();
        fileNum = totalFiles.size();
        
        for (std::string& path : totalFiles) {
            if (!filepathAccessible(path)) {
                llvm::errs() << "Unable to access file '" << path << "'\n";
            }
        }

        mult::ThreadPool pool(16);
        
        for (size_t i = 0; i < totalFiles.size(); ++i) {
            pool.enqueue(execute, database.get(), totalFiles[i]);
        }
    }

    std::stringstream ss;
    for (auto &elem : result) {
        ss << elem.second << "\n";
        elem.first.print(ss);
    }
    resultLog << ss;

    finish();

    return EXIT_SUCCESS;
}
