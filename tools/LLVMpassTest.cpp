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
#include "llvm/Support/Regex.h"

#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <climits>

using namespace llvm;

#define TARGET_BASE_DIR "/home/jdoh/Desktop/work/linux-v5.6/"
#define LOG_DIR "/home/jdoh/Desktop/work/Refcount-LLVM/log/linux/"

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

class ClangID {
    public:
    std::string filename;
    unsigned int line;
    std::string fieldname;

    ClangID() {}

    ClangID(const std::string &filename, unsigned int line, const std::string &fieldname)
    : filename(filename), line(line), fieldname(fieldname) {}

    void print(llvm::raw_fd_ostream &os) const {
        os << filename << ":" << line << ":" << fieldname << "\n";
    }

    std::string getOutput() const {
        std::stringstream ss;
        ss << filename << ":" << line << ":" << fieldname << "\n";
        return ss.str();
    }

    void print(llvm::raw_fd_ostream &os, const std::string funcName) const {
        os << filename << ":" << line << ":" << fieldname << ":" << funcName << "\n";
    }

    bool operator<(const ClangID &id) const {
        return std::tie(filename, line, fieldname) < std::tie(id.filename, id.line, id.fieldname);
    }

    bool operator==(const ClangID &id) const {
        return std::tie(filename, line, fieldname) == std::tie(id.filename, id.line, id.fieldname);
    }

    bool operator!=(const ClangID &id) const {
        return std::tie(filename, line, fieldname) != std::tie(id.filename, id.line, id.fieldname);
    }
};

class LLVMID {
    public:
    std::string filename;
    std::string structname;
    int64_t idx;

    LLVMID(const std::string &filename, const std::string &structname, int64_t idx)
    : filename(filename), structname(structname), idx(idx) {}

    void print(llvm::raw_fd_ostream &os) const {
        os << filename << ":" << structname << ":" << idx << "\n";
    }

    std::string getOutput() const {
        std::stringstream ss;
        ss << filename << ":" << structname << ":" << idx << "\n";
        return ss.str();
    }

    void print(llvm::raw_fd_ostream &os, const std::string funcName) const {
        os << filename << ":" << structname << ":" << idx << ":" << funcName << "\n";
    }

    bool operator<(const LLVMID &id) const {
        return std::tie(filename, structname, idx) < std::tie(id.filename, id.structname, id.idx);
    }

    bool operator==(const LLVMID &id) const {
        return std::tie(filename, structname, idx) == std::tie(id.filename, id.structname, id.idx);
    }

    bool operator!=(const LLVMID &id) const {
        return std::tie(filename, structname, idx) != std::tie(id.filename, id.structname, id.idx);
    }

    bool error() const {
        return filename == "";
    }
};

class RefcountOp {
public:
    enum Mode {
        SET,
        INC,
        DEC
    };

    enum Type {
        ATOMIC_T,
        ATOMIC_LONG_T,
        ATOMIC64_T,
        REFCOUNT_T,
        KREF
    };
    
    enum Mode mode;
    enum Type type;
    ClangID pos;
    LLVMID LID;
    int64_t val;

    RefcountOp(enum Mode mode, enum Type type, const ClangID &pos, const LLVMID &LID, int64_t val)
    : mode(mode), type(type), pos(pos), LID(LID), val(val) {}

    std::string getModeStr() const {
        switch (mode) {
        case SET:
            return "SET";
        case INC:
            return "INC";
        case DEC:
            return "DEC";
        default:
            return "UNKNOWN";
        }
    }

    uint64_t getVal() const {
        return val;
    }

    static std::string getStringType(Type type) {
        switch (type) {
        case ATOMIC_T:
            return "struct.atomic_t";
        case ATOMIC_LONG_T:
        case ATOMIC64_T:
            return "struct.atomic64_t";
        case REFCOUNT_T:
            return "struct.refcount_struct";
        case KREF:
            return "struct.kref";
        }
        return "";
    }

