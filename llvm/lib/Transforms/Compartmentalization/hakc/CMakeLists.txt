add_llvm_component_library(LLVMHAKC
        HAKCPass.cpp
        HAKCAnalysis/CommonHAKCAnalysis.cpp
        HAKCAnalysis/HAKCFunctionAnalysis.cpp
        HAKCAnalysis/HAKCModuleAnalysis.cpp
        HAKCAnalysis/HAKCPointerManager.cpp
        HAKCAnalysis/ManagedHAKCPointer.cpp

        HAKCCompartmentalizationPolicy/HAKCCompartment.cpp
        HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.cpp
        HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.cpp

        HAKCFunctionDefinition/HAKCCustomTransfer.cpp
        HAKCFunctionDefinition/HAKCFunctionDefinition.cpp

        HAKCSystem/HAKCAllocationSize.cpp
        HAKCSystem/HAKCSystemInformation.cpp
        HAKCSystem/HAKCWriter.cpp
        HAKCSystem/HAKCLogger.cpp

        HAKCTransformers/HAKCPostTargetAction.cpp
        HAKCTransformers/HAKCPreTransferAction.cpp
        HAKCTransformers/HAKCTransferAction.cpp
        HAKCTransformers/HAKCTransformer.cpp
        HAKCTransformers/HAKCTransferState.cpp

        HAKCTypeIdentifier/HAKCFunctionInfo.cpp
        HAKCTypeIdentifier/HAKCGlobalInfo.cpp
        HAKCTypeIdentifier/HAKCIndirectCallSource.cpp
        HAKCTypeIdentifier/HAKCInfo.cpp
        HAKCTypeIdentifier/HAKCSymbolInfo.cpp
        HAKCTypeIdentifier/HAKCTypeIdentifier.cpp
        HAKCTypeIdentifier/HAKCTypeInfo.cpp

        ADDITIONAL_HEADER_DIRS
        ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Compartmentalization/hakc

        DEPENDS
        intrinsics_gen

        ADD_TO_COMPONENT
        LLVMInstrumentation
)
