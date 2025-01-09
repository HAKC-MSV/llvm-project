//
// Created by de29664 on 12/5/24.
//

#ifndef HAKCWRITER_H
#define HAKCWRITER_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/IR/Value.h"

#include "llvm/Debuginfod/HTTPClient.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

using namespace llvm;

namespace llvm::hakc {
    class HAKCWriter {
    public:
        HAKCWriter();

        raw_ostream &ostream();

    protected:
        raw_ostream &os;

        void printDIType(const DIType *type, unsigned indents);

    public:
        HAKCWriter &operator<<(llvm::Value *V);

        HAKCWriter &operator<<(llvm::Value &V);

        HAKCWriter &operator<<(StringRef str);

        HAKCWriter &operator<<(unsigned int i);

        HAKCWriter &operator<<(unsigned long i);

        HAKCWriter &operator<<(bool b);

        HAKCWriter &operator<<(std::string str);

        HAKCWriter &operator<<(const char *s);

        HAKCWriter &operator<<(Function &F);

        HAKCWriter &operator<<(Module &M);

        HAKCWriter &operator<<(Module *M);

        HAKCWriter &operator<<(Type *Ty);

        HAKCWriter &operator<<(Type &Ty);

        HAKCWriter &operator<<(const DINode *DiNode);

        HAKCWriter &operator<<(const DINode &DiNode);

        HAKCWriter &operator<<(const DIType *DiType);

        HAKCWriter &operator<<(const hakc::HAKCCompartment &Compartment);

        HAKCWriter &operator<<(hakc::HAKCCompartmentDivision &Division);

        HAKCWriter &operator<<(hakc::HAKCTypeInfo &TypeInfo);

        HAKCWriter &operator<<(enum HAKCAllocationTypeEnum AllocationType);

        HAKCWriter &operator<<(const ManagedHAKCPointerUse &HAKCPointerUse);

        HAKCWriter &operator<<(const HAKCPointerBase &ManagedPointer);

        HAKCWriter &operator<<(const HAKCPointerBaseP &ManagedPointer);

        HAKCWriter &operator<<(HAKCFunctionInfo &HAKCFuncInfo);

        HAKCWriter &operator<<(HTTPRequest &HTTPRequest);
    };
} // hakc

#endif //HAKCWRITER_H