    void print(llvm::raw_fd_ostream &os) {
        os << pos.filename << ":" << pos.line << "\n";
        switch (mode) {
        case SET:
            os << "<SET,";
            break;
        case INC:
            os << "<INC,";
            break;
        case DEC:
            os << "<DEC,";
            break;
        default:
            os << "<UNKNOWN,";
        }
        switch (type) {
        case ATOMIC_T:
            os << "ATOMIC_T>\n";
            break;
        case ATOMIC_LONG_T:
            os << "ATOMIC_LONG_T>\n";
            break;
        case ATOMIC64_T:
            os << "ATOMIC64_T>\n";
            break;
        case REFCOUNT_T:
            os << "REFCOUNT_T>\n";
            break;
        case KREF:
            os << "KREF>\n";
            break;
        default:
            os << "UNKNOWN>\n";
        }
        os << "<key>\n";
        LID.print(os);
        os << "<value>\n";
        os << val << "\n";
    }
};

std::map<ClangID, std::vector<RefcountOp>> RefcountOpMap;
std::set<ClangID> RefcountOpSet;

std::ofstream FieldLog;
std::ofstream ValueLog;

char abspath[1000];

class Refcount : public PassInfoMixin<Refcount> {
private:
    std::map<LLVMID, ClangID> IDMap;
    std::map<std::string, std::vector<std::string>> UnionMap;
    std::string SourceFileName;
    LLVMContext *CTX;

    bool regexCompare(const std::string &regex, const std::string &target) {
        llvm::Regex R(regex);

        return R.match(target);
    }

    std::string removeDotDotDot(const std::string &filename) {
        realpath((TARGET_BASE_DIR + filename).c_str(), abspath);

        return std::string(abspath).substr(sizeof(TARGET_BASE_DIR) - 1);
    }
    
    std::string pathToLog(const std::string &filename) {
        std::string tmp = filename;

        for (char &c : tmp) {
            if (c == '/') {
                c = ' ';
            }
        }
        return tmp;
    }

    void parseIDMap(const std::string &filename) {
        // llvm::outs() << "idmappath: " << filename << "\n";
        std::ifstream ifs(filename);
        std::string line;

        std::vector<std::string> log;

        while (std::getline(ifs, line)) {
            log.push_back(line);
        }

        for (size_t i = 0; i < log.size(); i += 3) {
            size_t sep1, sep2;

            sep1 = log[i].find(':', 0);
            sep2 = log[i].find(':', sep1 + 1);
            
            // llvm::outs() << "sep1: " << sep1 << ", sep2: " << sep2 << "\n";

            // llvm::outs() << log[i].substr(0, sep1) << ":" << log[i].substr(sep1 + 1, sep2 - (sep1 + 1)) << ":" << log[i].substr(sep2 + 1) << "\n";

            LLVMID LID(
                log[i].substr(0, sep1),
                log[i].substr(sep1 + 1, sep2 - (sep1 + 1)),
                stoul(log[i].substr(sep2 + 1))
            );

            sep1 = log[i + 1].find(':', 0);
            sep2 = log[i + 1].find(':', sep1 + 1);

            // llvm::outs() << log[i + 1].substr(0, sep1) << ":" << log[i + 1].substr(sep1 + 1, sep2 - (sep1 + 1)) << ":" << log[i + 1].substr(sep2 + 1) << "\n";

            ClangID CID(
                log[i + 1].substr(0, sep1),
                stoul(log[i + 1].substr(sep1 + 1, sep2 - (sep1 + 1))),
                log[i + 1].substr(sep2 + 1)
            );

            auto ret = IDMap.insert({ LID, CID });
            if (!ret.second) {
                // ERROR
                llvm::errs() << "ERROR!\n";
            }
            // else {
            //     llvm::outs() << "------Expected------\n";
            //     LID.print(llvm::outs());
            //     CID.print(llvm::outs());
            //     llvm::outs() << "------Result------\n";
            //     ret.first->first.print(llvm::outs());
            //     ret.first->second.print(llvm::outs());
            // }
        }
    }

