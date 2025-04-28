#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/JSON.h"

#include <cstdlib>

using namespace llvm;

#include <vector>
#include <fstream>

#define BUILD_DIR "/home/jdoh/Desktop/linux-v6.6/"
#define TEST_DIR "/home/jdoh/Desktop/test/"
#define BUILD_LOG BUILD_DIR "compile_commands.json"
#define CMDS_PATH TEST_DIR "command.sh"

#define ERROR(msg) { errs() << "ERROR: " msg "\n"; exit(1); }

struct Command {
    std::vector<std::string> args;
    std::string dir;
    std::string file;
};

namespace llvm {
    namespace json {
        bool fromJSON(const Value &V, Command &C, Path P) {
            ObjectMapper O(V, P);
        
            return O && O.map("arguments", C.args) && O.map("directory", C.dir)
                    && O.map("file", C.file);
        }        
    }
}

static void parseJSON(const json::Value &V, Command &C) {
    json::Path::Root root;
    json::Path P(root);

    json::ObjectMapper OM(V, P);
    if (!OM || !OM.map("arguments", C.args) || !OM.map("directory", C.dir)
    || !OM.map("file", C.file)) {
        ERROR("JSON parse");
    }
}

static void prepareCMD(Command &C, std::vector<std::string> &Objs, std::ofstream &Cmds) {
    for (auto it = C.args.begin(); it != C.args.end(); ++it) {
        if (it->size() == 2 && !strncmp(it->c_str(), "-o", 2)) {
            ++it;
            Objs.push_back(C.dir + "/" + *it);
        }
    }
    // C.args.push_back("-emit-llvm");
    // C.args.push_back("-S");

    Cmds << "chdir " << C.dir << ';';
    for (std::string &arg : C.args) {
        Cmds << ' ' << arg;
    }
    Cmds << '\n';
}

static void executeCMD() {
    system("chmod 755 " CMDS_PATH "; " CMDS_PATH);
}

int main(int argc, char **argv) {
    llvm_shutdown_obj SDO;

    SMDiagnostic Err;
    LLVMContext Ctx;

    auto buffer = MemoryBuffer::getFile(BUILD_LOG);
    if (!buffer) {
        ERROR("reading json file");
    }

    auto jsonParsed = json::parse(buffer->get()->getBuffer());
    if (!jsonParsed) {
        ERROR("parsing json file");
    }
    
    json::Array *arr = jsonParsed->getAsArray();
    if (!arr) {
        ERROR("json format invalid");
    }

    std::ofstream cmds(CMDS_PATH, std::ios::trunc | std::ios::out);
    std::vector<std::string> objs;
    Command cmd;

    if (!cmds.is_open()) {
        ERROR("cmds creation failed");
    }
    cmds << "#!/bin/sh\n";

    for (const json::Value &entry : *arr) {
        parseJSON(entry, cmd);
        prepareCMD(cmd, objs, cmds);
    }

    cmds.close();
    executeCMD();

    return 0;
}