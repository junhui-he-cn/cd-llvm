add_llvm_component_library(LLVMCDInfo
  CDTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support
  TargetParser

  ADD_TO_COMPONENT
  CD
  )
