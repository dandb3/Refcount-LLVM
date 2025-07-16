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
#define LOG_DIR "/home/junwoong/work/refcount/build_refcount_select_test/log/"
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

    ID(const SourceManager &SM, const std::string &filename, const SourceLocation &loc)
    : filename(filename), line(SM.getExpansionLineNumber(loc)), col(SM.getExpansionColumnNumber(loc)) {}

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

class OPStat {
    public:
    ID pos;
    std::string name;
    std::pair<APIType, int> tuple;

    OPStat(const ID &pos, const std::string &name, const std::pair<APIType, int> &tuple)
    : pos(pos), name(name), tuple(tuple) {}

    void print(llvm::raw_fd_ostream &os, const std::string &indent = "") const {
        std::string APIstr;

        switch (tuple.first) {
        case SET:
            APIstr = "SET";
            break;
        case DIFF:
            APIstr = "DIFF";
            break;
        case VAR:
            APIstr = "VAR";
            break;
        case ERROR:
            APIstr = "ERROR";
            break;
        default:
            break;
        }

        os << indent << pos.filename << ":" << pos.line << ":" << pos.col << "\n";
        os << indent << name << ":<" << APIstr << "," << tuple.second << ">\n";
    }
};

typedef ID RefcntKey;
typedef std::vector<OPStat> RefcntVal;
typedef std::map<RefcntKey, RefcntVal> RefcntMap;

size_t fileNum;

// logs
llvm::raw_fd_ostream *resultLog;

// error logs
llvm::raw_fd_ostream *varErr;
llvm::raw_fd_ostream *fieldNoExistErr;

std::map<ID, std::string> operations;
RefcntMap result;

class ArgTypeCallback : public MatchFinder::MatchCallback {
    private:
    static size_t fileNo;

    RefcntMap::iterator getIter(const clang::SourceManager &SM, const Expr *refcntArg) {
        RefcntMap::iterator ret = result.end();

        refcntArg = refcntArg->IgnoreParenImpCasts();
        while (const auto *UO = dyn_cast<UnaryOperator>(refcntArg)) {
            refcntArg = UO->getSubExpr()->IgnoreParenImpCasts();
        }

        if (const auto *memberExpr = dyn_cast<MemberExpr>(refcntArg)) {
            if (const auto *fieldDecl = dyn_cast<FieldDecl>(memberExpr->getMemberDecl())) {
                const auto &fieldLoc = SM.getExpansionLoc(fieldDecl->getLocation());
                const auto &fieldLocBegin = SM.getExpansionLoc(fieldDecl->getBeginLoc());

                const FileEntry *FE = SM.getFileEntryForID(SM.getFileID(SM.getSpellingLoc(fieldLocBegin)));
                const auto &filename = FE->tryGetRealPathName().str().substr(sizeof(TARGET_DIR) - 1);

                ID fieldKey(SM, filename, fieldLoc);

                ret = result.find(fieldKey);
            }
        }
        return ret;
    }
    
    std::pair<APIType, int> getTuple(const Expr *valArg, APIType apiType,
        long long diff, long long sign) {
        if (valArg != nullptr) {
            valArg = valArg->IgnoreParenImpCasts();

            while (const auto *UO = dyn_cast<UnaryOperator>(valArg)) {
                if (UO->getOpcode() == UO_Minus) {
                    sign *= -1;
                }
                valArg = UO->getSubExpr()->IgnoreParenImpCasts();
            }

            if (const auto *intLit = dyn_cast<IntegerLiteral>(valArg)) {
                diff = intLit->getValue().getSExtValue();
            }
            else {
                return {APIType::VAR, 0};
            }
        }
        diff *= sign;
        return { apiType, diff };
    }

    bool setKeyVal(const clang::SourceManager &SM, const clang::LangOptions &LO,
        const CallExpr *node, APIType apiType, APIArgType argType,
        long long diff, long long sign) {

        const auto &funcLoc = SM.getExpansionLoc(node->getExprLoc());
        const auto &funcLocBegin = SM.getExpansionLoc(node->getBeginLoc());
        
        const FileEntry *FE = SM.getFileEntryForID(SM.getFileID(SM.getSpellingLoc(funcLocBegin)));
        const auto &filename = FE->tryGetRealPathName().str().substr(sizeof(TARGET_DIR) - 1);

        const std::string &calleeName = node->getDirectCallee()->getNameAsString();

        const Expr *refcntArg, *valArg;

        ID funcKey(SM, filename, funcLoc);

        bool err = !operations.insert({ funcKey, calleeName }).second;
        
        if (err) {
            // LOG
            // funcKey.print(*varErr, calleeName);
            return true;
        }

        switch (argType) {
        case APIArgType::REF_ONLY:
            refcntArg = node->getArg(0);
            valArg = nullptr;
            break;
        case APIArgType::REF_VAL:
            refcntArg = node->getArg(0);
            valArg = node->getArg(1);
            break;
        case APIArgType::VAL_REF:
            refcntArg = node->getArg(1);
            valArg = node->getArg(0);
            break;
        }

        std::pair<APIType, int> tuple = getTuple(valArg, apiType, diff, sign);

        if (tuple.first == APIType::VAR) {
            // LOG
            SourceRange range = valArg->getSourceRange();
            std::string text = clang::Lexer::getSourceText(
                clang::CharSourceRange::getTokenRange(range.getBegin(), range.getEnd()),
                SM, LO).str();
            
            funcKey.print(*varErr);
            *varErr << "\n" << text << "\n\n";
        }

        // OPStat stat(funcKey, calleeName, tuple);
        
        // auto mapIt = getIter(SM, refcntArg);
        // if (mapIt == result.end()) {
        //     // LOG
        //     SourceRange range = valArg->getSourceRange();
        //     std::string text = clang::Lexer::getSourceText(
        //         clang::CharSourceRange::getTokenRange(range.getBegin(), range.getEnd()),
        //         SM, LO).str();

        //     stat.print(*fieldNoExistErr, "");
        //     *fieldNoExistErr << "\n" << text << "\n\n";
        //     return true;
        // }

        // mapIt->second.push_back(stat);
        return false;
    }

