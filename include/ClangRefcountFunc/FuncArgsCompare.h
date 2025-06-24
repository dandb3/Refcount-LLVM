#ifndef CLANG_REFCOUNT_FUNC_H
#define CLANG_REFCOUNT_FUNC_H

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

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

static std::ofstream total_output;

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

// key = { path, line }
// value = { { SET, 1 }, { ADD, 1 }, { SUB, 1 }, ... }
typedef std::pair<std::string, unsigned int> RefcntKey;
typedef std::pair<APIType, int> RefcntVal;
typedef std::map<RefcntKey, std::vector<RefcntVal>> RefcntMap;
static RefcntMap refcntCandidates;
static std::set<std::string> funcNames;

class ArgTypeCallback : public MatchFinder::MatchCallback {
    private:
    std::set<std::string> files;

    RefcntMap::iterator getIter(const clang::SourceManager &SM,
        const Expr *refcntArg);
    RefcntVal getVal(const Expr *valArg, APIType apiType, long long diff,
        long long sign);
    bool setKeyVal(const clang::SourceManager &SM, const CallExpr *node,
        APIType apiType, APIArgType argType, long long diff, long long sign);

    public:
    virtual void onStartOfTranslationUnit() override;
    virtual void onEndOfTranslationUnit() override;
    virtual void run(const MatchFinder::MatchResult& Result) override;
};

class ArgTypeASTConsumer : public ASTConsumer {

    public:
    ArgTypeASTConsumer(clang::Preprocessor& PP);

    virtual void HandleTranslationUnit(ASTContext& Context) override;

    private:
    MatchFinder Matcher;
};

class ArgTypeFrontEndAction : public ASTFrontendAction {

    public:
    virtual bool BeginSourceFileAction(CompilerInstance &CI) override;

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
            CompilerInstance& CI, StringRef file) override;
    virtual void EndSourceFileAction() override;
};

// bool satisfyRules(std::vector<RefcntVal> &vec) {
//     bool setExist = false, incExist = false, decExist = false;  // Rule 1
//     bool setValueIsOne = true;                                  // Rule 2
//     bool incContainsOne = false, decContainsOne = false;        // Rule 3

//     for (auto &elem : vec) {
//         switch (elem.first) {
//         case APIType::SET:
//             setExist = true;
//             if (elem.second > 1) {
//                 setValueIsOne = false;
//             }
//             break;
//         case APIType::DIFF:
//             if (elem.second > 0) {
//                 incExist = true;
//                 if (elem.second == 1) {
//                     incContainsOne = true;
//                 }
//             }
//             else if (elem.second < 0) {
//                 decExist = true;
//                 if (elem.second == -1) {
//                     decContainsOne = true;
//                 }
//             }
//             break;
//         }
//     }
//     return setExist && incExist && decExist && setValueIsOne && incContainsOne && decContainsOne;
// }

#endif
