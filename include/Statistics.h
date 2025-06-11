#ifndef STATISTICS_H
#define STATISTICS_H

#include "llvm/Support/raw_ostream.h"

#include <set>

class GlobalStatistic {
    public:
    size_t fileNum;
    size_t fileNo;

    llvm::raw_fd_ostream *clangAnonymousStructLog;
    llvm::raw_fd_ostream *llvmAnonymousStructLog;
    llvm::raw_fd_ostream *llvmMultipleStructWithAnonLog;
    llvm::raw_fd_ostream *dupStructNameLog;
    llvm::raw_fd_ostream *fileAccessLog;
    llvm::raw_fd_ostream *compareLog;
};

class LocalStatistic {
    public:
    std::set<llvm::StringRef> clangStructNames;
    std::set<llvm::StringRef> llvmStructNames;

    std::set<std::pair<std::string, unsigned int>> anonymousSet;

    void clear() {
        // clear all members
        clangStructNames.clear();
        llvmStructNames.clear();
        anonymousSet.clear();
    }

    void print() {
        llvm::outs() << "------ Clang struct names ------\n";
        for (auto &name : clangStructNames) {
            llvm::outs() << name << "\n";
        }

        llvm::outs() << "------ LLVM struct names ------\n";
        for (auto &name : llvmStructNames) {
            llvm::outs() << name << "\n";
        }
    }

    void compare() {
        for (auto llvmIt = llvmStructNames.begin(); llvmIt != llvmStructNames.end(); ) {
            auto clangIt = clangStructNames.find(*llvmIt);
            if (clangIt != clangStructNames.end()) {
                clangStructNames.erase(clangIt);
                llvmIt = llvmStructNames.erase(llvmIt);
            }
            else {
                ++llvmIt;
            }
        }
    }
};

extern GlobalStatistic GS;
extern LocalStatistic LS;

#endif