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

#define TARGET_DIR "/home/jdoh/Desktop/work/linux-v5.6/"
#define LOG_DIR "/home/jdoh/Desktop/work/Refcount-LLVM/build_find_refcount_ops/log/"
#define COMPILE_DATABASE LOG_DIR "compile_commands.json"

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

#include "llvm/Support/raw_ostream.h"

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

enum APIType {
    SET,
    DIFF,
    VAR,
    ERROR
};

enum APIArgType {
    REF_ONLY, // ex) atomic_inc(atomic_t *v);
    REF_VAL,  // ex) atomic_set(atomic_t *v, int i);
    VAL_REF   // ex) atomic_add(int i, atomic_t *v);
};

class ID {
    public:
    std::string filename;
    unsigned int line;
    unsigned int col;

    ID(const std::string &filename, unsigned int line, unsigned int col)
    : filename(filename), line(line), col(col) {}

    void print(llvm::raw_fd_ostream &os) const {
        os << filename << ":" << line << ":" << col << "\n";
    }

    void print(llvm::raw_fd_ostream &os, const std::string funcName) const {
        os << filename << ":" << line << ":" << col << ":" << funcName << "\n";
    }

    bool operator<(const ID &id) const {
        return std::tie(filename, line, col) < std::tie(id.filename, id.line, id.col);
    }
};

size_t fileNum;

// logs
llvm::raw_fd_ostream *resultLog;

std::map<std::pair<ID, ID>, std::string> refcount_ops;

class ArgTypeCallback : public MatchFinder::MatchCallback {
    private:
    static size_t fileNo;

    public:
    virtual void onStartOfTranslationUnit() override {
        llvm::outs() << "Collecting refcount operations...\n";
        llvm::outs() << "[" << ++fileNo << "/" << fileNum << "]\n";
    }

    virtual void onEndOfTranslationUnit() override {}

    virtual void run(const MatchFinder::MatchResult& Result) override {
        const clang::FunctionDecl *node = Result.Nodes.getNodeAs<clang::FunctionDecl>("refcount-ops");

        if (node == nullptr) {
            llvm::errs() << "not refcount op!\n";
            return;
        }

        const auto &SM = *Result.SourceManager;

        SourceLocation Loc = node->getLocation();

        SourceLocation BLoc = node->getBeginLoc();

        const FileEntry *SFE = SM.getFileEntryForID(SM.getFileID(SM.getSpellingLoc(BLoc)));
        const FileEntry *EFE = SM.getFileEntryForID(SM.getFileID(SM.getExpansionLoc(BLoc)));

        const auto &SFilename = SFE->tryGetRealPathName().str().substr(sizeof(TARGET_DIR) - 1);
        const auto &EFilename = EFE->tryGetRealPathName().str().substr(sizeof(TARGET_DIR) - 1);

        // for (unsigned long i = 0; i < size; ++i) {
        ID SKey(SFilename, SM.getSpellingLineNumber(Loc), SM.getSpellingColumnNumber(Loc));
        ID EKey(EFilename, SM.getExpansionLineNumber(Loc), SM.getExpansionColumnNumber(Loc));
        
        auto result = refcount_ops.insert({ { SKey, EKey }, node->getNameAsString() });
        if (result.second) {
            *resultLog << result.first->second << "\n";
            *resultLog << node->getType().getAsString() << "\n";
            result.first->first.first.print(*resultLog);
            result.first->first.second.print(*resultLog);
            resultLog->flush();
        }
    }
};

size_t ArgTypeCallback::fileNo = 0;

class ArgTypeASTConsumer : public ASTConsumer {

    public:
    ArgTypeASTConsumer(clang::Preprocessor& PP) {
        auto *callback = new ArgTypeCallback;

        Matcher.addMatcher(
            functionDecl(
                isDefinition(),
                matchesName("^::(kref_|atomic_|atomic_long_|atomic64_|refcount_)")
            ).bind("refcount-ops"),
            callback
        );
    }

    virtual void HandleTranslationUnit(ASTContext& Context) override {
        Matcher.matchAST(Context);
    }

    private:
    MatchFinder Matcher;
};

class ArgTypeFrontEndAction : public ASTFrontendAction {

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
                new ArgTypeASTConsumer(CI.getPreprocessor()));
    }

    virtual void EndSourceFileAction() override {}
};

bool filepathAccessible(std::string &path)
{   
    std::ifstream file(path);
    return file.is_open();
}

void initialize(std::error_code &ecode) {
    resultLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "start-refcount-ops-raw.stat"), ecode);

    if (resultLog == nullptr) {
        llvm::errs() << "log file creation failed!\n";
        exit(1);
    }
}

void finish() {
    delete resultLog;
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

        Tool.run(newFrontendActionFactory<ArgTypeFrontEndAction>().get());
    }
    // filenames are in compile_commands.json
    else {
        std::string err_msg;
        auto database = clang::tooling::JSONCompilationDatabase::loadFromFile(COMPILE_DATABASE, err_msg, JSONCommandLineSyntax::AutoDetect);
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
        
        for (size_t i = 0; i < totalFiles.size(); ++i) {
            std::vector<std::string> file(1, totalFiles[i]);
            ClangTool Tool(*database, file);
            Tool.setDiagnosticConsumer(new WarningDiagConsumer);
        
            Tool.run(newFrontendActionFactory<ArgTypeFrontEndAction>().get());
        }
    }

    finish();

    return EXIT_SUCCESS;
}