    void parseUnionMap(const std::string &filename) {
        // llvm::outs() << "unionpath: " << filename << "\n";
        std::ifstream ifs(filename);
        std::string line;

        std::string unionname;
        std::vector<std::string> elems;

        while (std::getline(ifs, line)) {
            if (unionname == "") {
                if (!strncmp(line.c_str(), "union", 5)) {
                    unionname = line;
                }
            }
            else if (!strncmp(line.c_str(), "    ", 4)) {
                elems.push_back(line.substr(4));
            }
            else {
                auto ret = UnionMap.insert({ unionname, elems });
                if (!ret.second) {
                    // ERROR
                    llvm::errs() << "ERROR!\n";
                }
                // else {
                //     llvm::outs() << "------Expected------\n";
                //     llvm::outs() << unionname << "\n";
                //     for (auto &elem : elems) {
                //         llvm::outs() << "    " << elem << "\n";
                //     }
                //     llvm::outs() << "\n";
                //     llvm::outs() << "------Result------\n";
                //     llvm::outs() << ret.first->first << "\n";
                //     for (auto &elem : ret.first->second) {
                //         llvm::outs() << "    " << elem << "\n";
                //     }
                //     llvm::outs() << "\n";
                // }
                unionname.clear();
                elems.clear();
            }
        }
    }

    void init(Module &M) {
        // llvm::outs() << "Module filename: " << filename << "\n";
        CTX = &M.getContext();
        SourceFileName = removeDotDotDot(M.getSourceFileName());
        std::string logpath = LOG_DIR + pathToLog(SourceFileName);
        
        parseIDMap(logpath + ".idmap");
        parseUnionMap(logpath + ".union");
    }

    void fini() {
        SourceFileName.clear();
        IDMap.clear();
        UnionMap.clear();
    }

    LLVMID __handleBitCast(Type *parentTy, Type *dstTy) {
        auto *ST = dyn_cast<StructType>(parentTy);
        if (ST == nullptr) {
            llvm::errs() << "Bitcast error\n";
            return LLVMID("", "", -1);
        }

        std::string typeName = parentTy->getStructName().str();

        if (!typeName.compare(0, 6, "union.")) {
            for (auto &elem : UnionMap[typeName]) {
                StructType *elemTy = StructType::getTypeByName(*CTX, elem);
                if (elemTy) {
                    if (dyn_cast<Type>(elemTy) == dstTy) {
                        return LLVMID(SourceFileName, typeName, 0);
                    }
                    LLVMID LID = __handleBitCast(dyn_cast<Type>(elemTy), dstTy);
                    if (!LID.error()) {
                        return LID;
                    }
                }
            }
        }

        Type *childTy = ST->elements()[0];
        if (childTy == dstTy) {
            return LLVMID(SourceFileName, typeName, 0);
        }
        else {
            return __handleBitCast(childTy, dstTy);
        }
    }

    LLVMID handleBitCast(BitCastInst *BC) {
        Type *srcTy = BC->getSrcTy()->getPointerElementType();
        Type *dstTy = BC->getDestTy()->getPointerElementType();

        if (srcTy == dstTy) {
            return LLVMID("", "", -1);
        }

        return __handleBitCast(srcTy, dstTy);
    }

    LLVMID handleGEP(GetElementPtrInst *GEP) {
        Type *rootTy = GEP->getSourceElementType();
        std::vector<Value *> indices;

        for (unsigned i = 0; i + 1 < GEP->getNumIndices(); ++i) {
            indices.push_back(GEP->getOperand(i + 1));
        }
        Type *parentTy = GetElementPtrInst::getIndexedType(rootTy, indices);

        // parentTy->print(llvm::outs());
        // llvm::outs() << "\n";

        if (auto *ST = dyn_cast<StructType>(parentTy)) {
            ConstantInt *CI = dyn_cast<ConstantInt>(GEP->getOperand(GEP->getNumIndices()));
            if (CI == nullptr) {
                llvm::errs() << "Variable index\n";
                return LLVMID("", "", -1);
            }
            return LLVMID(SourceFileName, parentTy->getStructName().str(), CI->getSExtValue());
        }
        else {
            // ERROR
            llvm::errs() << "Parent struct error\n";
            return LLVMID("", "", -1);
        }
    }

    LLVMID identifyField(Value *field, RefcountOp::Type type) {
        llvm::outs() << "identifyField() called\n";
        if (auto *BC = dyn_cast<BitCastInst>(field)) {
            return handleBitCast(BC);
        }
        else if (auto *GEP = dyn_cast<GetElementPtrInst>(field)) {
            return handleGEP(GEP);
        }
        else {
            llvm::errs() << "No bitcast and GEP error\n";
            return LLVMID("", "", -1);
        }
    }

