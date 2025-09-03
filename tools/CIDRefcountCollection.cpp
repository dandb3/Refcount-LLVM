//========================================================================
// FILE:
//    StaticMain.cpp
//
// DESCRIPTION:
//    A command-line tool that counts all static calls (i.e. calls as seen
//    in the source code) in the input LLVM file. Internally it uses the
//    StaticCallCounter pass.
//
// USAGE:
//    # First, generate an LLVM file:
//      clang -emit-llvm <input-file> -c -o <output-llvm-file>
//    # Now you can run this tool as follows:
//      <BUILD/DIR>/bin/static <output-llvm-file>
//
// License: MIT
//========================================================================

#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <fstream>
#include <map>

using namespace llvm;

#define TARGET_DIR "/home/jdoh/Desktop/work/linux-v5.6/"
#define LOG_DIR "/home/jdoh/Desktop/work/Refcount-LLVM/build_CID/log/"

#define LOG(var) var->print(llvm::outs()); llvm::outs() << "\n";

//===----------------------------------------------------------------------===//
// Command line options
//===----------------------------------------------------------------------===//
static cl::OptionCategory CallCounterCategory{"call counter options"};

static cl::opt<std::string> InputModule{cl::Positional,
                                        cl::desc{"<Module to analyze>"},
                                        cl::value_desc{"bitcode filename"},
                                        cl::init(""),
                                        cl::Required,
                                        cl::cat{CallCounterCategory}};


class ID {
    public:
    std::string filename;
    unsigned int line;
    std::string fieldname;

    ID(const std::string &filename, unsigned int line, const std::string &fieldname)
    : filename(filename), line(line), fieldname(fieldname) {}

    void print(llvm::raw_fd_ostream &os) const {
        os << filename << ":" << line << ":" << fieldname << "\n";
    }

    void print(llvm::raw_fd_ostream &os, const std::string funcName) const {
        os << filename << ":" << line << ":" << fieldname << ":" << funcName << "\n";
    }

    bool operator<(const ID &id) const {
        return std::tie(filename, line, fieldname) < std::tie(id.filename, id.line, id.fieldname);
    }
};

typedef ID RefcntKey; // (filename, line, fieldname)
typedef std::string RefcntVal; // refcount type
typedef std::map<RefcntKey, RefcntVal> RefcntMap;

RefcntMap result;                                        
char absolutePath[1000];

void parseFile(const std::string &path, std::vector<std::string> &files) {
    std::string line;
    std::ifstream bitcodes(path);

    if (!bitcodes.is_open()) {
        llvm::errs() << "build.sh open failed\n";
        exit(1);
    }

    while (std::getline(bitcodes, line)) {
        files.push_back(TARGET_DIR + line);
        // llvm::outs() << TARGET_DIR + line << "\n";
    }
}

int main(int Argc, char **Argv) {
    cl::HideUnrelatedOptions(CallCounterCategory);

    cl::ParseCommandLineOptions(Argc, Argv,
                              "Counts the number of static function "
                              "calls in the input IR file\n");

    llvm_shutdown_obj SDO;

    std::vector<std::string> files;
    parseFile(InputModule.getValue(), files);

    int count = 0;
    for (const auto &file : files) {
        llvm::outs() << "file: " << file << "\n";
        llvm::outs() << "[" << ++count << "/" << files.size() << "]\n";
        SMDiagnostic Err;
        LLVMContext Ctx;
        std::unique_ptr<Module> M = parseIRFile(file, Err, Ctx);

        if (!M) {
            errs() << "Error reading bitcode file: " << InputModule << "\n";
            Err.print(Argv[0], errs());
            return -1;
        }

        DebugInfoFinder DIFinder;
        DIFinder.processModule(*M);
        for (auto *T : DIFinder.types()) {
            /**
             * 1. DT->getTag() == dwarf::DW_TAG_member
             * 2. name exists
             * 3. baseType is in refcount field
             *     refcount field
             *     - atomic_t: DW_TAG_typedef
             *         - z_erofs_onlinepage_t
             *         - snd_use_lock_t
             *     - atomic_long_t: DW_TAG_typedef
             *     - atomic64_t: DW_TAG_typedef
             *     - refcount_t: DW_TAG_typedef
             *     - kref: DW_TAG_structure_type
             */
            auto *DT = dyn_cast<DIDerivedType>(T);
            if (DT == nullptr)
                continue;
            if (DT->getTag() != dwarf::DW_TAG_member)
                continue;
            if (DT->getName() == "")
                continue;
            auto *BT = DT->getBaseType();
            if (BT == nullptr)
                continue;
            if (BT->getTag() == dwarf::DW_TAG_typedef) {
                if (BT->getName() != "atomic_t" && BT->getName() != "atomic_long_t"
                    && BT->getName() != "atomic64_t" && BT->getName() != "refcount_t"
                    && BT->getName() != "z_erofs_onlinepage_t"
                    && BT->getName() != "snd_use_lock_t")
                    continue;
            }
            else if (BT->getTag() == dwarf::DW_TAG_structure_type) {
                if (BT->getName() != "kref")
                    continue;
            }
            else {
                continue;
            }

            auto *DIF = DT->getFile();
            if (DIF == nullptr)
                continue;

            std::string relativePath((DIF->getDirectory().str() + "/" + DIF->getFilename().str()));
            
            std::string filename(realpath(relativePath.c_str(), absolutePath));
            unsigned int line = DT->getLine();
            std::string fieldName = DT->getName().str();

            ID key(filename, line, fieldName);
            result.insert({ key, BT->getName().str() });
        }
    }

    std::error_code ecode;
    raw_fd_ostream resultLog(llvm::StringRef(LOG_DIR "result.stat"), ecode);
    for (auto &elem : result) {
        resultLog << elem.second << "\n";
        elem.first.print(resultLog);
    }

    return 0;
}
