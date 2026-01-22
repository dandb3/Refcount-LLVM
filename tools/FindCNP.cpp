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

#define TARGET_BASE_DIR "/home/jdoh/Desktop/work/linux-v6.17/"
#define LOG_PATH "/home/jdoh/Desktop/work/custom_llvm_pass/data/refcount-CNP.log"
#define COMPILE_DATABASE TARGET_BASE_DIR "compile_commands.json"
#define REFCOUNT_FIELDS "/home/jdoh/Desktop/work/custom_llvm_pass/data/refcount-field.log"

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

size_t fileNum;

std::set<std::string> RefcountIDs; // (ClangID)
std::map<std::string, std::set<std::string>> CNPMap; // (ClangID, set of StructName)

std::string getStructName(const RecordDecl *RD) {
    std::string result = RD->getNameAsString();
    std::string prefix = RD->isUnion() ? "union." : "struct.";
    
    if (result != "")
        return prefix + result;
    
    if (const TypedefNameDecl *TND = RD->getTypedefNameForAnonDecl()) {
        if (TND->getNameAsString() != "")
            return prefix + TND->getNameAsString();
    }
    return "";
}

class FieldTypeCallback : public MatchFinder::MatchCallback {
    public:
    static size_t fileNo;

    virtual void onStartOfTranslationUnit() override {
        llvm::outs() << "Collecting refcount candidates...\n";
        llvm::outs() << "[" << ++fileNo << "/" << fileNum << "]\n";
    }

    virtual void onEndOfTranslationUnit() override {}

    virtual void run(const MatchFinder::MatchResult& Result) override {
        const clang::FieldDecl *FD = Result.Nodes.getNodeAs<clang::FieldDecl>("field");

        if (FD == nullptr) {
            return;
        }

        auto &SM = FD->getASTContext().getSourceManager();

        const FileEntry *FE = SM.getFileEntryForID(SM.getFileID(SM.getExpansionLoc(FD->getBeginLoc())));

        std::string Filename = FE->tryGetRealPathName().str().substr(sizeof(TARGET_BASE_DIR) - 1);
        std::string Line = std::to_string(SM.getExpansionLineNumber(FD->getLocation()));
        std::string Fieldname = FD->getNameAsString();

        std::string CID = Filename + ":" + Line + ":" + Fieldname;
        
        if (RefcountIDs.find(CID) == RefcountIDs.end())
            return;
        
        const RecordDecl *tmp = dyn_cast<RecordDecl>(FD->getLexicalDeclContext());
        const RecordDecl *parent;
        std::string CNPName;

        do {
            parent = tmp;
            CNPName = getStructName(parent);
            if (CNPName != "") {
                CNPMap[CID].insert(CNPName);
            }
        } while (tmp = dyn_cast<RecordDecl>(parent->getLexicalDeclContext()));
    }
};

size_t FieldTypeCallback::fileNo = 0;

class FieldTypeASTConsumer : public ASTConsumer {

    public:
    FieldTypeASTConsumer(clang::Preprocessor& PP) {

        auto *callback = new FieldTypeCallback;
        Matcher.addMatcher(
            fieldDecl(
                // unless(hasParent(recordDecl(hasAnyName(
                //     "kref",
                //     "refcount_struct"
                // ))))
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
    private:
        std::string Filename;

    public:
    virtual bool BeginSourceFileAction(CompilerInstance &CI) override {
        const auto &SM = CI.getSourceManager();
        const clang::FileEntry *FE = SM.getFileEntryForID(SM.getMainFileID());
        Filename = FE->tryGetRealPathName().str().substr(sizeof(TARGET_BASE_DIR) - 1);
        llvm::outs() << "file: " << Filename << "\n";
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

void init() {
    std::ifstream ifs(REFCOUNT_FIELDS);

    if (!ifs.is_open()) {
        llvm::errs() << "Opening refcount-field.log failed!\n";
        exit(1);
    }

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty())
            continue;
            
        if (line.find_first_not_of(" \t\r\n") == std::string::npos)
            continue;
        
        RefcountIDs.insert(line);
    }
}

void fini() {
    std::ofstream ofs(LOG_PATH);

    if (!ofs.is_open()) {
        llvm::errs() << "Opening refcount-CNP.log failed!\n";
        exit(1);
    }

    for (auto &elem : CNPMap) {
        ofs << elem.first << "----";
        for (auto &el : elem.second) {
            ofs << el << ",";
        }
        ofs << "\n";
    }
}

int main(int argc, const char** argv)
{
    init();

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
        
            Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());
        }
    }

    fini();

    return EXIT_SUCCESS;
}
