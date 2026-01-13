set(LLVM_LINK_COMPONENTS
  Analysis
  Core
  HAKC
  )

add_llvm_unittest(HAKC_UNIT_TESTS
  FakeServerClientTest.cpp
  )
