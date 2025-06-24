#ifndef CLANGREFCOUNT_H
#define CLANGREFCOUNT_H

#include "Statistics.h"

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

class WarningDiagConsumer : public DiagnosticConsumer {

    public:
    virtual void HandleDiagnostic(
            DiagnosticsEngine::Level Level, const Diagnostic& Info) override {
        // simply do nothing
    }
};

class FieldTypePPCallbacks : public clang::PPCallbacks {
    virtual void InclusionDirective(SourceLocation HashLoc,
        const Token &IncludeTok,
        StringRef FileName,
        bool IsAngled,
        CharSourceRange FilenameRange,
        const FileEntry *File,
        StringRef SearchPath,
        StringRef RelativePath,
        const clang::Module *Imported,
        SrcMgr::CharacteristicKind FileType);
};

class FieldTypeCallback : public MatchFinder::MatchCallback {
    public:
    virtual void onStartOfTranslationUnit() override;
    virtual void onEndOfTranslationUnit() override;
    virtual void run(const MatchFinder::MatchResult& Result) override;
};

// ----------------------------------------------------------------------------
// REGISTERING CALLBACKS
// ----------------------------------------------------------------------------

// The ASTConsumer allows us to dictate:
//      - which AST nodes we match, and
//      - how we want to handle those matched nodes

class FieldTypeASTConsumer : public ASTConsumer {

    public:
    FieldTypeASTConsumer(clang::Preprocessor& PP);

    virtual void HandleTranslationUnit(ASTContext& Context) override;

    private:
    MatchFinder Matcher;
};

// The FrontEndAction is the main entry point for the clang tooling library
// and allows us to add callbacks via:
//      PPCallback classes  - for callbacks involving the preprocessor
//      ASTConsumer classes - for callbacks involving AST nodes 

class FieldTypeFrontEndAction : public ASTFrontendAction {
    private:
    std::string cPath;
    std::string bcPath;

    public:
    virtual bool BeginSourceFileAction(CompilerInstance &CI) override;

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
            CompilerInstance& CI, StringRef file) override;

    virtual void EndSourceFileAction() override;
};

#endif