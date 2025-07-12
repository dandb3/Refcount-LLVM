#ifndef STATISTICS_H
#define STATISTICS_H

#include "llvm/Support/raw_ostream.h"
#include "RefcountNode.h"

class GlobalStatistic {
    public:
    size_t fileNum;
    size_t fileNo;

    llvm::raw_fd_ostream *dupStructLog;
    llvm::raw_fd_ostream *fileAccessLog;
};

class LocalStatistic {
    public:
    std::map<std::string, RefcountNode *> clangRefcountTrees;
    std::map<std::string, RefcountNode *> llvmRefcountTrees;
    std::set<std::string> dupStructNames;

    // std::set<std::pair<std::string, unsigned int>> anonymousSet;

    void clear() {
        // clear all members
        for (auto it = clangRefcountTrees.begin(); it != clangRefcountTrees.end(); ++it) {
            delete it->second;
        }
        clangRefcountTrees.clear();

        for (auto it = llvmRefcountTrees.begin(); it != llvmRefcountTrees.end(); ++it) {
            delete it->second;
        }
        llvmRefcountTrees.clear();
        dupStructNames.clear();
        // anonymousSet.clear();
    }

    void print() {
        llvm::outs() << "------ Clang struct names ------\n";
        for (auto &elem : clangRefcountTrees) {
            // llvm::outs() << "struct: " << elem.first << "\n";
            elem.second->print();
        }

        llvm::outs() << "------ LLVM struct names ------\n";
        for (auto &elem : llvmRefcountTrees) {
            // llvm::outs() << "struct: " << elem.first << "\n";
            elem.second->print();
        }
    }

    void compare() {
        // for (auto llvmIt = llvmRefcountTrees.begin(); llvmIt != llvmRefcountTrees.end(); ) {
        //     auto clangIt = clangRefcountTrees.find(llvmIt->first);
        //     if (clangIt != clangRefcountTrees.end()) {
        //         if (clangIt->second > llvmIt->second) {
        //             llvmIt = llvmRefcountTrees.erase(llvmIt);
        //         }
        //         else if (clangIt->second < llvmIt->second) {
        //             clangRefcountTrees.erase(clangIt);
        //             ++llvmIt;
        //         }
        //         else {
        //             llvmIt = llvmRefcountTrees.erase(llvmIt);
        //             clangRefcountTrees.erase(clangIt);
        //         }
        //     }
        //     else {
        //         ++llvmIt;
        //     }
        // }
    }
};

extern GlobalStatistic GS;
extern LocalStatistic LS;

#endif