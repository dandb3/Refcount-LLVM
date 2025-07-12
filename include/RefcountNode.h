#ifndef REFCOUNTNODE_H
#define REFCOUNTNODE_H

#include "RefcountLib.h"

using namespace clang;
using namespace llvm;

class RefcountType {
    public:
    enum NodeType {
        REF_ATOMIC_T,
        REF_ATOMIC_LONG_T,
        REF_ATOMIC64_T,
        REF_REFCOUNT_STRUCT,
        REF_KREF,
        REF_STRUCT,
        REF_UNION,
        REF_DUMMY,
        REF_HASREF // tmp type; only used while making tree
    };

    static NodeType stringToType(const std::string &name) {
        if (name == "atomic_t") {
            return REF_ATOMIC_T;
        }
        else if (name == "atomic_long_t") {
            return REF_ATOMIC_LONG_T;
        }
        else if (name == "atomic64_t") {
            return REF_ATOMIC64_T;
        }
        else if (name == "refcount_struct") {
            return REF_REFCOUNT_STRUCT;
        }
        else if (name == "kref") {
            return REF_KREF;
        }
        else {
            return REF_DUMMY;
        }
    }

    static bool isRefcountType(NodeType type) {
        switch (type) {
        case REF_ATOMIC_T:
        case REF_ATOMIC_LONG_T:
        case REF_ATOMIC64_T:
        case REF_REFCOUNT_STRUCT:
        case REF_KREF:
            return true;
        default:
            return false;
        }
    }

    static bool isRefStructOrUnionType(NodeType type) {
        switch (type) {
        case REF_STRUCT:
        case REF_UNION:
            return true;
        default:
            return false;
        }
    }

    static std::string typeToString(NodeType type) {
        switch (type) {
            case REF_ATOMIC_T:
                return "atomic_t";
            case REF_ATOMIC_LONG_T:
                return "atomic_long_t";
            case REF_ATOMIC64_T:
                return "atomic64_t";
            case REF_REFCOUNT_STRUCT:
                return "refcount_struct";
            case REF_KREF:
                return "kref";
            default:
                return "";
        }
    }
};

class RefcountNode {
    private:
    std::vector<RefcountNode *> children;
    RefcountType::NodeType type;
    std::string typeName;
    
    /**
     * child for comparing union
     * only used in Clang union
     */
    RefcountNode *representative;
    
    // called in LLVM pass makeTree()
    static RefcountNode *getOrMakeTree(StructType *ST, std::set<StructType *> &structSet,
        std::map<std::string, std::pair<RefcountNode *, bool>> &cache) {
        RefcountNode *node;
        auto it = structSet.find(ST);
        std::string rawTypeName = RefLLVMLib::getRawTypeName(ST);
    
        if (it == structSet.end()) {
            auto &elem = cache[rawTypeName];
            elem.second = true;
            return elem.first->clone();
        }
        
        structSet.erase(it);
        node = makeTree(ST, structSet, cache);
        if (node == nullptr) {
            return nullptr;
        }
    
        cache[rawTypeName] = { node, true };
        // 집어넣을 때 이미 존재하면 에러처리?
        return node->clone();
    }

    // called in print()
    void printIndent(std::string indent) {
        std::string newIndent = indent + "    ";
        llvm::outs() << indent;
        if (type == RefcountType::REF_STRUCT) {
            llvm::outs() << "struct ";
        }
        else if (type == RefcountType::REF_UNION) {
            llvm::outs() << "union ";
        }
        llvm::outs() << typeName << " {\n";
        for (auto *child : children) {
            if (RefcountType::isRefcountType(child->type)) {
                llvm::outs() << newIndent << RefcountType::typeToString(child->type) << "\n";
            }
            else if (RefcountType::isRefStructOrUnionType(child->type)) {
                child->printIndent(newIndent);
            }
            else {
                llvm::outs() << newIndent << "dummy " << child->typeName << "\n";
            }
        }
        llvm::outs() << indent << "}\n";
    }

    RefcountNode *clone() {
        RefcountNode *root, *newChild;

        root = new RefcountNode(type, typeName);
        if (root == nullptr) {
            return nullptr;
        }

        for (RefcountNode *child : children) {
            newChild = child->clone();
            if (newChild == nullptr) {
                delete root;
                return nullptr;
            }

            root->children.push_back(newChild);
        }

        return root;
    }

    void add(RefcountNode *node) {
        children.push_back(node);
    }

    public:
    RefcountNode(RefcountType::NodeType type)
    : type(type) {}
    RefcountNode(RefcountType::NodeType type, const std::string &typeName)
    : type(type), typeName(typeName) {}
    ~RefcountNode() {
        for (RefcountNode *child : children) {
            delete child;
        }
    }

    bool hasRefcountField() {
        // RefcountType::isRefcountType(type) ??
        return RefcountType::isRefStructOrUnionType(type);
    }

    const std::string &getTypeName() {
        return typeName;
    }

    void print() {
        printIndent("");
    }

