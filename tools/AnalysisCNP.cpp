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
#define LOG_DIR "/home/junwoong/work/refcount/build_analysisCNP/log/"
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
    ERROR
};

enum APIArgType {
    REF_ONLY, // ex) atomic_inc(atomic_t *v);
    REF_VAL,  // ex) atomic_set(atomic_t *v, int i);
    VAL_REF   // ex) atomic_add(int i, atomic_t *v);
};

enum RefcountType {
    REF_ATOMIC_T,
    REF_ATOMIC_LONG_T,
    REF_ATOMIC64_T,
    REF_REFCOUNT_T,
    REF_KREF,
    REF_ERROR
};

static std::set<std::pair<std::string, unsigned int>> anonymousSet;
static std::set<std::string> globalWorkingFiles;

size_t fileNum;

llvm::raw_fd_ostream *anonymousLog;

std::map<std::pair<std::string, unsigned int>, RefcountType> result;

static RefcountType getRefcountType(const std::string &type) {
    if (type == "atomic_t") {
        return REF_ATOMIC_T;
    }
    else if (type == "atomic_long_t") {
        return REF_ATOMIC_LONG_T;
    }
    else if (type == "atomic64_t") {
        return REF_ATOMIC64_T;
    }
    else if (type == "refcount_t") {
        return REF_REFCOUNT_T;
    }
    else if (type == "struct kref") {
        return REF_KREF;
    }
    else {
        llvm::errs() << "UNKNOWN STRUCT NAME FOUND!\n";
        llvm::errs() << "Name: " << type << "\n";
        return REF_ERROR;
    }
}

class FieldTypeCallback : public MatchFinder::MatchCallback {
    public:
    static size_t fileNo;
    std::set<std::string> workingFiles;

    virtual void onStartOfTranslationUnit() override {
        llvm::outs() << "[" << ++fileNo << "/" << fileNum << "]\n";
    }

    virtual void onEndOfTranslationUnit() override {
        globalWorkingFiles.insert(workingFiles.begin(), workingFiles.end());
        workingFiles.clear();
    }

    virtual void run(const MatchFinder::MatchResult& Result) override {
        const clang::FieldDecl *node = Result.Nodes.getNodeAs<clang::FieldDecl>("field");

        if (node == nullptr) {
            return;
        }

        const auto &SM = *Result.SourceManager;
        const auto &fieldLoc = node->getBeginLoc();
        // const auto &filename = SM.getFilename(SM.getSpellingLoc(fieldLoc)).str();

        const FileEntry *FE = SM.getFileEntryForID(SM.getFileID(SM.getSpellingLoc(fieldLoc)));
        const auto &filename = FE->tryGetRealPathName().str();



        result.insert({ { filename, SM.getExpansionLineNumber(fieldLoc) }, getRefcountType(node->getType().getAsString()) });

        // if (globalWorkingFiles.find(filename) != globalWorkingFiles.end()) {
        //     return;
        // }

        // workingFiles.insert(filename);
        
        // const RecordDecl *tmp = dyn_cast<RecordDecl>(node->getLexicalDeclContext());
        // const RecordDecl *parent;
        
        // do {
        //     parent = tmp;
        // } while((tmp = dyn_cast<RecordDecl>(parent->getLexicalDeclContext())));

        // const auto &parentLoc = parent->getBeginLoc();

        // auto &stat = result[{filename, SM.getExpansionLineNumber(parentLoc)}];
        // std::string type = node->getType().getAsString();

        // ++stat.res[type];
    }
};

size_t FieldTypeCallback::fileNo = 0;

class FieldTypeASTConsumer : public ASTConsumer {

    public:
    FieldTypeASTConsumer(clang::Preprocessor& PP) {

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
        return true;
    }

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
            CompilerInstance& CI, StringRef file) override {
        return std::unique_ptr<ASTConsumer>(
                new FieldTypeASTConsumer(CI.getPreprocessor()));
    }

    virtual void EndSourceFileAction() override {
        
    }
};

bool filepathAccessible(std::string &path)
{   
    std::ifstream file(path);
    return file.is_open();
}

static std::set<std::pair<std::string, unsigned int>> manualCase;