    bool identifyValue(Value *valArg, int64_t *val) {
        // llvm::outs() << "identifyValue() called\n";
        if (auto *CI = dyn_cast<ConstantInt>(valArg)) {
            *val = CI->getSExtValue();
            return true;
        }
        llvm::errs() << "Identify Value error\n";
        return false;
    }

    void analysisInstruction(CallBase *CB) {
        auto *Callee = CB->getCalledFunction();
        if (Callee == nullptr) {
            return;
        }
        // llvm::outs() << Callee->getName() << "\n";

        std::string funcName = Callee->getName().str();
        enum RefcountOp::Type type;
        enum RefcountOp::Mode mode;
        unsigned int fieldIdx, valIdx = -1;

        // Check if it is a refcount operation
        if (!funcName.compare(0, 12, "atomic_long_")) {
            type = RefcountOp::Type::ATOMIC_LONG_T;
        }
        else if (!funcName.compare(0, 9, "atomic64_")) {
            type = RefcountOp::Type::ATOMIC64_T;
        }
        else if (!funcName.compare(0, 7, "atomic_")) {
            type = RefcountOp::Type::ATOMIC_T;
        }
        else if (!funcName.compare(0, 9, "refcount_")) {
            type = RefcountOp::Type::REFCOUNT_T;
        }
        else if (!funcName.compare(0, 5, "kref_")) {
            type = RefcountOp::Type::KREF;
        }
        else {
            return;
        }
        
        // llvm::outs() << "Refcount Operation Detected\n";
        // Handle edge cases first
        if (funcName == "atomic_inc_unless_ge") {
            fieldIdx = 1;
            mode = RefcountOp::Mode::INC;
        }
        else if (regexCompare("_add_unless", funcName)) {
            fieldIdx = 0;
            valIdx = 1;
            mode = RefcountOp::Mode::INC;
        }
        // Handle general cases
        else if (regexCompare("(_inc|_get)", funcName)) {
            fieldIdx = 0;
            mode = RefcountOp::Mode::INC;
        }
        else if (regexCompare("(_dec|_put)", funcName)) {
            fieldIdx = 0;
            mode = RefcountOp::Mode::DEC;
        }
        else if (regexCompare("_add", funcName)) {
            fieldIdx = 1;
            valIdx = 0;
            mode = RefcountOp::Mode::INC;
        }
        else if (regexCompare("_sub", funcName)) {
            fieldIdx = 1;
            valIdx = 0;
            mode = RefcountOp::Mode::DEC;
        }
        else if (regexCompare("_set", funcName)) {
            fieldIdx = 0;
            valIdx = 1;
            mode = RefcountOp::Mode::SET;
        }
        else if (regexCompare("_init", funcName)) {
            fieldIdx = 0;
            mode = RefcountOp::Mode::SET;
        }
        else {
            // No ERROR
            return;
        }

        // llvm::outs() << "Refcount Operation Type Identified\n";
        DebugLoc DL = CB->getDebugLoc();
        std::string filename;
        unsigned int line = DL.getLine();
        auto *DIL = DL.get();
        DIScope *Scope = DIL->getScope();

        DIFile *DIF;
        while (!(DIF = dyn_cast<DIFile>(Scope))) {
            Scope = Scope->getScope();
        }
        filename = removeDotDotDot(DIF->getFilename().str());
        

        // std::string filename = removeDotDotDot(DI->getFilename().str());
        // unsigned int line = DI->getLine();

        ClangID RefcountOpID(filename, line, "");
        // auto ret = RefcountOpSet.insert(RefcountOpID);
        // if (!ret.second) {
        //     return;
        // }

        /**
         * 1. Identify refcount operation based on Callee name
         * 2. Call identifyField() with field argument
         * 3. Call identifyValue() with value argument
         */
        llvm::outs() << Callee->getName() << "\n";
        llvm::outs() << "fieldIdx: " << fieldIdx << "\n";
        llvm::outs() << filename << ":" << line << "\n";
        llvm::outs() << "arg_size(): " << CB->arg_size() << "\n";
        LLVMID LID = identifyField(CB->getArgOperand(fieldIdx), type);

        int64_t val;

        if (LID.error()) {
            llvm::errs() << filename << ":" << line << "\n";
            // LOG
            if (!FieldLog.is_open()) {
                FieldLog.open(LOG_DIR "../result/field.log");
                if (!FieldLog.is_open()) {
                    llvm::errs() << "FIELD LOG OPEN FAILED\n";
                }
            }
            FieldLog << filename << ":" << line << "\n";
            return;
        }
        // LID.print(llvm::outs());
        if (valIdx != -1) {
            llvm::outs() << "valIdx: " << valIdx << "\n";
            if (!identifyValue(CB->getArgOperand(valIdx), &val)) {
                llvm::errs() << filename << ":" << line << "\n";
                // LOG
                if (!ValueLog.is_open()) {
                    ValueLog.open(LOG_DIR "../result/value.log");
                    if (!ValueLog.is_open()) {
                        llvm::errs() << "VALUE LOG OPEN FAILED\n";
                    }
                }
                ValueLog << filename << ":" << line << "\n";
                return;
            }
        
            if (val < 0) {
                if (mode == RefcountOp::Mode::INC)
                    mode = RefcountOp::Mode::DEC;
                else if (mode == RefcountOp::Mode::DEC)
                    mode = RefcountOp::Mode::INC;
                val = -(val);
            }
        }
        else {
            val = 1;
        }

        // llvm::outs() << "val: " << val << "\n";
        RefcountOpMap[IDMap[LID]].push_back({ mode, type, RefcountOpID, LID, val });
    }

public:
    PreservedAnalyses run(llvm::Module &M, ModuleAnalysisManager &MAM) {
        // llvm::outs() << "run() called\n";
        init(M);
        
        for (auto &F : M) {
            for (auto &BB : F) {
                for (auto &I : BB) {
                    if (auto *CB = dyn_cast<CallBase>(&I)) {
                        analysisInstruction(CB);
                    }
                }
            }
        }

        fini();

        return PreservedAnalyses::none();
    }

