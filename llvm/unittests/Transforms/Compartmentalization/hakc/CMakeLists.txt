set(LLVM_LINK_COMPONENTS
        Analysis
        AsmParser
        Core
        Instrumentation
        Passes
        Support
)

add_llvm_unittest(HAKCUnitTests
        FakeServerClientTest.cpp
)

target_link_libraries(HAKCUnitTests PRIVATE LLVMTestingSupport)

set_property(TARGET HAKCUnitTests PROPERTY FOLDER "Tests/UnitTests/HAKCTests")
