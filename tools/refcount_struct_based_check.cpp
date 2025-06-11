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
#define LOG_DIR "/home/junwoong/work/refcount/build1/log/"
#define COMPILE_DATABASE LOG_DIR "compile_commands.json"

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

// static std::ofstream total_output;

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

// typedef std::pair<std::string, unsigned int> RefcntKey;
// typedef std::pair<APIType, int> RefcntVal;
// typedef std::map<RefcntKey, std::vector<RefcntVal>> RefcntMap;
// static RefcntMap refcntCandidates;
// static std::set<std::string> funcNames;

static std::set<std::string> structNames;
static std::set<std::pair<std::string, unsigned int>> anonymousSet;
static std::set<std::string> globalWorkingFiles;

size_t fileNum;

llvm::raw_fd_ostream *anonymousLog;

class FieldTypeCallback : public MatchFinder::MatchCallback {
    private:
    std::set<std::string> workingFiles;
    static size_t fileNo;

    public:
    virtual void onStartOfTranslationUnit() override {
        llvm::outs() << "[" << fileNo++ << "/" << fileNum << "]\n";
    }

    virtual void onEndOfTranslationUnit() override {
        globalWorkingFiles.insert(workingFiles.begin(), workingFiles.end());
        workingFiles.clear();
    }

    virtual void run(const MatchFinder::MatchResult& Result) override {
        const clang::FieldDecl *node = Result.Nodes.getNodeAs<clang::FieldDecl>("fieldType");

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
        
        const RecordDecl *tmp = dyn_cast<RecordDecl>(node->getLexicalDeclContext());
        const RecordDecl *parent;
        
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
            const TranslationUnitDecl *top = dyn_cast<TranslationUnitDecl>(parent->getLexicalDeclContext());
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
                auto result = anonymousSet.insert({filename, SM.getExpansionLineNumber(loc)});
                if (!result.second) {
                    llvm::errs() << "ERROR occured in run() - " << filename << ":" << SM.getExpansionLineNumber(loc) << "\n";
                    exit(1);
                }
                *anonymousLog << "pos: " << filename << ":" << SM.getExpansionLineNumber(loc) << "\n";
                *anonymousLog << "name: " << name << "\n";
                parent->print(*anonymousLog);
                *anonymousLog << "\n";

                return;
            }
        }

        auto result = structNames.insert(name);
        if (!result.second) {
            // There might be multiple structs with duplicated name...
            *anonymousLog << "DUPLICATED NAME FOUND!\n";
            *anonymousLog << "pos: " << filename << ":" << SM.getExpansionLineNumber(loc) << "\n";
            *anonymousLog << "name: " << name << "\n";
            parent->print(*anonymousLog);
            *anonymousLog << "\n";
        }
    }
};

size_t FieldTypeCallback::fileNo = 1;

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
        // const auto &SM = CI.getSourceManager();
        
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

int main(int argc, const char** argv)
{
    // Parse the command line arguments. This will provide us with
    // the list of files we need to check as well as allow us to
    // check for the optional flags. Note that we also allow for
    // zero or more arguments to allow for more fine-grained error
    // checking

    // filenames explicitly passed
    if (argc > 1) {
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
        fileNum = argc - 1;
        
        std::error_code error_code;
        anonymousLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "ancestor_anonymous.log"), error_code);
        // if (!anonymousLog) {
        //     llvm::errs() << "anonymousLog open failed\n";
        // }
        Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());

        delete anonymousLog;
    }
    // filenames are in compile_commands.json
    else {
        // llvm::outs() << "Usage: " << argv[0] << " <filename>\n";
        // llvm::outs() << "start\n";
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

        // // Next, we create the tool which will perform all of the 
        // // code analysis. In the base code you are provided, this
        // // tool doesn't perform any analysis at all.
        ClangTool Tool(*database, database->getAllFiles());
        Tool.setDiagnosticConsumer(new WarningDiagConsumer);
        fileNum = database->getAllFiles().size();

        std::error_code error_code;
        anonymousLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "ancestor_anonymous.log"), error_code);
        // if (!anonymousLog) {
        //     llvm::errs() << "anonymousLog open failed\n";
        // }
        Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());

        delete anonymousLog;
    }
    return EXIT_SUCCESS;
}