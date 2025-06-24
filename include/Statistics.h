#ifndef STATISTICS_H
#define STATISTICS_H

#include "llvm/Support/raw_ostream.h"

#include <set>
#include <map>

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
    llvm::raw_fd_ostream *filesStructLog;
    llvm::raw_fd_ostream *filesStructIncludeLog;
};

class LocalStatistic {
    public:
    std::map<std::string, unsigned int> clangStructNames;
    std::map<std::string, unsigned int> llvmStructNames;
    std::set<std::string> dupStructNames;

    // std::set<std::pair<std::string, unsigned int>> anonymousSet;

    void clear() {
        // clear all members
        clangStructNames.clear();
        llvmStructNames.clear();
        dupStructNames.clear();
        // anonymousSet.clear();
    }

    void print() {
        llvm::outs() << "------ Clang struct names ------\n";
        for (auto &elem : clangStructNames) {
            llvm::outs() << elem.first << "\n";
        }

        llvm::outs() << "------ LLVM struct names ------\n";
        for (auto &elem : llvmStructNames) {
            llvm::outs() << elem.first << "\n";
        }
    }

    void compare() {
        for (auto llvmIt = llvmStructNames.begin(); llvmIt != llvmStructNames.end(); ) {
            auto clangIt = clangStructNames.find(llvmIt->first);
            if (clangIt != clangStructNames.end()) {
                if (clangIt->second > llvmIt->second) {
                    llvmIt = llvmStructNames.erase(llvmIt);
                }
                else if (clangIt->second < llvmIt->second) {
                    clangStructNames.erase(clangIt);
                    ++llvmIt;
                }
                else {
                    llvmIt = llvmStructNames.erase(llvmIt);
                    clangStructNames.erase(clangIt);
                }
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