static void initManualCase() {
    for (int i = 331; i <= 333; ++i) {
        manualCase.insert({
            "/home/junwoong/linux/linux-current-v6.6/include/linux/mm_types.h",
            i
        });
    }
    for (int i = 1148; i <= 1168; ++i) {
        manualCase.insert({
            "/home/junwoong/linux/linux-current-v6.6/fs/smb/client/cifsglob.h",
            i
        });
    }
    manualCase.insert({
        "/home/junwoong/linux/linux-current-v6.6/include/linux/mroute_base.h",
        154
    });
    manualCase.insert({
        "/home/junwoong/linux/linux-current-v6.6/drivers/gpu/drm/msm/disp/dpu1/dpu_hw_interrupts.h",
        63
    });
    manualCase.insert({
        "/home/junwoong/linux/linux-current-v6.6/kernel/sched/fair.c",
        6684
    });
    manualCase.insert({
        "/home/junwoong/linux/linux-current-v6.6/drivers/net/ethernet/google/gve/gve.h",
        477
    });
    manualCase.insert({
        "/home/junwoong/linux/linux-current-v6.6/drivers/net/ethernet/google/gve/gve.h",
        480
    });
    manualCase.insert({
        "/home/junwoong/linux/linux-current-v6.6/drivers/net/ethernet/google/gve/gve.h",
        503
    });
    manualCase.insert({
        "/home/junwoong/linux/linux-current-v6.6/drivers/net/ethernet/google/gve/gve.h",
        508
    });
    manualCase.insert({
        "/home/junwoong/linux/linux-current-v6.6/include/net/inetpeer.h",
        51
    });
    manualCase.insert({
        "/home/junwoong/linux/linux-current-v6.6/include/net/nexthop.h",
        104
    });
}

static void checkManualCase(const std::pair<std::string, unsigned int> &p) {
    manualCase.erase(p);
}

static void resultManualCase() {
    llvm::outs() << "-----RESULT-----\n";
    for (auto &elem : manualCase) {
        llvm::outs() << "File: " << elem.first << ":" << elem.second << "\n";
    }
}

int main(int argc, const char** argv)
{
    initManualCase();
    resultManualCase();
    // filenames explicitly passed
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
        
        std::error_code error_code;
        anonymousLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "root_anonymous.log"), error_code);
        Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());

        delete anonymousLog;
    }
    // filenames are in compile_commands.json
    else {
        std::string err_msg;
        auto database = clang::tooling::JSONCompilationDatabase::loadFromFile(COMPILE_DATABASE, err_msg, JSONCommandLineSyntax::AutoDetect);
        if (database == nullptr) {
            llvm::errs() << "JSON file parse failed\n";
            return 1;
        }
        
        for (std::string& path : database->getAllFiles()) {
            if (!filepathAccessible(path)) {
                llvm::errs() << "Unable to access file '" << path << "'\n";
            }
        }
        ClangTool Tool(*database, database->getAllFiles());
        Tool.setDiagnosticConsumer(new WarningDiagConsumer);
        fileNum = database->getAllFiles().size();

        std::error_code error_code;
        anonymousLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "root_anonymous.log"), error_code);
        Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());

        delete anonymousLog;
    }
    
    int res[6];

    memset(res, 0, sizeof(res));

    for (auto &elem : result) {
        checkManualCase(elem.first);
        ++res[elem.second];
    }

    llvm::outs() << "atomic_t: " << res[REF_ATOMIC_T] << "\n";
    llvm::outs() << "atomic_long_t: " << res[REF_ATOMIC_LONG_T] << "\n";
    llvm::outs() << "atomic64_t: " << res[REF_ATOMIC64_T] << "\n";
    llvm::outs() << "refcount_t: " << res[REF_REFCOUNT_T] << "\n";
    llvm::outs() << "kref: " << res[REF_KREF] << "\n";
    llvm::outs() << "ERROR: " << res[REF_ERROR] << "\n";

    resultManualCase();

    // std::map<std::string, unsigned int> dupTotal;
    // std::map<std::string, unsigned int> oneTotal;
    // std::map<std::string, unsigned int> uniTotal;

    // for (auto it = result.begin(); it != result.end(); ++it) {
    //     auto &val = it->second;

    //     for (auto iter = val.res.begin(); iter != val.res.end(); ++iter) {
    //         dupTotal[iter->first] += iter->second;
    //         oneTotal[iter->first] += 1;
    //     }
    //     uniTotal[val.res.begin()->first] += 1;
    // }

    // llvm::outs() << "dupTotal\n";
    // for (auto it = dupTotal.begin(); it != dupTotal.end(); ++it) {
    //     llvm::outs() << "    " << it->first << ": " << it->second << "\n";
    // }
    // llvm::outs() << "oneTotal\n";
    // for (auto it = oneTotal.begin(); it != oneTotal.end(); ++it) {
    //     llvm::outs() << "    " << it->first << ": " << it->second << "\n";
    // }
    // llvm::outs() << "uniTotal\n";
    // for (auto it = uniTotal.begin(); it != uniTotal.end(); ++it) {
    //     llvm::outs() << "    " << it->first << ": " << it->second << "\n";
    // }

    return EXIT_SUCCESS;
}