    public:
    virtual void onStartOfTranslationUnit() override {
        llvm::outs() << "Collecting refcount operations...\n";
        llvm::outs() << "[" << ++fileNo << "/" << fileNum << "]\n";
    }

    virtual void onEndOfTranslationUnit() override {}

    virtual void run(const MatchFinder::MatchResult& Result) override {
        const clang::CallExpr *node = Result.Nodes.getNodeAs<clang::CallExpr>("argType");

        if (node == nullptr) {
            llvm::errs() << "node not matching argType!\n";
            return;
        }
        
        const auto &SM = *Result.SourceManager;
        const auto &LO = Result.Context->getLangOpts();
        const std::string &calleeName = node->getDirectCallee()->getNameAsString();
        
        bool err;

        if (calleeName.find("init") != std::string::npos) {
            err = setKeyVal(SM, LO, node, APIType::SET, APIArgType::REF_ONLY, 1, 1);
        }
        else if (calleeName.find("get") != std::string::npos
            || calleeName.find("inc") != std::string::npos) {
            err = setKeyVal(SM, LO, node, APIType::DIFF, APIArgType::REF_ONLY, 1, 1);
        }
        else if (calleeName.find("put") != std::string::npos
            || calleeName.find("dec") != std::string::npos) {
            err = setKeyVal(SM, LO, node, APIType::DIFF, APIArgType::REF_ONLY, 1, -1);
        }
        else if (calleeName.find("set") != std::string::npos) {
            err = setKeyVal(SM, LO, node, APIType::SET, APIArgType::REF_VAL, 0, 1);
        }
        else if (calleeName.find("add_unless") != std::string::npos) {
            err = setKeyVal(SM, LO, node, APIType::DIFF, APIArgType::REF_VAL, 0, 1);
        }
        else if (calleeName.find("add") != std::string::npos) {
            err = setKeyVal(SM, LO, node, APIType::DIFF, APIArgType::VAL_REF, 0, 1);
        }
        else if (calleeName.find("sub") != std::string::npos) {
            err = setKeyVal(SM, LO, node, APIType::DIFF,  APIArgType::VAL_REF, 0, -1);
        }
    }
};

size_t ArgTypeCallback::fileNo = 0;

class ArgTypeASTConsumer : public ASTConsumer {

    public:
    ArgTypeASTConsumer(clang::Preprocessor& PP) {
        auto *callback = new ArgTypeCallback;

        Matcher.addMatcher(
            callExpr(callee(functionDecl(
                matchesName("^::(kref_|atomic_|atomic_long_|atomic64_|refcount_).*(set|add|sub|inc|dec|init|get|put).*")
            ))).bind("argType"),
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

void parseInputFile() {
    std::ifstream ifs(LOG_DIR "result.stat");

    if (!ifs.is_open()) {
        llvm::errs() << "refcount stat does not exist!\n";
        exit(1);
    }

    std::string typeName, fileLine, filename;
    unsigned int line, col;
    char sep;

    while (std::getline(ifs, typeName)) {
        if (typeName.empty())
            continue;

        if (!std::getline(ifs, fileLine)) {
            std::cerr << "parse failed\n";
            exit(1);
        }

        std::istringstream iss(fileLine);

        if (std::getline(iss, filename, ':') &&
            (iss >> line >> sep >> col) &&
            sep == ':') {
            ID key(filename, line, col);

            result[key];
        } else {
            std::cerr << "parse failed\n";
            exit(1);
        }
    }
}

void initialize(std::error_code &ecode) {
    // parseInputFile();

    resultLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "operations.stat"), ecode);
    fieldNoExistErr = new raw_fd_ostream(llvm::StringRef(LOG_DIR "field_no_exist.err"), ecode);
    varErr = new raw_fd_ostream(llvm::StringRef(LOG_DIR "var.err"), ecode);

    if (fieldNoExistErr == nullptr || varErr == nullptr
        || resultLog == nullptr) {
        llvm::errs() << "log file creation failed!\n";
        exit(1);
    }
}

void finish() {
    delete resultLog;
    delete fieldNoExistErr;
    delete varErr;
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

    // for (auto &elem : result) {
    //     elem.first.print(*resultLog);
    //     for (auto &vecElem : elem.second) {
    //         vecElem.print(*resultLog, "    ");
    //     }
    // }

    finish();

    return EXIT_SUCCESS;
}