    static bool isRequired() { return true; }
};

void parsebcFiles(std::vector<std::string> &bcFiles) {
    std::ifstream ifs(TARGET_BASE_DIR "bitcodes.txt");
    if (!ifs.is_open()) {
        llvm::errs() << "PARSE FAILED!\n";
    }

    std::string line;

    while (std::getline(ifs, line)) {
        bcFiles.push_back(TARGET_BASE_DIR + line);
    }
}

int main(int Argc, char **Argv) {
    cl::HideUnrelatedOptions(CallCounterCategory);

    cl::ParseCommandLineOptions(Argc, Argv,
                              "Counts the number of static function "
                              "calls in the input IR file\n");


    SMDiagnostic Err;
    LLVMContext Ctx;

    std::vector<std::string> bcFiles;
    parsebcFiles(bcFiles);

    size_t count = 0;

    for (auto &file : bcFiles) {
        llvm_shutdown_obj SDO;
        llvm::outs() << "[" << ++count << "/" << bcFiles.size() << "]\n";
        llvm::outs() << file << "\n";
        std::unique_ptr<Module> M = parseIRFile(file, Err, Ctx);

        if (!M) {
            errs() << "Error reading bitcode file: " << InputModule << "\n";
            Err.print(Argv[0], errs());
            return -1;
        }

        ModulePassManager MPM;
        MPM.addPass(Refcount());

        ModuleAnalysisManager MAM;
        PassBuilder PB;
        PB.registerModuleAnalyses(MAM);

        MPM.run(*M, MAM);
    }

    std::ofstream totalResult(LOG_DIR "../result/total.stat");
    if (!totalResult.is_open()) {
        llvm::errs() << "result file open failed\n";
    }

    for (auto &elem : RefcountOpMap) {
        totalResult << elem.first.getOutput();
        for (auto &vecElem : elem.second) {
            totalResult << vecElem.getModeStr() << "," << vecElem.getVal() << "\n";
        }
        totalResult << "\n";
    }

    return 0;
}
