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
        HAKCFunctionDefinition/HAKCTransferFunction.cpp

        HAKCSystem/HAKCAllocationSize.cpp
        HAKCSystem/HAKCSystemInformation.cpp
        HAKCSystem/HAKCWriter.cpp

        HAKCTransformers/HAKCTransformer.cpp

        HAKCTypeIdentifier/HAKCFunctionInfo.cpp
        HAKCTypeIdentifier/HAKCGlobalInfo.cpp
        HAKCTypeIdentifier/HAKCIndirectCallSource.cpp
        HAKCTypeIdentifier/HAKCInfo.cpp
        HAKCTypeIdentifier/HAKCSymbolInfo.cpp
        HAKCTypeIdentifier/HAKCTypeIdentifier.cpp
        HAKCTypeIdentifier/HAKCTypeInfo.cpp

        ADDITIONAL_HEADER_DIRS
        ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Compartmentalization/hakc

        ADD_TO_COMPONENT
        LLVMInstrumentation
)
