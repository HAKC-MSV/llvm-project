//
// Created by al32163 on 10/23/2024
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCAllocationSize.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

namespace llvm::hakc {
    ConstantInt *HAKCSingleArgumentSize::GetSize(CallInst *Val) {
        IRBuilder<> irBuilder(Val);
        Value *size = Val->getArgOperand(ArgNo);
        size = irBuilder.CreateZExtOrBitCast(size, irBuilder.getInt64Ty());
        auto *CI = dyn_cast<ConstantInt>(size);
        return CI;
    }

    HAKCSingleArgumentSize::HAKCSingleArgumentSize(Function *AllocationFunction,
                                                   const std::vector<std::string> &Arguments)
        : HAKCAllocationSize(AllocationFunction), ArgNo(0) {
        if (Arguments.empty()) {
            CommonHAKCAnalysis::getWriter(true) << "Invalid Arguments\n";
            throw std::exception();
        }

        StringRef Arg(Arguments[0]);
        Arg.getAsInteger(10, ArgNo);
    }

    HAKCAllocationSize::HAKCAllocationSize(Function *AllocationFunction) : AllocationFunction(AllocationFunction) {
    }


    std::shared_ptr<HAKCAllocationSize>
    HAKCAllocationSize::FromYaml(const hakc::HAKCYAMLAllocationType &YamlAllocation, Module &M) {
        auto *F = M.getFunction(YamlAllocation.FunctionName);
        if (!F) {
            return nullptr;
        }

        switch (YamlAllocation.AllocationType) {
            default:
                CommonHAKCAnalysis::getWriter(true) << "HAKCAllocation Type " << YamlAllocation.AllocationType
                        << " is not supported\n";
                throw std::exception();
            case hakc::SimpleArgumentSize:
                return std::make_shared<HAKCSingleArgumentSize>(F, YamlAllocation.Arguments);
        }
    }

    Function *HAKCAllocationSize::GetAllocationFunction() {
        return AllocationFunction;
    }
} // namespace llvm::hakc
