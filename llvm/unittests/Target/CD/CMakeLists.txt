include_directories(
  ${LLVM_MAIN_SRC_DIR}/lib/Target/CD
  ${LLVM_BINARY_DIR}/lib/Target/CD
  )

set(LLVM_LINK_COMPONENTS
  CDCodeGen
  Support
  )

add_llvm_target_unittest(CDTests
  CDBytecodeFormatTest.cpp
  )
