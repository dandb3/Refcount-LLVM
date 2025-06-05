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

#define TEST_DIR "new"
#define LOG_DIR "/home/jdoh/test/" TEST_DIR "/log/"
#define COMPILE_DATABASE "/home/jdoh/test/" TEST_DIR "/compile_commands.json"

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

// ----------------------------------------------------------------------------
// COMMAND LINE ARGUMENT SETUP
// ----------------------------------------------------------------------------

// Set up the standard command line arguments for the tool.
// These command line arguments are standard across all tools
// built using the LLVM framework.
static cl::OptionCategory refcntCategory("refcnt options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// // Here we add an extra non-standard command line flag 
// // for demonstration purposes
static cl::opt<bool> verbose("verbose",
    cl::desc(R"(Generate verbose output)"),     // the description
    cl::init(false),                            // the initial value of the option
    cl::cat(refcntCategory)                   // what category this belongs to
);

// ----------------------------------------------------------------------------
// DEFAULT WARNING SUPPRESSION
// ----------------------------------------------------------------------------

// The WarningDiagConsumer allows us to suppress warning and error messages
// which are raised when a file is being parsed by clang. This allows us to
// turn off extraneous output since we assume that we are checking code 
// which already compiles.
class WarningDiagConsumer : public DiagnosticConsumer {

    public:
    virtual void HandleDiagnostic(
            DiagnosticsEngine::Level Level, const Diagnostic& Info) override {
        // simply do nothing
    }
};

// ----------------------------------------------------------------------------
// CALLBACK CLASSES
// ----------------------------------------------------------------------------

// Here we will be adding our callback classes which will be
// doing the actual code analysis
//
// ...

static std::ofstream total_output;

// const RecordDecl *getTopLevelStruct(const FieldDecl *field) {
//     const DeclContext *tmp = dyn_cast<DeclContext>(field->getParent());
//     const RecordDecl *parent = nullptr;

//     while (dyn_cast<RecordDecl>(tmp)) {
//         parent = dyn_cast<RecordDecl>(tmp);
//         tmp = parent->getParent();
//     }

//     return parent;
// }

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

// key = { path, line}
// value = { { SET, 1 }, { ADD, 1 }, { SUB, 1 }, ... }
typedef std::pair<std::string, unsigned int> RefcntKey;
typedef std::pair<APIType, int> RefcntVal;
typedef std::map<RefcntKey, std::vector<RefcntVal>> RefcntMap;
static RefcntMap refcntCandidates;
static std::set<std::string> funcNames;

std::ofstream recursiveRefcount;

class FieldTypeCallback : public MatchFinder::MatchCallback {
    private:
    std::set<std::string> files;

    public:
    virtual void onStartOfTranslationUnit() override {
        
    }

    virtual void onEndOfTranslationUnit() override {
        
    }

    virtual void run(const MatchFinder::MatchResult& Result) override {
        const clang::FieldDecl *node = Result.Nodes.getNodeAs<clang::FieldDecl>("fieldType");

        if (node == nullptr) {
            return;
        }

        const auto &SM = *Result.SourceManager;
        const auto &loc = node->getBeginLoc();
        
        const RecordDecl *tmp = node->getParent();
        const RecordDecl *parent;
        
        do {
            parent = tmp;
            // llvm::outs() << parent->getName() << "\n";
        } while(tmp = dyn_cast<RecordDecl>(parent->getParent()));
        
        llvm::outs() << "Struct: " << parent->getName() << "\n";
        llvm::outs() << "    " << node->getType().getAsString() << " " << node->getName() << "\n";
    }
};

// ----------------------------------------------------------------------------
// REGISTERING CALLBACKS
// ----------------------------------------------------------------------------

// The ASTConsumer allows us to dictate:
//      - which AST nodes we match, and
//      - how we want to handle those matched nodes

class FieldTypeASTConsumer : public ASTConsumer {

    public:
    FieldTypeASTConsumer(clang::Preprocessor& PP) {
    
        // Here we add all of the checks that should be run
        // when the AST is traversed by using Matcher.addMatcher

        // At the moment, there are no checks registered

        // PP.addPPCallbacks(std::make_unique<clang::PPCallbacks>());

        auto *callback = new FieldTypeCallback;

        Matcher.addMatcher(
            fieldDecl(
                anyOf(
                    hasType(typedefNameDecl(hasAnyName(
                        "atomic_t",
                        "atomic_long_t",
                        "atomic64_t",
                        "refcount_t"
                    ))),
                    hasType(recordDecl(hasName("kref")))
                ),
                unless(hasAncestor(recordDecl(hasAnyName(
                    "kref",
                    "refcount_struct"
                ))))
            ).bind("fieldType"),
            callback
        );
    }

    virtual void HandleTranslationUnit(ASTContext& Context) override {
        Matcher.matchAST(Context);
    }

    private:
    MatchFinder Matcher;
};

// The FrontEndAction is the main entry point for the clang tooling library
// and allows us to add callbacks via:
//      PPCallback classes  - for callbacks involving the preprocessor
//      ASTConsumer classes - for callbacks involving AST nodes 

class FieldTypeFrontEndAction : public ASTFrontendAction {

    public:
    virtual bool BeginSourceFileAction(CompilerInstance &CI) override {
        const auto &SM = CI.getSourceManager();
        
        // const std::string &filePath = SM.getFileEntryForID(SM.getMainFileID())->tryGetRealPathName().str();

        // llvm::outs() << "File path: " << filePath << "\n";

        return true;
    }

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
            CompilerInstance& CI, StringRef file) override {

        // Here we add any checks which require the preprocessor.
        // At the moment, there are no checks registered

        return std::unique_ptr<ASTConsumer>(
                new FieldTypeASTConsumer(CI.getPreprocessor()));
    }

    virtual void EndSourceFileAction() override {
        
    }
};