    /**
     * makeTree() : make tree for struct
     * 
     * returns nullptr if allocation fails
     * 
     * child type
     * - refcount - atomic_t, atomic_long_t, atomic64_t, refcount_struct, kref
     * - anonymous struct containing refcounts
     * - anonymous union containing refcounts
     * - dummy - named struct / union
     *         - anonymous struct / union not containing refcounts
     *         - non-refcount fields
     * 
     * ??have to check whether returned RefcountNode contains refcount fields
     */

    // used in clang libtooling
    static RefcountNode *makeTree(const RecordDecl *node) {
        RefcountType::NodeType type;
        RefcountNode *parent, *child;
        std::string typeName;
        const RecordDecl *RD;
        const RecordType *RT;
    
        parent = new RefcountNode(RefcountType::REF_DUMMY, RefClangLib::getTypeName(node));
        if (parent == nullptr) {
            return nullptr;
        }
        
        for (FieldDecl *FD : node->fields()) {
            RT = FD->getType().getCanonicalType()->getAs<RecordType>();
            if (RT == nullptr) {
                child = new RefcountNode(RefcountType::REF_DUMMY);
                if (child == nullptr) {
                    delete parent;
                    return nullptr;
                }
                parent->add(child);
                continue;
            }
    
            RD = RT->getDecl();
            typeName = RefClangLib::getTypeName(RD);
            type = RefcountType::stringToType(typeName);
            if (RefcountType::isRefcountType(type)) {
                child = new RefcountNode(type, typeName);
                if (child == nullptr) {
                    delete parent;
                    return nullptr;
                }
                parent->add(child);
                parent->type = RefcountType::REF_HASREF;
            }
            else if (typeName == "") {
                child = makeTree(RD);
                if (child == nullptr) {
                    delete parent;
                    return nullptr;
                }
                parent->add(child);
                if (child->hasRefcountField()) {
                    parent->type = RefcountType::REF_HASREF;
                }
            }
            else {
                child = new RefcountNode(RefcountType::REF_DUMMY, typeName);
                if (child == nullptr) {
                    delete parent;
                    return nullptr;
                }
                parent->add(child);
            }
        }
    
        if (parent->type == RefcountType::REF_HASREF) {
            if (node->isStruct()) {
                parent->type = RefcountType::REF_STRUCT;
            }
            else if (node->isUnion()) {
                parent->type = RefcountType::REF_UNION;

                
            }
            else {
                // ERROR!
            }
        }
    
        return parent;
    }

    // used in LLVM pass
    static RefcountNode *makeTree(StructType *ST, std::set<StructType *> &structSet,
        std::map<std::string, std::pair<RefcountNode *, bool>> &cache) {
        RefcountNode *parent, *child;
        StructType *fieldST;
        RefcountType::NodeType type;
        std::string typeName;
    
        llvm::outs() << "makeTree() start - Name: " << RefLLVMLib::getRawTypeName(ST) << "\n";
        parent = new RefcountNode(RefcountType::REF_DUMMY, RefLLVMLib::getTypeName(ST));
        if (parent == nullptr) {
            return nullptr;
        }
    
        for (llvm::Type *fieldTy : ST->elements()) {
            fieldST = dyn_cast<StructType>(fieldTy);
            if (fieldST == nullptr) {
                child = new RefcountNode(RefcountType::REF_DUMMY);
                if (child == nullptr) {
                    delete parent;
                    return nullptr;
                }
                parent->add(child);
                continue;
            }
    
            typeName = RefLLVMLib::getTypeName(fieldST);
            type = RefcountType::stringToType(typeName);
    
            llvm::outs() << "    Name: " << RefLLVMLib::getRawTypeName(fieldST) << "\n";

            if (RefcountType::isRefcountType(type)) {
                llvm::outs() << "is Refcount Type\n";
                child = new RefcountNode(type, typeName);
                if (child == nullptr) {
                    delete parent;
                    return nullptr;
                }
    
                parent->add(child);
                parent->type = RefcountType::REF_HASREF;
            }
            else if (typeName == "") {
                llvm::outs() << "is anonymous\n";
                /**
                 * if child is in cache:
                 *     get cloned child
                 * if child is not in cache:
                 *     create new child and put it into cache
                 *     get cloned child
                 */
                child = getOrMakeTree(fieldST, structSet, cache);
                if (child == nullptr) {
                    delete parent;
                    return nullptr;
                }
                parent->add(child);
                if (child->hasRefcountField()) {
                    parent->type = RefcountType::REF_HASREF;
                }
            }
            else {
                llvm::outs() << "is dummy\n";

                child = new RefcountNode(RefcountType::REF_DUMMY, typeName);
                if (child == nullptr) {
                    delete parent;
                    return nullptr;
                }
                parent->add(child);
            }
        }

        if (parent->type == RefcountType::REF_HASREF) {
            if (RefLLVMLib::isStruct(ST)) {
                parent->type = RefcountType::REF_STRUCT;
            }
            else if (RefLLVMLib::isUnion(ST)) {
                parent->type = RefcountType::REF_UNION;
            }
            else {
                // ERROR!
            }
        }
    
        return parent;
    }
};

#endif
