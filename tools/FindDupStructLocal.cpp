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
#define LOG_DIR "/home/junwoong/work/refcount/build_dup_struct_local/log/"
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

static size_t fileNum;
static size_t fileNo;

static llvm::raw_fd_ostream *dupStructLog;
static llvm::raw_fd_ostream *anonStructLog;

class FieldTypeCallback : public MatchFinder::MatchCallback {
    private:
    std::set<std::string> structNames;
    std::set<std::pair<std::string, unsigned int>> positions;

    public:
    virtual void onStartOfTranslationUnit() override {}

    virtual void onEndOfTranslationUnit() override {
        structNames.clear();
        positions.clear();
    }

    virtual void run(const MatchFinder::MatchResult& Result) override {
        const clang::RecordDecl *node = Result.Nodes.getNodeAs<clang::RecordDecl>("fieldType");

        if (node == nullptr) {
            return;
        }

        const auto &SM = *Result.SourceManager;
        const auto &loc = node->getBeginLoc();
        const auto &filename = SM.getFilename(SM.getSpellingLoc(loc)).str();
        
        const RecordDecl *tmp = node;
        const RecordDecl *parent;
        
        do {
            parent = tmp;
            if (parent->getName() != "")
                break;
        } while((tmp = dyn_cast<RecordDecl>(parent->getLexicalDeclContext())));

        StringRef name = parent->getName();

        if (name == "") {
            const DeclContext *top = parent->getLexicalDeclContext();
            bool typedefFound = false;

            for (auto it = top->decls_begin(); it != top->decls_end(); ++it) {
                const RecordDecl *cur = dyn_cast<RecordDecl>(*it);
                if (cur == nullptr)
                    continue;

                if (cur == parent) {
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
                        name = TD->getName();
                        typedefFound = true;
                        break;
                    }
                    break;
                }
            }

            if (!typedefFound) {
                *anonStructLog << "pos: " << filename << ":" << SM.getExpansionLineNumber(loc) << "\n";
                *anonStructLog << "name: " << name << "\n";
                parent->print(*anonStructLog);
                *anonStructLog << "\n";

                return;
            }
        }

        const auto &parentLoc = parent->getBeginLoc();
        const auto &parentFilename = SM.getFilename(SM.getSpellingLoc(parentLoc)).str();

        if (positions.insert({parentFilename, SM.getExpansionLineNumber(parentLoc)}).second) {
            if (!structNames.insert(name.str()).second) {
                *dupStructLog << "pos: " << parentFilename << ":" << SM.getExpansionLineNumber(parentLoc) << "\n";
                *dupStructLog << "name: " << name << "\n";
                parent->print(*dupStructLog);
                *dupStructLog << "\n";
            }
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
    dupStructLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "dup_struct.log"), error_code);
    anonStructLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "anon_struct.log"), error_code);
    fileNum = totalFiles.size();
}

void finish() {
    delete dupStructLog;
    delete anonStructLog;
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