// ----------------------------------------------------------------------------
// OUR PROGRAM
// ----------------------------------------------------------------------------

// Checks if the given filepath can be accessed without issue.
// Returns true if it is accessible, else returns false.
bool filepathAccessible(std::string path)
{   
    std::ifstream file(path);
    return file.is_open();
}

bool satisfyRules(std::vector<RefcntVal> &vec) {
    bool setExist = false, incExist = false, decExist = false;  // Rule 1
    bool setValueIsOne = true;                                  // Rule 2
    bool incContainsOne = false, decContainsOne = false;        // Rule 3

    for (auto &elem : vec) {
        switch (elem.first) {
        case APIType::SET:
            setExist = true;
            if (elem.second > 1) {
                setValueIsOne = false;
            }
            break;
        case APIType::DIFF:
            if (elem.second > 0) {
                incExist = true;
                if (elem.second == 1) {
                    incContainsOne = true;
                }
            }
            else if (elem.second < 0) {
                decExist = true;
                if (elem.second == -1) {
                    decContainsOne = true;
                }
            }
            break;
        }
    }
    return setExist && incExist && decExist && setValueIsOne && incContainsOne && decContainsOne;
}

int main(int argc, const char** argv)
{
    // Parse the command line arguments. This will provide us with
    // the list of files we need to check as well as allow us to
    // check for the optional flags. Note that we also allow for
    // zero or more arguments to allow for more fine-grained error
    // checking
    if (argc == 2) {
        auto OptionsParser = CommonOptionsParser::create(argc, argv, refcntCategory, cl::ZeroOrMore);
        if (auto err = OptionsParser.takeError()) {
            llvm::errs() << std::move(err);
            return EXIT_FAILURE;
        }
    

        // Our program is meant to analyse source code, so if we didn't
        // get any filepaths, we print an error message and exit
        auto files = OptionsParser->getSourcePathList();
        if (files.empty()) {
            llvm::errs() << "Error: No input files specified\n";
            return EXIT_FAILURE;
        }
        // Also, if any of the filepaths we've received are invalid,
        // we also print an error message and exit
        for (auto path : files) {
            if (!filepathAccessible(path)) {
                llvm::errs() << "Unable to access file '" << path << "'\n";
                return EXIT_FAILURE;
            }
        }

        ClangTool Tool(OptionsParser->getCompilations(), files);
        Tool.setDiagnosticConsumer(new WarningDiagConsumer);

        recursiveRefcount.open(LOG_DIR "recursive.log");
        if (!recursiveRefcount.is_open()) {
            llvm::errs() << "recursiveRefcount open failed\n";
        }
        Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());

        recursiveRefcount.close();
    }
    else {
        llvm::outs() << "Usage: " << argv[0] << " <filename>\n";
        // llvm::outs() << "start\n";
        // std::string err_msg;
        // auto database = clang::tooling::JSONCompilationDatabase::loadFromFile(COMPILE_DATABASE, err_msg, JSONCommandLineSyntax::AutoDetect);
        // if (database == nullptr) {
        //     llvm::errs() << "JSON file parse failed\n";
        //     return 1;
        // }
        
        // for (std::string& path : database->getAllFiles()) {
        //     if (!filepathAccessible(path)) {
        //         llvm::errs() << "Unable to access file '" << path << "'\n";
        //     }
        // }

        // // Next, we create the tool which will perform all of the 
        // // code analysis. In the base code you are provided, this
        // // tool doesn't perform any analysis at all.
        // ClangTool Tool(*database, database->getAllFiles());
        // Tool.setDiagnosticConsumer(new WarningDiagConsumer);
        // system("rm -rf " LOG_DIR "*");

        // recursiveRefcount.open(LOG_DIR "recursive.log");
        // if (!recursiveRefcount.is_open()) {
        //     llvm::errs() << "recursiveRefcount open failed\n";
        // }

        // Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());

        // recursiveRefcount.close();
    }
    return EXIT_SUCCESS;
}