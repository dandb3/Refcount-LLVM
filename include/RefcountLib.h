#ifndef REFCOUNTLIB_H
#define REFCOUNTLIB_H

#include "clang/Tooling/Tooling.h"
#include "llvm/IR/PassManager.h"

using namespace clang;
using namespace llvm;

class RefClangLib {
    public:
    static std::string getTypeName(const RecordDecl *node) {
        std::string name;
        auto *TND = node->getTypedefNameForAnonDecl();

        if (TND == nullptr) {
            name = node->getNameAsString();
        }
        else {
            name = TND->getNameAsString();
        }

        return name;
    }
};

class RefLLVMLib {
    private:
    static std::string getStructType(StructType *ST) {
        std::string result, tmp;
        size_t pos;

        tmp = getRawTypeName(ST);
        pos = tmp.find_first_of('.');
        if (pos == std::string::npos) {
            llvm::errs() << "ERROR: struct type\n";
            exit(1);
        }

        return tmp.substr(0, pos);
    }

    public:
    static std::string getRawTypeName(StructType *ST) {
        return ST->getName().str();
    }

    static std::string getTypeName(const std::string &name) {
        std::string result;
        size_t start, end;

        start = name.find_first_of('.');
        end = name.find('.', start + 1);
    
        if (start == std::string::npos) {
            llvm::errs() << "ERROR: struct name\n";
            exit(1);
        }

        result = name.substr(start + 1, end - (start + 1));

        if (result == "anon") {
            result.clear();
        }

        return result;
    }

    static std::string getTypeName(StructType *ST) {
        return getTypeName(getRawTypeName(ST));
    }

    static bool isStruct(StructType *ST) {
        return getStructType(ST) == "struct";
    }

    static bool isUnion(StructType *ST) {
        return getStructType(ST) == "union";
    }
};

#endif
