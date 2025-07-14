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
#define LOG_DIR "/home/junwoong/work/refcount/build2/log-cur/"
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
typedef std::pair<RefcountType, std::vector<OPStat>> RefcntVal;
typedef std::map<RefcntKey, RefcntVal> RefcntMap;

size_t fileNum;

// logs
llvm::raw_fd_ostream *resultLog;

// error logs
llvm::raw_fd_ostream *fieldNoExistLog;
llvm::raw_fd_ostream *funcAccessedLog;

std::map<ID, std::string> operations;
RefcntMap result;

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
    else if (type == "refcount_struct") {
        return REF_REFCOUNT_STRUCT;
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

    virtual void onStartOfTranslationUnit() override {
        llvm::outs() << "Collecting refcount candidates...\n";
        llvm::outs() << "[" << ++fileNo << "/" << fileNum << "]\n";
    }

    virtual void onEndOfTranslationUnit() override {}

    virtual void run(const MatchFinder::MatchResult& Result) override {
        const clang::FieldDecl *node = Result.Nodes.getNodeAs<clang::FieldDecl>("field");

        if (node == nullptr) {
            return;
        }

        const auto &SM = *Result.SourceManager;
        const auto &fieldLoc = SM.getExpansionLoc(node->getLocation());
        const auto &fieldLocBegin = SM.getExpansionLoc(node->getBeginLoc());
        // const auto &filename = SM.getFilename(SM.getSpellingLoc(fieldLoc)).str();

        const FileEntry *FE = SM.getFileEntryForID(SM.getFileID(SM.getSpellingLoc(fieldLocBegin)));
        const auto &filename = FE->tryGetRealPathName().str().substr(sizeof(TARGET_DIR) - 1);

        const auto *RT = node->getType().getCanonicalType()->getAs<RecordType>();
        if (RT) {
            const RecordDecl *RD = RT->getDecl()->getDefinition();
            if (RD) {
                result.insert({ { SM, filename, fieldLoc },
                    { getRefcountType(node->getType().getAsString()), std::vector<OPStat>() } });
            }
        }
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

class ArgTypeCallback : public MatchFinder::MatchCallback {
    private:
    static size_t fileNo;

    RefcntMap::iterator getIter(const clang::SourceManager &SM, const Expr *refcntArg) {
        RefcntMap::iterator ret = result.end();

        refcntArg = refcntArg->IgnoreParenImpCasts();
        while (const auto *unaryOp = dyn_cast<UnaryOperator>(refcntArg)) {
            refcntArg = unaryOp->getSubExpr()->IgnoreParenImpCasts();
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

            if (const auto *unaryOp = dyn_cast<UnaryOperator>(valArg)) {
                if (unaryOp->getOpcode() == UO_Minus) {
                    sign *= -1;
                    valArg = unaryOp->getSubExpr()->IgnoreParenImpCasts();
                }
            }

            if (const auto *intLit = dyn_cast<IntegerLiteral>(valArg)) {
                diff = intLit->getValue().getSExtValue();
            }
            else {
                // llvm::errs() << "Argument is not literal!\n";
                // node->dump();
                return {APIType::VAR, 0};
            }
        }
        diff *= sign;
        return { apiType, diff };
    }

    bool setKeyVal(const clang::SourceManager &SM, const CallExpr *node,
        APIType apiType, APIArgType argType, long long diff, long long sign) {

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
            // funcKey.print(*funcAccessedLog, calleeName);
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

        OPStat stat(funcKey, calleeName, getTuple(valArg, apiType, diff, sign));
        
        auto mapIt = getIter(SM, refcntArg);
        if (mapIt == result.end()) {
            // LOG
            stat.print(*fieldNoExistLog, "");
            return true;
        }

        mapIt->second.second.push_back(stat);
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
        const std::string &calleeName = node->getDirectCallee()->getNameAsString();
        
        bool err;

        if (calleeName.find("init") != std::string::npos) {
            err = setKeyVal(SM, node, APIType::SET, APIArgType::REF_ONLY, 1, 1);
        }
        else if (calleeName.find("get") != std::string::npos
            || calleeName.find("inc") != std::string::npos) {
            err = setKeyVal(SM, node, APIType::DIFF, APIArgType::REF_ONLY, 1, 1);
        }
        else if (calleeName.find("put") != std::string::npos
            || calleeName.find("dec") != std::string::npos) {
            err = setKeyVal(SM, node, APIType::DIFF, APIArgType::REF_ONLY, 1, -1);
        }
        else if (calleeName.find("set") != std::string::npos) {
            err = setKeyVal(SM, node, APIType::SET, APIArgType::REF_VAL, 0, 1);
        }
        else if (calleeName.find("add_unless") != std::string::npos) {
            err = setKeyVal(SM, node, APIType::DIFF, APIArgType::REF_VAL, 0, 1);
        }
        else if (calleeName.find("add") != std::string::npos) {
            err = setKeyVal(SM, node, APIType::DIFF, APIArgType::VAL_REF, 0, 1);
        }
        else if (calleeName.find("sub") != std::string::npos) {
            err = setKeyVal(SM, node, APIType::DIFF,  APIArgType::VAL_REF, 0, -1);
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

void initialize(std::error_code &ecode) {
    resultLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "result.stat"), ecode);
    fieldNoExistLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "field_no_exist.err"), ecode);
    funcAccessedLog = new raw_fd_ostream(llvm::StringRef(LOG_DIR "func_accessed.err"), ecode);

    if (fieldNoExistLog == nullptr || funcAccessedLog == nullptr
        || resultLog == nullptr) {
        llvm::errs() << "log file creation failed!\n";
        exit(1);
    }
}

void finish() {
    delete resultLog;
    delete fieldNoExistLog;
    delete funcAccessedLog;
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
        
        for (std::string& path : database->getAllFiles()) {
            if (!filepathAccessible(path)) {
                llvm::errs() << "Unable to access file '" << path << "'\n";
            }
        }
        ClangTool Tool(*database, database->getAllFiles());
        Tool.setDiagnosticConsumer(new WarningDiagConsumer);
        fileNum = database->getAllFiles().size();

        Tool.run(newFrontendActionFactory<FieldTypeFrontEndAction>().get());
        Tool.run(newFrontendActionFactory<ArgTypeFrontEndAction>().get());
    }

    for (auto &elem : result) {
        elem.first.print(*resultLog);

        std::string type;
        switch (elem.second.first) {
        case REF_ATOMIC_T:
            type = "atomic_t";
            break;
        case REF_ATOMIC_LONG_T:
            type = "atomic_long_t";
            break;
        case REF_ATOMIC64_T:
            type = "atomic64_t";
            break;
        case REF_REFCOUNT_T:
            type = "refcount_t";
            break;
        case REF_KREF:
            type = "kref";
            break;
        case REF_REFCOUNT_STRUCT:
            type = "refcount_struct";
            break;
        case REF_ERROR:
            type = "atomic_t";
            break;
        }
        *resultLog << type << "\n";
        for (auto &vecElem : elem.second.second) {
            vecElem.print(*resultLog, "    ");
        }
    }

    finish();

    return EXIT_SUCCESS;
}
