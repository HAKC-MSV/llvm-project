//
// Created by al32163 on 10/23/2024
//

#ifndef HAKC_HAKCALLOCATIONSIZE_H
#define HAKC_HAKCALLOCATIONSIZE_H

#include <memory>
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

using namespace llvm;

namespace llvm::hakc {
    class HAKCAllocationSize {
    public:
        virtual ~HAKCAllocationSize() = default;

        static std::shared_ptr<HAKCAllocationSize> FromYaml(const HAKCYAMLAllocationType &YamlLine, Module &M);

        virtual ConstantInt *GetSize(CallInst *val) = 0;

        Function *GetAllocationFunction();

    protected:
        explicit HAKCAllocationSize(Function *AllocationFunction);

        HAKCAllocationSize() = default;

        Function *AllocationFunction;
    };

    class HAKCSingleArgumentSize : public HAKCAllocationSize {
        friend class HAKCAllocationSize;

    public:
        HAKCSingleArgumentSize(Function *AllocationFunction, const std::vector<std::string> &Arguments);

        ConstantInt *GetSize(CallInst *Val) override;

    protected:
        unsigned ArgNo;
    };
} // namespace hakc

#endif//HAKC_HAKCALLOCATIONSIZE_H
