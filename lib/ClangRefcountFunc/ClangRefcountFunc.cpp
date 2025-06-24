#include "ClangRefcountFunc/ClangRefcountFunc.h"

/* ---------- MatchCallback ---------- */

RefcntMap::iterator ArgTypeCallback::getIter(const clang::SourceManager &SM,
    const Expr *refcntArg) {
    RefcntMap::iterator ret = refcntCandidates.end();

    refcntArg = refcntArg->IgnoreParenImpCasts();
    while (const auto *unaryOp = dyn_cast<UnaryOperator>(refcntArg)) {
        refcntArg = unaryOp->getSubExpr()->IgnoreParenImpCasts();
    }

    if (const auto *memberExpr = dyn_cast<MemberExpr>(refcntArg)) {
        if (const auto *fieldDecl = dyn_cast<FieldDecl>(memberExpr->getMemberDecl())) {
            SourceLocation loc = fieldDecl->getBeginLoc();
            ret = refcntCandidates.find({SM.getFilename(loc).str(), SM.getExpansionLineNumber(loc)});
        }
    }
    return ret;
}

RefcntVal ArgTypeCallback::getVal(const Expr *valArg, APIType apiType, long long diff,
    long long sign) {
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
            llvm::errs() << "Argument is not literal!\n";
            // node->dump();
            return {APIType::ERROR, 0};
        }
    }
    diff *= sign;
    return {apiType, diff};
}

bool ArgTypeCallback::setKeyVal(const clang::SourceManager &SM, const CallExpr *node,
    APIType apiType, APIArgType argType, long long diff, long long sign) {
    const Expr *refcntArg, *valArg;

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

    auto mapIt = getIter(SM, refcntArg);
    if (mapIt == refcntCandidates.end()) {
        return true;
    }

    auto val = getVal(valArg, apiType, diff, sign);
    if (val.first == APIType::ERROR) {
        return true;
    }

    mapIt->second.push_back(val);
    return false;
}

void ArgTypeCallback::onStartOfTranslationUnit() {}

void ArgTypeCallback::onEndOfTranslationUnit() {}

void ArgTypeCallback::run(const MatchFinder::MatchResult& Result) {
    const clang::CallExpr *node = Result.Nodes.getNodeAs<clang::CallExpr>("argType");

    if (node == nullptr) {
        llvm::errs() << "node not matching argType!\n";
        return;
    }

    node->dumpColor();
    // Expr **args = node->getArgs();
    // unsigned int numArgs = node->getNumArgs();

    ASTContext *context = Result.Context;
    PrintingPolicy policy(context->getLangOpts());
    node->printPretty(llvm::outs(), nullptr, policy);
    llvm::outs() << "\n";

    const clang::Expr *arg = node->getArg(0);
    arg->printPretty(llvm::outs(), nullptr, policy);
    llvm::outs() << "\n";

    arg = arg->IgnoreParenImpCasts();
    arg->printPretty(llvm::outs(), nullptr, policy);
    llvm::outs() << "\n";

    while (const auto *unaryOp = dyn_cast<UnaryOperator>(arg)) {
        arg = unaryOp->getSubExpr()->IgnoreParenImpCasts();
        arg->printPretty(llvm::outs(), nullptr, policy);
        llvm::outs() << "\n";
    }

    // const auto &SM = *Result.SourceManager;
    // const auto &loc = node->getBeginLoc();
    // const auto &srcFile = SM.getFilename(SM.getSpellingLoc(loc)).str();

    // if (srcFile.empty()) {
    //     llvm::outs() << "Path empty!\n";
    //     return;
    // }

    // std::string calleeName = node->getDirectCallee()->getNameAsString();
    // bool err;

    // if (calleeName.find("init") != std::string::npos) {
    //     err = setKeyVal(SM, node, APIType::SET, APIArgType::REF_ONLY, 1, 1);
    // }
    // else if (calleeName.find("get") != std::string::npos
    //     || calleeName.find("inc") != std::string::npos) {
    //     err = setKeyVal(SM, node, APIType::DIFF, APIArgType::REF_ONLY, 1, 1);
    // }
    // else if (calleeName.find("put") != std::string::npos
    //     || calleeName.find("dec") != std::string::npos) {
    //     err = setKeyVal(SM, node, APIType::DIFF, APIArgType::REF_ONLY, 1, -1);
    // }
    // else if (calleeName.find("set") != std::string::npos) {
    //     err = setKeyVal(SM, node, APIType::SET, APIArgType::REF_VAL, 0, 1);
    // }
    // else if (calleeName.find("add_unless") != std::string::npos) {
    //     err = setKeyVal(SM, node, APIType::DIFF, APIArgType::REF_VAL, 0, 1);
    // }
    // else if (calleeName.find("add") != std::string::npos) {
    //     err = setKeyVal(SM, node, APIType::DIFF, APIArgType::VAL_REF, 0, 1);
    // }
    // else if (calleeName.find("sub") != std::string::npos) {
    //     err = setKeyVal(SM, node,APIType::DIFF,  APIArgType::VAL_REF, 0, -1);
    // }
}

/* ---------- ASTConsumer ---------- */

ArgTypeASTConsumer::ArgTypeASTConsumer(clang::Preprocessor& PP) {

    auto *callback = new ArgTypeCallback;

    Matcher.addMatcher(
        callExpr(callee(functionDecl(
            matchesName("^::(kref_|atomic_|atomic_long_|atomic64_|refcount_).*(set|add|sub|inc|dec|init|get|put).*")
        ))).bind("argType"),
        callback
    );
}

void ArgTypeASTConsumer::HandleTranslationUnit(ASTContext& Context) {
    Matcher.matchAST(Context);
}

/* ---------- ASTFrontendAction ---------- */

bool ArgTypeFrontEndAction::BeginSourceFileAction(CompilerInstance &CI) {
    return true;
}

std::unique_ptr<ASTConsumer> ArgTypeFrontEndAction::CreateASTConsumer(
    CompilerInstance& CI, StringRef file) {

return std::unique_ptr<ASTConsumer>(
        new ArgTypeASTConsumer(CI.getPreprocessor()));
}

void ArgTypeFrontEndAction::EndSourceFileAction() {}
